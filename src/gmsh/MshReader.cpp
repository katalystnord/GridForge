#include "gmsh/MshReader.h"

#include <QFile>

#include <cctype>
#include <cstring>
#include <map>

namespace gf::gmsh {

namespace {

// The names the Gmsh manual gives the element types, for messages.
const std::map<int, const char *> &typeNames()
{
    static const std::map<int, const char *> names = {
        {1, "2-node line"},
        {2, "3-node triangle"},
        {3, "4-node quadrangle"},
        {4, "4-node tetrahedron"},
        {5, "8-node hexahedron"},
        {6, "6-node prism"},
        {7, "5-node pyramid"},
        {8, "3-node second order line"},
        {9, "6-node second order triangle"},
        {10, "9-node second order quadrangle"},
        {11, "10-node second order tetrahedron"},
        {12, "27-node second order hexahedron"},
        {13, "18-node second order prism"},
        {14, "14-node second order pyramid"},
        {15, "1-node point"},
        {16, "8-node second order quadrangle"},
        {17, "20-node second order hexahedron"},
        {18, "15-node second order prism"},
        {19, "13-node second order pyramid"},
        {20, "9-node third order incomplete triangle"},
        {21, "10-node third order triangle"},
        {26, "4-node third order edge"},
        {29, "20-node third order tetrahedron"},
    };
    return names;
}

// The types read so far: nodes per element and dimension.
struct TypeInfo {
    int nodes, dimension;
};
const std::map<int, TypeInfo> &readTypes()
{
    static const std::map<int, TypeInfo> types = {
        {1, {2, 1}},  {2, {3, 2}},   {3, {4, 2}},   {4, {4, 3}},   {5, {8, 3}},
        {6, {6, 3}},  {8, {3, 1}},   {9, {6, 2}},   {11, {10, 3}}, {15, {1, 0}},
        {16, {8, 2}}, {17, {20, 3}}, {18, {15, 3}},
    };
    return types;
}

// Walks the file: text a line or a word at a time, and, inside the
// sections a binary file writes in binary, values as raw bytes.
class Cursor
{
public:
    explicit Cursor(const QByteArray &data) : d(data) {}

    bool binary = false;   // the file's mode
    bool raw = false;      // inside a section written in binary
    int sizeBytes = 8;     // the file's size_t
    QString section;       // for "the file ends inside ..."
    QString error;

    bool atEnd() const { return pos >= d.size(); }

    // The next line, without its end; false at the end of the file.
    bool line(QByteArray *out)
    {
        if (atEnd())
            return false;
        qsizetype end = d.indexOf('\n', pos);
        if (end < 0)
            end = d.size();
        *out = d.mid(pos, end - pos);
        if (out->endsWith('\r'))
            out->chop(1);
        pos = end + 1;
        return true;
    }

    // The next line that is not blank.
    bool nonBlankLine(QByteArray *out)
    {
        while (line(out))
            if (!out->trimmed().isEmpty())
                return true;
        return false;
    }

    bool integer(int *out)
    {
        if (raw)
            return bytes(out, sizeof(int));
        QByteArray t;
        bool ok = false;
        if (word(&t))
            *out = t.toInt(&ok);
        return ok || fail(t);
    }

    bool size(long long *out)
    {
        if (raw) {
            unsigned long long v = 0;
            if (!bytes(&v, sizeof v))
                return false;
            *out = static_cast<long long>(v);
            return *out >= 0 || fail("a count or tag out of range");
        }
        QByteArray t;
        bool ok = false;
        if (word(&t))
            *out = t.toLongLong(&ok);
        return (ok && *out >= 0) || fail(t);
    }

    bool real(double *out)
    {
        if (raw)
            return bytes(out, sizeof(double));
        QByteArray t;
        bool ok = false;
        if (word(&t))
            *out = t.toDouble(&ok);
        return ok || fail(t);
    }

    // Where the cursor is, as a person finds it: a line in text, a byte in
    // binary data.
    QString where(qsizetype at = -1) const
    {
        if (at < 0)
            at = pos;
        if (raw)
            return QString("byte %1").arg(at);
        qsizetype n = 1;
        for (qsizetype i = 0; i < at && i < d.size(); ++i)
            n += d[i] == '\n';
        return QString("line %1").arg(n);
    }

