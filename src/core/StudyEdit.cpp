#include "core/StudyEdit.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <functional>
#include <set>

namespace gf::core {

namespace {

struct Units3 {
    QString length, force, stress, acceleration;
};
Units3 unitsOf(Units u)
{
    if (u == Units::MetreNewtonSecond)
        return {"m", "N", "Pa", "m/s2"};
    return {"mm", "N", "MPa", "mm/s2"};
}

std::optional<double> number(const QJsonValue &v)
{
    return v.isDouble() ? std::optional<double>(v.toDouble()) : std::nullopt;
}

// The pieces of a key: "load:0:1" -> {"load", "0", "1"}.
QStringList partsOf(const QString &key) { return key.split(':'); }

} // namespace

std::vector<EditableField> editableFields(const Study &study, const QString &key)
{
    const QStringList p = partsOf(key);
    const Units3 u = unitsOf(study.setup.units);
    std::vector<EditableField> out;
    auto xyz = [&](const Movement &m, const QString &unit, const QString &blank) {
        for (const auto &[name, v] : {std::pair{"x", m.x}, std::pair{"y", m.y}, std::pair{"z", m.z}})
            out.push_back({name, name, unit, v, blank});
    };
    if (p.value(0) == "mesh") {
        const StudyStep *s = study.step(p.value(1));
        if (s && s->kind == "mesh" && s->by == "gridforge") {
            out.push_back({"maxSize", "Largest element", u.length, number(s->parameters["maxSize"]), {}});
            out.push_back({"order", "Order (1 or 2)", {}, number(s->parameters["order"]), "1"});
        }
    } else if (p.value(0) == "load" && p.size() == 3) {
        const int i = p[1].toInt(), k = p[2].toInt();
        if (i >= 0 && i < int(study.setup.steps.size()) && k >= 0 && k < int(study.setup.steps[i].loads.size())) {
            const Load &l = study.setup.steps[i].loads[k];
            switch (l.kind) {
            case Load::Kind::Displacement:
                xyz(l.displacement, u.length, "free");
                break;
            case Load::Kind::Force:
                xyz(l.vector, u.force, "none");
                break;
            case Load::Kind::Gravity:
                xyz(l.vector, u.acceleration, "none");
                break;
            case Load::Kind::Pressure:
                out.push_back({"pressure", "Pressure (positive inward)", u.stress, l.pressure, {}});
                break;
            }
        }
    } else if (p.value(0) == "support" && p.size() == 2) {
        const int i = p[1].toInt();
        if (i >= 0 && i < int(study.setup.supports.size())) {
            const Support &s = study.setup.supports[i];
            if (!s.fixed && !s.symmetry)
                xyz(s.displacement, u.length, "free");
        }
    } else if (p.value(0) == "material") {
        if (const Material *m = study.setup.material(key.section(':', 1))) {
            const QString card = m->card.isEmpty() ? QString("none given") : QString("from its card");
            for (const auto &[name, label, unit] :
                 {std::tuple{"YoungsModulus", "Young's modulus", u.stress}, std::tuple{"PoissonRatio", "Poisson's ratio", QString()},
                  std::tuple{"Density", "Density", u.length == "mm" ? QString("t/mm3") : QString("kg/m3")}}) {
                const auto it = m->values.find(name);
                out.push_back({name, label, unit, it == m->values.end() ? std::nullopt : std::optional<double>(it->second),
                               card});
            }
        }
    }
    return out;
}

QJsonObject withEdit(const QJsonObject &study, const QString &key, const Edit &edit, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QJsonObject();
    };
    const QStringList p = partsOf(key);
    // Which names this key takes, and where in study.json they live.
    QJsonObject root = study;
    auto apply = [&](QJsonObject target, const QStringList &names) -> std::optional<QJsonObject> {
        for (const auto &[name, value] : edit) {
            if (!names.contains(name)) {
                fail(QString("%1 has no %2 to edit").arg(key, name));
                return std::nullopt;
            }
            if (value)
                target[name] = *value;
            else
                target.remove(name);
        }
        return target;
    };

    if (p.value(0) == "mesh") {
        QJsonArray steps = root["steps"].toArray();
        for (int i = 0; i < steps.size(); ++i) {
            QJsonObject s = steps[i].toObject();
            if (s["id"].toString() != p.value(1) || s["kind"].toString() != "mesh")
                continue;
            const auto params = apply(s["parameters"].toObject(), {"maxSize", "order"});
            if (!params)
                return {};
            if (!((*params)["maxSize"].toDouble() > 0))
                return fail("the largest element must be above 0");
            const int order = (*params)["order"].toInt(1);
            if (order != 1 && order != 2)
                return fail("the order is 1 or 2");
            s["parameters"] = *params;
            steps[i] = s;
            root["steps"] = steps;
            return root;
        }
        return fail("nothing to edit at " + key);
    }

