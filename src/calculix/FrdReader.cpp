#include "calculix/FrdReader.h"

#include <QFile>
#include <QtEndian>

#include <algorithm>

// Column positions are 0-based offsets from the Fortran formats in the cgx
// manual's "Result Format" chapter, long format (FORMAT 1):
//   2C   (1X,'   2','C',18X,I12,37X,I1)        count 24..35, format 73
//   3C   (1X,'   3','C',18X,I12,37X,I1)        count 24..35, format 73
//   -1   node  (1X,'-1',I10,3E12.5)            id 3..12, x y z from 13
//   -1   elem  (1X,'-1',I10,3I5)               id 3..12, type, group, material
//   -2   elem  (1X,'-2',10I10)                 node ids from 3, 10 a line
//   100C (1X,' 100','C',6A1,E12.5,I12,20A1,I2,I5,10A1,I2)
//                                              value 12..23, count 24..35,
//                                              type 56..57, step 58..62, format 73..74
//   -4   (1X,I2,2X,8A1,2I5)                    name 5..12, ncomps 13..17, irtype 18..22
//   -5   (1X,I2,2X,8A1,5I5,8A1)                name 5..12, menu, ictype, ind1, ind2, iexist
//   -1   data (1X,I2,I10,6E12.5)               id 3..12, values from 13, 6 a line
// Values sit in fixed columns: "0.00000E+00-3.00000E-03" is two numbers.
//
// Binary (read byte by byte from ccx 2.23's *NODE OUTPUT file, fixture
// bar-binary): the headers stay ASCII lines; the records under a block
// header are little-endian binary with no -3 after them.
//   nodes, format 3     int32 id, float64 x y z
//   elements, format 2  int32 id, type, group, material, node ids
//   results, format 2   int32 node id, float32 per supplied component

namespace gf::calculix {

int FrdDataset::suppliedCount() const
{
    return int(std::count_if(components.begin(), components.end(),
                             [](const FrdComponent &c) { return c.supplied; }));
}

bool FrdDataset::valueAt(int nodeId, const QString &component, double *out) const
{
    int column = -1, k = 0;
    for (const FrdComponent &c : components) {
        if (!c.supplied)
            continue;
        if (c.name == component) {
            column = k;
            break;
        }
        ++k;
    }
    const auto it = std::find(nodeIds.begin(), nodeIds.end(), nodeId);
    if (column < 0 || it == nodeIds.end())
        return false;
    *out = values[size_t(it - nodeIds.begin()) * size_t(suppliedCount()) + size_t(column)];
    return true;
}

const FrdNode *FrdFile::node(int id) const
{
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [id](const FrdNode &n) { return n.id == id; });
    return it == nodes.end() ? nullptr : &*it;
}

const FrdDataset *FrdFile::dataset(const QString &name, int occurrence) const
{
    for (const FrdDataset &d : datasets)
        if (d.name == name && occurrence-- == 0)
            return &d;
    return nullptr;
}

namespace {

// Node counts for the element types the manual confirms.
int nodesPerElement(int type)
{
    switch (type) {
    case 1: return 8;   // he8
    case 2: return 6;   // pe6
    case 3: return 4;   // te4
    case 4: return 20;  // he20 (ccx's C3D20 and C3D20R, fixture cube-he20)
    case 5: return 15;  // pe15
    case 6: return 10;  // te10 (ccx's C3D10, fixture cantilever-te10)
    case 7: return 3;   // tr3 (S3, CPS3 with OUTPUT=2D; fixture shell-s3)
    case 8: return 6;   // tr6 (S6; shell-s6)
    case 9: return 4;   // qu4 (S4, CPS4; shell-s4, plane-cps4)
    case 10: return 8;  // qu8 (S8; shell-s8)
    case 11: return 2;  // be2 (B31; beam-b31)
    case 12: return 3;  // be3 (B32R, end, end, middle; beam-b32r)
    default: return 0;
    }
}

class Reader {
public:
    explicit Reader(QByteArray data) : data_(std::move(data)) {}

