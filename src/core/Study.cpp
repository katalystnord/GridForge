#include "core/Study.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace gf::core {

const StudyStep *Study::step(const QString &id) const
{
    for (const StudyStep &s : steps)
        if (s.id == id)
            return &s;
    return nullptr;
}

std::vector<const StudyStep *> Study::stepsOf(const QString &kind) const
{
    std::vector<const StudyStep *> out;
    for (const StudyStep &s : steps)
        if (s.kind == kind)
            out.push_back(&s);
    return out;
}

namespace {

struct Fault {
    QString text;
};
[[noreturn]] void fault(const QString &text) { throw Fault{text}; }

// What each kind of step gives (study spec, 2.1).
QStringList outputsOf(const QString &kind)
{
    if (kind == "input")
        return {"file"};
    if (kind == "mesh")
        return {"mesh"};
    return {"results"};  // solve, derive
}

Study read(const QString &path)
{
    const QFileInfo info(path);
    const QString file = info.isDir() ? QDir(path).filePath("study.json") : path;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        fault("no study.json in " + (info.isDir() ? path : info.absolutePath()));
    QJsonParseError parse;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll(), &parse).object();
    if (parse.error != QJsonParseError::NoError)
        fault(QString("study.json: %1 at byte %2").arg(parse.errorString()).arg(parse.offset));
    if (o["format"].toString() != "gridforge-study")
        fault(QString("study.json: not a GridForge study (its format is \"%1\")").arg(o["format"].toString()));

    Study study;
    study.folder = QFileInfo(file).absolutePath();
    const QJsonArray steps = o["steps"].toArray();
    for (int i = 0; i < steps.size(); ++i) {
        const QString where = QString("steps[%1]").arg(i);
        const QJsonObject s = steps[i].toObject();
        StudyStep step;
        step.id = s["id"].toString();
        if (step.id.isEmpty())
            fault(where + ": no id");
        if (study.step(step.id))
            fault(QString("%1: the id \"%2\" is used twice").arg(where, step.id));
        step.kind = s["kind"].toString();
        if (!QStringList{"input", "mesh", "solve", "derive"}.contains(step.kind))
            fault(QString("%1: kind \"%2\" is not input, mesh, solve or derive").arg(where, step.kind));
        step.by = s["by"].isObject() ? s["by"].toObject()["toolkit"].toString() : s["by"].toString();
        step.file = s["file"].toString();
        if (step.kind == "input" && !QFileInfo(QDir(study.folder).filePath(step.file)).isFile())
            fault(QString("%1: no file %2 in the study's folder").arg(where, step.file));
        step.parameters = s["parameters"].toObject();
        const QJsonObject inputs = s["inputs"].toObject();
        for (auto it = inputs.begin(); it != inputs.end(); ++it) {
            const QString at = QString("%1.inputs.%2").arg(where, it.key());
            QStringList links;
            if (it.value().isArray())
                for (const QJsonValue &v : it.value().toArray())
                    links << v.toString();
            else
                links << it.value().toString();
            for (const QString &link : links) {
                const QString from = link.section('.', 0, 0), output = link.section('.', 1);
                const StudyStep *source = study.step(from);
                if (!source) {
                    bool later = false;
                    for (int k = i + 1; k < steps.size(); ++k)
                        later = later || steps[k].toObject()["id"].toString() == from;
                    if (later)
                        fault(QString("%1: step \"%2\" comes after it; steps are listed in the order "
                                      "they can run")
                                  .arg(at, from));
                    fault(QString("%1: no step \"%2\"").arg(at, from));
                }
                if (!outputsOf(source->kind).contains(output))
                    fault(QString("%1: step \"%2\" has no output \"%3\" (it gives: %4)")
                              .arg(at, from, output, outputsOf(source->kind).join(", ")));
            }
            step.inputs[it.key()] = links;
        }
        study.steps.push_back(step);
    }

    study.setupJson = o["setup"].toObject();
    QString error;
    study.setup = readSetup(study.setupJson, &error);
    if (!error.isEmpty())
        fault("setup." + error);
    return study;
}

} // namespace

Study readStudy(const QString &path, QString *error)
{
    try {
        Study s = read(path);
        if (error)
            error->clear();
        return s;
    } catch (const Fault &f) {
        if (error)
            *error = f.text;
        return {};
    }
}

} // namespace gf::core
