#include "calculix/InpSets.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace gf::calculix {

const InpSet *InpSets::find(const QString &name, bool nodes) const
{
    const QString key = name.toUpper();
    for (const InpSet &s : sets)
        if (s.nodes == nodes && s.name.toUpper() == key)
            return &s;
    return nullptr;
}

namespace {

class Reader {
public:
    InpSets read(const QString &path)
    {
        readFile(path, 0);
        return std::move(r_);
    }

private:
    enum class Mode { Other, Node, Element, Nset, Elset };

    InpSets r_;
    QHash<QString, size_t> index_;   // "N:NAME" or "E:NAME" to its place in r_.sets
    std::vector<QSet<int>> members_;  // each set's ids, to keep each once

    InpSet *set(const QString &name, bool nodes)
    {
        const QString key = (nodes ? "N:" : "E:") + name.toUpper();
        auto it = index_.find(key);
        if (it == index_.end()) {
            index_.insert(key, r_.sets.size());
            r_.sets.push_back({name, nodes, {}});
            members_.emplace_back();
            return &r_.sets.back();
        }
        return &r_.sets[*it];
    }
    void add(InpSet *s, int id)
    {
        QSet<int> &m = members_[size_t(s - r_.sets.data())];
        if (!m.contains(id)) {
            m.insert(id);
            s->ids.push_back(id);
        }
    }

    void readFile(const QString &path, int depth)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            r_.error = QString("cannot open %1: %2").arg(path, f.errorString());
            return;
        }
        if (depth > 16) {
            r_.error = QString("%1: included more than 16 deep (does it include itself?)").arg(path);
            return;
        }
        r_.files << path;
        const QString file = QFileInfo(path).fileName();
        const QDir dir = QFileInfo(path).dir();

        Mode mode = Mode::Other;
        InpSet *target = nullptr;
        bool generate = false, continued = false;
        int lineNo = 0;
        auto fail = [&](const QString &what) {
            r_.error = QString("%1, line %2: %3").arg(file).arg(lineNo).arg(what);
        };

        while (r_.ok() && !f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            ++lineNo;
            if (line.isEmpty() || line.startsWith("**"))
                continue;

            if (line.startsWith('*')) {
                const QStringList parts = line.mid(1).split(',');
                const QString keyword = parts.value(0).trimmed().toUpper();
                QHash<QString, QString> param;
                for (qsizetype k = 1; k < parts.size(); ++k) {
                    const QString p = parts[k].trimmed();
                    const qsizetype eq = p.indexOf('=');
                    param.insert((eq < 0 ? p : p.left(eq)).trimmed().toUpper(),
                                 eq < 0 ? QString() : p.mid(eq + 1).trimmed());
                }
                mode = Mode::Other;
                target = nullptr;
                generate = param.contains("GENERATE");
                continued = false;
                if (keyword == "INCLUDE") {
                    const QString input = param.value("INPUT");
                    if (input.isEmpty()) {
                        fail("*INCLUDE without INPUT=");
                        return;
                    }
                    readFile(QDir::isAbsolutePath(input) ? input : dir.filePath(input), depth + 1);
                    if (!r_.ok() && !r_.error.contains(file + ", line"))
                        fail(r_.error);
                } else if (keyword == "NODE" || keyword == "NSET") {
                    mode = keyword == "NODE" ? Mode::Node : Mode::Nset;
                    const QString name = param.value("NSET");
                    if (!name.isEmpty())
                        target = set(name, true);
                    else if (mode == Mode::Nset)
                        fail("*NSET without NSET=");
                } else if (keyword == "ELEMENT" || keyword == "ELSET") {
                    mode = keyword == "ELEMENT" ? Mode::Element : Mode::Elset;
                    const QString name = param.value("ELSET");
                    if (!name.isEmpty())
                        target = set(name, false);
                    else if (mode == Mode::Elset)
                        fail("*ELSET without ELSET=");
                }
                continue;
            }

            if (mode == Mode::Other || !target)
                continue;
            QStringList fields = line.split(',');
            if (fields.last().trimmed().isEmpty())
                fields.removeLast();  // a trailing comma

            if (mode == Mode::Node || mode == Mode::Element) {
                // The first field is the id; an element's node list may go
                // on over lines that end in a comma, which hold no id.
                const bool first = !continued;
                continued = mode == Mode::Element && line.endsWith(',');
                if (!first)
                    continue;
                bool ok = false;
                const int id = fields.value(0).trimmed().toInt(&ok);
                if (!ok) {
                    fail(QString("\"%1\" is not a %2 number").arg(fields.value(0).trimmed(),
                                                                   mode == Mode::Node ? "node" : "element"));
                    return;
                }
                add(target, id);
                continue;
            }

            const bool nodes = mode == Mode::Nset;
            if (generate) {
                int v[3] = {0, 0, 1};
                bool ok = fields.size() == 2 || fields.size() == 3;
                for (qsizetype k = 0; ok && k < fields.size(); ++k)
                    v[k] = fields[k].trimmed().toInt(&ok);
                if (!ok || v[2] <= 0 || v[1] < v[0]) {
                    fail("GENERATE wants first, last[, step], with last >= first and step > 0");
                    return;
                }
                for (int id = v[0]; id <= v[1]; id += v[2])
                    add(target, id);
                continue;
            }
            for (const QString &field : fields) {
                const QString t = field.trimmed();
                if (t.isEmpty())
                    continue;
                bool ok = false;
                const int id = t.toInt(&ok);
                if (ok) {
                    add(target, id);
                    continue;
                }
                const auto it = index_.find((nodes ? "N:" : "E:") + t.toUpper());
                if (it == index_.end()) {
                    fail(QString("the set %1 lists %2, which no %3 set defines before it")
                             .arg(target->name, t, nodes ? "node" : "element"));
                    return;
                }
                // Copied, as the set may grow while it is read.
                const std::vector<int> other = r_.sets[*it].ids;
                for (int id : other)
                    add(target, id);
            }
        }
    }
};

} // namespace

InpSets readInpSets(const QString &path)
{
    return Reader().read(path);
}

} // namespace gf::calculix