    FrdFile read()
    {
        while (f_.ok() && next()) {
            const QString &l = line_;
            if (l.startsWith("    1C"))
                continue;  // model header: nothing in it the results need
            if (l.startsWith("    1U"))
                f_.userHeader.append(l.mid(6).trimmed());
            else if (l.startsWith("    1P"))
                parameters_.append(l.mid(6).trimmed());
            else if (l.startsWith("    2C"))
                readNodes();
            else if (l.startsWith("    3C"))
                readElements();
            else if (l.startsWith("  100C"))
                readDataset();
            else if (l.startsWith(" 9999"))
                return f_;
            else
                fail("not a record this reader knows: \"" + l.left(12) + "\"");
        }
        if (f_.ok())
            fail("the file ends without its end record (9999)");
        return f_;
    }

private:
    // The next ASCII line, without its line end.
    bool next()
    {
        if (pos_ >= data_.size())
            return false;
        qsizetype end = data_.indexOf('\n', pos_);
        if (end < 0)
            end = data_.size();
        line_ = QString::fromLatin1(data_.mid(pos_, end - pos_));
        if (line_.endsWith('\r'))
            line_.chop(1);
        pos_ = end + 1;
        ++lineNo_;
        recordAt_ = -1;
        return true;
    }
    // The next binary record of `size` bytes; null if the file ends first.
    const char *record(qsizetype size, const QString &where)
    {
        recordAt_ = pos_;
        if (data_.size() - pos_ < size) {
            fail("the file ends inside " + where);
            return nullptr;
        }
        const char *p = data_.constData() + pos_;
        pos_ += size;
        return p;
    }
    static int int32(const char *p) { return qFromLittleEndian<qint32>(p); }
    static double float64(const char *p) { return qFromLittleEndian<double>(p); }
    static double float32(const char *p) { return qFromLittleEndian<float>(p); }
    bool nextOr(const QString &where)
    {
        if (next())
            return true;
        fail("the file ends inside " + where);
        return false;
    }
    void fail(const QString &what)
    {
        if (f_.ok())
            f_.error = recordAt_ < 0 ? QString("line %1: %2").arg(lineNo_).arg(what)
                                     : QString("byte %1: %2").arg(recordAt_).arg(what);
    }
    QString field(int from, int width) const { return line_.mid(from, width).trimmed(); }
    bool integer(int from, int width, int *out, bool blankIsZero = false)
    {
        const QString s = field(from, width);
        if (s.isEmpty() && blankIsZero) {
            *out = 0;
            return true;
        }
        bool ok = false;
        *out = s.toInt(&ok);
        if (!ok)
            fail("expected a whole number in columns " + QString::number(from + 1) + " to "
                 + QString::number(from + width) + ", found \"" + s + "\"");
        return ok;
    }
    bool real(int from, int width, double *out)
    {
        const QString s = field(from, width);
        bool ok = false;
        *out = s.toDouble(&ok);
        if (!ok)
            fail("expected a number in columns " + QString::number(from + 1) + " to "
                 + QString::number(from + width) + ", found \"" + s + "\"");
        return ok;
    }
    bool key(const char *k)
    {
        if (line_.mid(1, 2) == k)
            return true;
        fail(QString("expected a %1 record").arg(k));
        return false;
    }
    // The block header's format: 1 (long ASCII), or the one binary format
    // ccx writes for this block. Anything else is refused by name.
    bool format(int binary, const char *block, int *out)
    {
        if (!integer(73, 2, out))
            return false;
        if (*out == 1 || *out == binary)
            return true;
        fail(*out == 0 ? QString("%1 in the short ASCII format (0) are not read yet").arg(block)
                       : QString("%1 in format %2 are not read yet (1 and %3 are)")
                             .arg(block).arg(*out).arg(binary));
        return false;
    }
    bool blockEnd(const QString &where)
    {
        return nextOr(where) && key("-3");
    }

    void readNodes()
    {
        int count = 0, fmt = 0;
        if (!integer(24, 12, &count) || !format(3, "nodes", &fmt))
            return;
        for (int i = 0; i < count; ++i) {
            const QString where = QString("the node block (%1 of %2 nodes)").arg(i).arg(count);
            FrdNode n;
            if (fmt == 3) {
                const char *p = record(28, where);
                if (!p)
                    return;
                n = {int32(p), float64(p + 4), float64(p + 12), float64(p + 20)};
                f_.nodes.push_back(n);
                continue;
            }
            if (!nextOr(where) || !key("-1"))
                return;
            if (!integer(3, 10, &n.id) || !real(13, 12, &n.x) || !real(25, 12, &n.y)
                || !real(37, 12, &n.z))
                return;
            f_.nodes.push_back(n);
        }
        if (fmt == 1)
            blockEnd("the node block");
    }