    qsizetype position() const { return pos; }

    // Sets the error (once) and returns false.
    bool failAt(qsizetype at, const QString &what)
    {
        if (error.isEmpty())
            error = where(at) + ": " + what;
        return false;
    }

private:
    const QByteArray &d;
    qsizetype pos = 0;

    bool word(QByteArray *out)
    {
        while (pos < d.size() && std::isspace(static_cast<unsigned char>(d[pos])))
            ++pos;
        const qsizetype start = pos;
        while (pos < d.size() && !std::isspace(static_cast<unsigned char>(d[pos])))
            ++pos;
        *out = d.mid(start, pos - start);
        return !out->isEmpty();
    }

    bool bytes(void *out, size_t n)
    {
        if (pos + qsizetype(n) > d.size()) {
            pos = d.size();
            return fail(QByteArray());
        }
        std::memcpy(out, d.constData() + pos, n);
        pos += qsizetype(n);
        return true;
    }

    bool fail(const QByteArray &found)
    {
        if (found.isEmpty())
            return failAt(pos, "the file ends inside " + section);
        return failAt(pos - found.size(),
                      QString("expected a number in %1, found \"%2\"")
                          .arg(section, QString::fromLatin1(found.left(40))));
    }
};

bool endOf(Cursor &c, const QByteArray &name)
{
    c.raw = false;
    QByteArray l;
    if (!c.nonBlankLine(&l))
        return c.failAt(c.position(), "the file ends inside " + QString::fromLatin1(name));
    if (l.trimmed() != "$End" + name.mid(1))
        return c.failAt(c.position() - l.size() - 1,
                        QString("expected $End%1, found \"%2\"")
                            .arg(QString::fromLatin1(name.mid(1)),
                                 QString::fromLatin1(l.trimmed().left(40))));
    return true;
}

bool meshFormat(Cursor &c, MshFile &f)
{
    QByteArray l;
    const qsizetype at = c.position();
    if (!c.nonBlankLine(&l))
        return c.failAt(at, "the file ends inside $MeshFormat");
    const QList<QByteArray> w = l.simplified().split(' ');
    if (w.size() != 3)
        return c.failAt(at, QString("expected \"version file-type data-size\", found \"%1\"")
                                .arg(QString::fromLatin1(l.left(40))));
    f.version = QString::fromLatin1(w[0]);
    if (f.version != "4.1")
        return c.failAt(at, QString("MSH version %1 is not read; only 4.1 is (Gmsh: "
                                    "-format msh41)")
                                .arg(f.version));
    f.binary = w[1] == "1";
    if (!f.binary && w[1] != "0")
        return c.failAt(at, "file type " + QString::fromLatin1(w[1]) + " is neither ASCII nor binary");
    c.binary = f.binary;
    c.sizeBytes = w[2].toInt();
    if (f.binary) {
        if (c.sizeBytes != 8)
            return c.failAt(at, QString("a binary file with %1-byte sizes is not read yet")
                                    .arg(c.sizeBytes));
        c.raw = true;
        int one = 0;
        if (!c.integer(&one))
            return false;
        if (one != 1)
            return c.failAt(c.position() - 4,
                            "written with the other byte order, which is not read yet");
    }
    return endOf(c, "$MeshFormat");
}

bool physicalNames(Cursor &c, MshFile &f)
{
    // Text even in a binary file.
    long long n = 0;
    if (!c.size(&n))
        return false;
    QByteArray l;
    c.line(&l);  // the rest of the count's line
    for (long long i = 0; i < n; ++i) {
        const qsizetype at = c.position();
        if (!c.nonBlankLine(&l))
            return c.failAt(at, "the file ends inside $PhysicalNames");
        const QByteArray t = l.trimmed();
        const qsizetype quote = t.indexOf('"');
        const QList<QByteArray> w = t.left(quote).simplified().split(' ');
        bool okDim = false, okTag = false;
        MshPhysicalName p;
        if (w.size() == 2) {
            p.dimension = w[0].toInt(&okDim);
            p.tag = w[1].toInt(&okTag);
        }
        if (quote < 0 || !t.endsWith('"') || t.size() - quote < 2 || !okDim || !okTag)
            return c.failAt(at, QString("expected 'dimension tag \"name\"', found \"%1\"")
                                    .arg(QString::fromLatin1(t.left(60))));
        p.name = QString::fromUtf8(t.mid(quote + 1, t.size() - quote - 2));
        f.physicalNames.push_back(p);
    }
    return endOf(c, "$PhysicalNames");
}

bool entities(Cursor &c, MshFile &f)
{
    c.raw = c.binary;
    long long count[4] = {};
    for (long long &n : count)
        if (!c.size(&n))
            return false;
    for (int dim = 0; dim < 4; ++dim) {
        for (long long i = 0; i < count[dim]; ++i) {
            MshEntity e;
            e.dimension = dim;
            double box[6];
            long long phys = 0;
            if (!c.integer(&e.tag))
                return false;
            for (int k = 0; k < (dim == 0 ? 3 : 6); ++k)
                if (!c.real(&box[k]))
                    return false;
            if (!c.size(&phys))
                return false;
            for (long long k = 0; k < phys; ++k) {
                int p = 0;
                if (!c.integer(&p))
                    return false;
                e.physicalTags.push_back(p);
            }
            if (dim > 0) {
                long long bounding = 0;
                if (!c.size(&bounding))
                    return false;
                for (long long k = 0; k < bounding; ++k) {
                    int b = 0;
                    if (!c.integer(&b))
                        return false;
                }
            }
            f.entities.push_back(e);
        }
    }
    return endOf(c, "$Entities");
}

bool nodes(Cursor &c, MshFile &f)
{
    c.raw = c.binary;
    long long blocks = 0, total = 0, minTag = 0, maxTag = 0;
    if (!c.size(&blocks) || !c.size(&total) || !c.size(&minTag) || !c.size(&maxTag))
        return false;
    for (long long b = 0; b < blocks; ++b) {
        const qsizetype at = c.position();
        int dim = 0, tag = 0, parametric = 0;
        long long n = 0;
        if (!c.integer(&dim) || !c.integer(&tag) || !c.integer(&parametric) || !c.size(&n))
            return false;
        if (parametric != 0)
            return c.failAt(at, QString("nodes with parametric coordinates (entity %1 of "
                                        "dimension %2) are not read yet")
                                    .arg(tag)
                                    .arg(dim));
        const size_t first = f.nodes.size();
        for (long long i = 0; i < n; ++i) {
            MshNode node;
            node.entityDimension = dim;
            node.entityTag = tag;
            if (!c.size(&node.tag))
                return false;
            f.nodes.push_back(node);
        }
        for (long long i = 0; i < n; ++i) {
            MshNode &node = f.nodes[first + size_t(i)];
            if (!c.real(&node.x) || !c.real(&node.y) || !c.real(&node.z))
                return false;
        }
    }
    if (static_cast<long long>(f.nodes.size()) != total)
        return c.failAt(c.position(), QString("$Nodes says %1 nodes but holds %2")
                                          .arg(total)
                                          .arg(f.nodes.size()));
    return endOf(c, "$Nodes");
}

bool elements(Cursor &c, MshFile &f)
{
    c.raw = c.binary;
    long long blocks = 0, total = 0, minTag = 0, maxTag = 0;
    if (!c.size(&blocks) || !c.size(&total) || !c.size(&minTag) || !c.size(&maxTag))
        return false;
    const auto &types = readTypes();
    for (long long b = 0; b < blocks; ++b) {
        const qsizetype at = c.position();
        int dim = 0, tag = 0, type = 0;
        long long n = 0;
        if (!c.integer(&dim) || !c.integer(&tag) || !c.integer(&type) || !c.size(&n))
            return false;
        const auto it = types.find(type);
        if (it == types.end())
            return c.failAt(at, QString("element type %1 (%2) is not read yet")
                                    .arg(type)
                                    .arg(elementTypeName(type)));
        if (it->second.dimension != dim)
            return c.failAt(at, QString("%1 elements on an entity of dimension %2")
                                    .arg(elementTypeName(type))
                                    .arg(dim));
        for (long long i = 0; i < n; ++i) {
            MshElement e;
            e.type = MshElementType(type);
            e.entityDimension = dim;
            e.entityTag = tag;
            if (!c.size(&e.tag))
                return false;
            e.nodes.resize(size_t(it->second.nodes));
            for (long long &node : e.nodes)
                if (!c.size(&node))
                    return false;
            f.elements.push_back(std::move(e));
        }
    }
    if (static_cast<long long>(f.elements.size()) != total)
        return c.failAt(c.position(), QString("$Elements says %1 elements but holds %2")
                                          .arg(total)
                                          .arg(f.elements.size()));
    return endOf(c, "$Elements");
}

// Sections of the format not read yet: refused rather than ignored, as
// they change what the mesh means.
bool refused(const QByteArray &name)
{
    static const QList<QByteArray> names = {
        "$PartitionedEntities", "$Periodic",        "$GhostElements",
        "$Parametrizations",    "$NodeData",        "$ElementData",
        "$ElementNodeData",     "$InterpolationScheme",
    };
    return names.contains(name);
}

} // namespace

QString elementTypeName(int type)
{
    const auto it = typeNames().find(type);
    return it == typeNames().end() ? QString("type %1").arg(type) : QString(it->second);
}

int nodesPerElement(MshElementType type)
{
    const auto it = readTypes().find(int(type));
    return it == readTypes().end() ? 0 : it->second.nodes;
}

int dimensionOf(MshElementType type)
{
    const auto it = readTypes().find(int(type));
    return it == readTypes().end() ? -1 : it->second.dimension;
}

const MshEntity *MshFile::entity(int dimension, int tag) const
{
    for (const MshEntity &e : entities)
        if (e.dimension == dimension && e.tag == tag)
            return &e;
    return nullptr;
}

QString MshFile::physicalName(int dimension, int tag) const
{
    for (const MshPhysicalName &p : physicalNames)
        if (p.dimension == dimension && p.tag == tag)
            return p.name;
    static const char *kinds[] = {"Point", "Curve", "Surface", "Volume"};
    const char *kind = dimension >= 0 && dimension < 4 ? kinds[dimension] : "Entity";
    return QString("Physical%1%2").arg(kind).arg(tag);
}

MshFile readMsh(const QString &path)
{
    MshFile f;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        f.error = QString("cannot open %1: %2").arg(path, file.errorString());
        return f;
    }
    const QByteArray data = file.readAll();
    Cursor c(data);
    bool first = true;
    QByteArray l;
    while (c.nonBlankLine(&l)) {
        const qsizetype at = c.position() - l.size() - 1;
        const QByteArray name = l.trimmed();
        if (first && name != "$MeshFormat") {
            f.error = "not a Gmsh mesh: it does not begin with $MeshFormat";
            return f;
        }
        first = false;
        if (!name.startsWith('$')) {
            c.failAt(at, QString("expected a section, found \"%1\"")
                             .arg(QString::fromLatin1(name.left(40))));
            break;
        }
        c.section = QString::fromLatin1(name);
        bool ok = true;
        if (name == "$MeshFormat")
            ok = meshFormat(c, f);
        else if (name == "$PhysicalNames")
            ok = physicalNames(c, f);
        else if (name == "$Entities")
            ok = entities(c, f);
        else if (name == "$Nodes")
            ok = nodes(c, f);
        else if (name == "$Elements")
            ok = elements(c, f);
        else if (refused(name))
            ok = c.failAt(at, QString("%1 is not read yet").arg(c.section));
        else {
            // Left to the user by the format ($Comments and the like).
            const QByteArray end = "$End" + name.mid(1);
            bool found = false;
            while (!found && c.line(&l))
                found = l.trimmed() == end;
            ok = found || c.failAt(at, QString("%1 has no %2")
                                           .arg(c.section, QString::fromLatin1(end)));
            f.passedOver << c.section;
        }
        if (!ok)
            break;
    }
    if (first && c.error.isEmpty()) {
        f.error = "not a Gmsh mesh: it does not begin with $MeshFormat";
        return f;
    }
    f.error = c.error;
    return f;
}

} // namespace gf::gmsh