    QJsonObject setup = root["setup"].toObject();
    if (p.value(0) == "load" && p.size() == 3) {
        QJsonArray steps = setup["steps"].toArray();
        const int i = p[1].toInt(), k = p[2].toInt();
        QJsonObject step = steps.at(i).toObject();
        QJsonArray loads = step["loads"].toArray();
        if (i < 0 || i >= steps.size() || k < 0 || k >= loads.size())
            return fail("nothing to edit at " + key);
        QJsonObject load = loads[k].toObject();
        if (load.contains("pressure")) {
            const auto changed = apply(load, {"pressure"});
            if (!changed)
                return {};
            load = *changed;
        } else {
            const QString kind = load.contains("displacement") ? "displacement" : load.contains("force") ? "force" : "gravity";
            const auto changed = apply(load[kind].toObject(), {"x", "y", "z"});
            if (!changed)
                return {};
            load[kind] = *changed;
        }
        loads[k] = load;
        step["loads"] = loads;
        steps[i] = step;
        setup["steps"] = steps;
    } else if (p.value(0) == "support" && p.size() == 2) {
        QJsonArray supports = setup["supports"].toArray();
        const int i = p[1].toInt();
        if (i < 0 || i >= supports.size() || !supports[i].toObject().contains("displacement"))
            return fail("nothing to edit at " + key);
        QJsonObject s = supports[i].toObject();
        const auto changed = apply(s["displacement"].toObject(), {"x", "y", "z"});
        if (!changed)
            return {};
        s["displacement"] = *changed;
        supports[i] = s;
        setup["supports"] = supports;
    } else if (p.value(0) == "material") {
        QJsonObject materials = setup["materials"].toObject();
        const QString name = key.section(':', 1);
        if (!materials.contains(name))
            return fail("nothing to edit at " + key);
        QJsonObject m = materials[name].toObject();
        const auto changed = apply(m["values"].toObject(), {"YoungsModulus", "PoissonRatio", "Density"});
        if (!changed)
            return {};
        m["values"] = *changed;
        materials[name] = m;
        setup["materials"] = materials;
    } else {
        return fail("nothing to edit at " + key);
    }
    root["setup"] = setup;
    return root;
}

namespace {

// Reads <folder>/study.json, changes it, checks the result as readStudy
// does, and writes it back; the file is left as it was on any fault.
bool rewriteStudy(const QString &folder, const std::function<QJsonObject(const QJsonObject &, QString *)> &change,
                  QString *error)
{
    const QString path = QDir(folder).filePath("study.json");
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "cannot read " + path;
        return false;
    }
    const QJsonObject before = QJsonDocument::fromJson(in.readAll()).object();
    in.close();
    const QJsonObject after = change(before, error);
    if (after.isEmpty())
        return false;
    // Checked as a study is read, before anything is written.
    QString problem;
    readSetup(after["setup"].toObject(), &problem);
    if (!problem.isEmpty()) {
        if (error)
            *error = "setup." + problem;
        return false;
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(after).toJson()) < 0 || !out.commit()) {
        if (error)
            *error = "cannot write " + path;
        return false;
    }
    if (error)
        error->clear();
    return true;
}

} // namespace

bool editStudy(const QString &folder, const QString &key, const Edit &edit, QString *error)
{
    return rewriteStudy(
        folder, [&](const QJsonObject &study, QString *e) { return withEdit(study, key, edit, e); }, error);
}

bool addFaceGroup(const QString &folder, const QString &name, const std::vector<int> &faces, QString *error)
{
    const QString named = name.trimmed();
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            auto fail = [&](const QString &why) {
                if (e)
                    *e = why;
                return QJsonObject();
            };
            if (named.isEmpty())
                return fail("a group needs a name");
            if (faces.empty())
                return fail("no faces picked");
            if (*std::min_element(faces.begin(), faces.end()) < 1)
                return fail("faces are numbered from 1");
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject groups = setup["groups"].toObject();
            if (groups.contains(named))
                return fail(QString("there is a group named %1 already").arg(named));
            const std::set<int> once(faces.begin(), faces.end());
            QJsonArray list;
            for (int f : once)
                list.append(f);
            groups[named] = QJsonObject{{"faces", list}};
            setup["groups"] = groups;
            root["setup"] = setup;
            return root;
        },
        error);
}

} // namespace gf::core