    void readElements()
    {
        int count = 0, fmt = 0;
        if (!integer(24, 12, &count) || !format(2, "elements", &fmt))
            return;
        for (int i = 0; i < count; ++i) {
            const QString where = QString("the element block (%1 of %2 elements)").arg(i).arg(count);
            FrdElement e;
            int type = 0;
            if (fmt == 2) {
                const char *p = record(16, where);
                if (!p)
                    return;
                e.id = int32(p);
                type = int32(p + 4);
                e.group = int32(p + 8);
                e.material = int32(p + 12);
            } else if (!nextOr(where) || !key("-1") || !integer(3, 10, &e.id)
                       || !integer(13, 5, &type) || !integer(18, 5, &e.group)
                       || !integer(23, 5, &e.material)) {
                return;
            }
            const int want = nodesPerElement(type);
            if (want == 0) {
                fail(QString("element %1 has type %2, which is no element type this reader "
                             "knows").arg(e.id).arg(type));
                return;
            }
            e.type = FrdElementType(type);
            if (fmt == 2) {
                const char *p = record(4 * want, where);
                if (!p)
                    return;
                for (int k = 0; k < want; ++k)
                    e.nodes.push_back(int32(p + 4 * k));
                f_.elements.push_back(std::move(e));
                continue;
            }
            while (int(e.nodes.size()) < want) {
                if (!nextOr(where) || !key("-2"))
                    return;
                for (int k = 0; k < 10 && int(e.nodes.size()) < want; ++k) {
                    int id = 0;
                    if (!integer(3 + 10 * k, 10, &id))
                        return;
                    e.nodes.push_back(id);
                }
            }
            f_.elements.push_back(std::move(e));
        }
        if (fmt == 1)
            blockEnd("the element block");
    }

    void readDataset()
    {
        FrdDataset d;
        int count = 0, fmt = 0;
        if (!real(12, 12, &d.value) || !integer(24, 12, &count)
            || !integer(56, 2, &d.analysisType, true) || !integer(58, 5, &d.step)
            || !format(2, "results", &fmt))
            return;
        d.parameters = parameters_;
        parameters_.clear();

        if (!nextOr("a results block") || !key("-4"))
            return;
        d.name = field(5, 8);
        int ncomps = 0, irtype = 0;
        if (!integer(13, 5, &ncomps) || !integer(18, 5, &irtype))
            return;
        if (irtype != 1) {
            fail(QString("%1: material-dependent results (type %2) are not read yet")
                     .arg(d.name).arg(irtype));
            return;
        }
        for (int i = 0; i < ncomps; ++i) {
            if (!nextOr("the components of " + d.name) || !key("-5"))
                return;
            FrdComponent c;
            c.name = field(5, 8);
            int menu = 0, exist = 0;
            if (!integer(13, 5, &menu) || !integer(18, 5, &c.kind)
                || !integer(23, 5, &c.index1, true) || !integer(28, 5, &c.index2, true)
                || !integer(33, 5, &exist, true))
                return;
            c.supplied = exist != 1;
            d.components.push_back(c);
        }
        const int per = d.suppliedCount();
        if (fmt == 2) {
            for (int i = 0; i < count; ++i) {
                const char *p = record(4 + 4 * per, QString("%1 (%2 of %3 nodes)")
                                                         .arg(d.name).arg(i).arg(count));
                if (!p)
                    return;
                d.nodeIds.push_back(int32(p));
                for (int k = 0; k < per; ++k)
                    d.values.push_back(float32(p + 4 + 4 * k));
            }
            f_.datasets.push_back(std::move(d));
            return;
        }
        if (per > 6) {
            fail(QString("%1: more than six values per node are not read yet").arg(d.name));
            return;
        }
        for (int i = 0; i < count; ++i) {
            if (!nextOr(QString("%1 (%2 of %3 nodes)").arg(d.name).arg(i).arg(count))
                || !key("-1"))
                return;
            int id = 0;
            if (!integer(3, 10, &id))
                return;
            d.nodeIds.push_back(id);
            for (int k = 0; k < per; ++k) {
                double v = 0;
                if (!real(13 + 12 * k, 12, &v))
                    return;
                d.values.push_back(v);
            }
        }
        if (!blockEnd(d.name))
            return;
        f_.datasets.push_back(std::move(d));
    }

    QByteArray data_;
    qsizetype pos_ = 0;        // the next byte to read
    int lineNo_ = 0;           // ASCII lines read so far; the current line's number
    qsizetype recordAt_ = -1;  // where the current binary record starts; -1 on a line
    QString line_;
    QStringList parameters_;
    FrdFile f_;
};

} // namespace

FrdFile readFrd(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        FrdFile f;
        f.error = QString("cannot open %1: %2").arg(path, file.errorString());
        return f;
    }
    return Reader(file.readAll()).read();
}

} // namespace gf::calculix
