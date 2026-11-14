#include "core/Toolkit.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace fs::core {

namespace {

QStringList strings(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &e : v.toArray())
        out << e.toString();
    return out;
}

const QStringList kStages{"mesh", "solve", "derive"};
const QStringList kTrust{"solved", "surrogate", "derived"};

// A program: in the toolkit's folder when it is there, else on PATH.
QString locate(const QString &folder, const QString &program)
{
    const QFileInfo here(QDir(folder).absoluteFilePath(program));
    if (!program.contains('/') && here.isFile() && here.isExecutable())
        return here.absoluteFilePath();
    if (program.contains('/')) {
        const QFileInfo f(QDir(folder).absoluteFilePath(program));
        return f.isFile() && f.isExecutable() ? f.absoluteFilePath() : QString();
    }
    return QStandardPaths::findExecutable(program);
}

} // namespace

Toolkit readToolkit(const QString &folder, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return Toolkit{};
    };
    const QString path = QDir(folder).absoluteFilePath("toolkit.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail(path + ": cannot be read");
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!doc.isObject())
        return fail(path + ": not JSON (" + pe.errorString() + ")");
    const QJsonObject o = doc.object();
    if (o.value("format").toString() != "fluidstress-toolkit")
        return fail(path + ": not a toolkit manifest (format is not \"fluidstress-toolkit\")");
    if (!o.value("version").toString().startsWith("1."))
        return fail(path + ": manifest version " + o.value("version").toString()
                    + " is not one this application reads (1.x)");

    Toolkit t;
    t.folder = QFileInfo(folder).absoluteFilePath();
    t.id = o.value("id").toString();
    static const QRegularExpression idRe("^[a-z0-9][a-z0-9-]*$");
    if (!idRe.match(t.id).hasMatch())
        return fail(path + ": id \"" + t.id + "\" must be lowercase letters, digits and hyphens");
    const QString who = "toolkit \"" + t.id + "\" (" + path + ")";
    t.name = o.value("name").toString(t.id);
    t.toolkitVersion = o.value("toolkit_version").toString();
    t.licence = o.value("licence").toString();
    t.homepage = o.value("homepage").toString();
    t.notices = strings(o.value("notices"));

    for (const QJsonValue &sv : o.value("stages").toArray()) {
        const QJsonObject so = sv.toObject();
        ToolkitStage s{so.value("stage").toString(), strings(so.value("kinds")),
                       strings(so.value("features")), so.value("trust").toString()};
        if (!kStages.contains(s.stage))
            return fail(who + ": stage \"" + s.stage + "\" is not one of " + kStages.join(", "));
        if (!kTrust.contains(s.trust))
            return fail(who + ": trust \"" + s.trust + "\" is not one of " + kTrust.join(", "));
        if (s.kinds.isEmpty())
            return fail(who + ": its " + s.stage + " stage names no kinds");
        t.stages.push_back(s);
    }
    if (t.stages.empty())
        return fail(who + ": it declares no stages");

    t.command = strings(o.value("command"));
    if (t.command.isEmpty() || t.command.front().isEmpty())
        return fail(who + ": no command to run");
    if (!t.command.contains("{job}"))
        return fail(who + ": its command never names the job folder ({job})");

    for (const QJsonValue &rv : o.value("requires").toArray()) {
        const QJsonObject ro = rv.toObject();
        ToolkitRequirement r;
        if (ro.contains("program"))
            r.oneOf << ro.value("program").toString();
        r.oneOf << strings(ro.value("one_of"));
        if (!r.oneOf.isEmpty())
            t.requirements.push_back(r);
    }
    return t;
}

ToolkitSet findToolkits(const QStringList &folders)
{
    ToolkitSet set;
    for (const QString &root : folders) {
        const QDir dir(root);
        for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString folder = dir.absoluteFilePath(sub);
            if (!QFileInfo::exists(QDir(folder).absoluteFilePath("toolkit.json")))
                continue;
            QString err;
            Toolkit t = readToolkit(folder, &err);
            if (t.id.isEmpty()) {
                set.problems << err;
                continue;
            }
            auto same = std::find_if(set.toolkits.begin(), set.toolkits.end(),
                                     [&](const Toolkit &k) { return k.id == t.id; });
            if (same != set.toolkits.end()) {
                set.notes << "toolkit \"" + t.id + "\" in " + t.folder + " is used instead of the one in "
                                 + same->folder;
                *same = std::move(t);
            } else {
                set.toolkits.push_back(std::move(t));
            }
        }
    }
    return set;
}

const Toolkit *ToolkitSet::find(const QString &id) const
{
    for (const Toolkit &t : toolkits)
        if (t.id == id)
            return &t;
    return nullptr;
}

std::vector<Offer> ToolkitSet::offers(const QString &stage, const QString &kind,
                                      const QStringList &features) const
{
    std::vector<Offer> out;
    for (const Toolkit &t : toolkits) {
        const ToolkitStage *withStage = nullptr;
        const ToolkitStage *withKind = nullptr;
        for (const ToolkitStage &s : t.stages) {
            if (s.stage != stage)
                continue;
            withStage = &s;
            if (s.kinds.contains(kind))
                withKind = &s;
        }
        if (!withStage)
            continue;  // not a toolkit for this stage at all
        Offer o{&t, false, {}, {}};
        if (!withKind) {
            o.why = t.name + " does not do " + kind;
        } else {
            QStringList lacking;
            for (const QString &f : features)
                if (!withKind->features.contains(f))
                    lacking << f;
            if (!lacking.isEmpty()) {
                o.why = t.name + ": no " + lacking.join(", no ");
            } else {
                o.can = true;
                o.trust = withKind->trust;
            }
        }
        out.push_back(o);
    }
    return out;
}

Availability availability(const Toolkit &t)
{
    Availability a;
    if (locate(t.folder, t.command.front()).isEmpty())
        a.missing << t.command.front();
    for (const ToolkitRequirement &r : t.requirements) {
        bool any = false;
        for (const QString &p : r.oneOf)
            any = any || !locate(t.folder, p).isEmpty();
        if (!any)
            a.missing << r.oneOf.join(" or ");
    }
    a.ready = a.missing.isEmpty();
    return a;
}

QStringList commandFor(const Toolkit &t, const QString &jobFolder)
{
    QStringList argv;
    for (const QString &arg : t.command)
        argv << (arg == "{job}" ? jobFolder : arg);
    if (const QString found = locate(t.folder, argv.front()); !found.isEmpty())
        argv.front() = found;
    return argv;
}

} // namespace fs::core
