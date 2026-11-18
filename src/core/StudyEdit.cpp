#include "core/StudyEdit.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
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

// After its numbers are edited, a load keeps the direction it says it was
// given in only while that still holds: along the same axis or line,
// turned round if the numbers were; otherwise it is given by components.
void keepDirection(QJsonObject &load, const QJsonObject &was, const QJsonObject &now)
{
    std::optional<Direction> d = parseDirection(load.value("direction").toString());
    if (!d)
        return;
    auto part = [](const QJsonObject &o, int i) { return o[QString(QChar('x' + i))].toDouble(); };
    bool holds = true;
    if (d->from == Direction::From::Axis) {
        const QString along(QChar('x' + d->axis));
        for (int i = 0; i < 3; ++i)
            holds = holds && (i == d->axis || part(now, i) == 0);
        holds = holds && now.contains(along);
        if (holds && part(now, d->axis) != 0)
            d->reversed = part(now, d->axis) < 0;
    } else {
        double cross = 0, dot = 0, a = 0, b = 0;
        for (int i = 0; i < 3; ++i) {
            const int j = (i + 1) % 3, k = (i + 2) % 3;
            const double c = part(was, j) * part(now, k) - part(was, k) * part(now, j);
            cross += c * c;
            dot += part(was, i) * part(now, i);
            a += part(was, i) * part(was, i);
            b += part(now, i) * part(now, i);
        }
        holds = b > 0 && std::sqrt(cross) <= 1e-9 * std::sqrt(a * b);
        if (holds && dot < 0)
            d->reversed = !d->reversed;
    }
    if (holds)
        load["direction"] = directionText(*d);
    else
        load.remove("direction");
}

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
            out.push_back({"throughWall", "Elements through a wall", {}, number(s->parameters["throughWall"]), "2"});
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
            const auto params = apply(s["parameters"].toObject(), {"maxSize", "order", "throughWall"});
            if (!params)
                return {};
            if (!((*params)["maxSize"].toDouble() > 0))
                return fail("the largest element must be above 0");
            const int order = (*params)["order"].toInt(1);
            if (order != 1 && order != 2)
                return fail("the order is 1 or 2");
            if (params->contains("throughWall")) {
                const double n = (*params)["throughWall"].toDouble(-1);
                if (n < 0 || n != std::floor(n))
                    return fail("the elements through a wall are a whole number, 0 or more (0: none added)");
            }
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
            const QJsonObject was = load[kind].toObject();
            const auto changed = apply(was, {"x", "y", "z"});
            if (!changed)
                return {};
            load[kind] = *changed;
            keepDirection(load, was, *changed);
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

bool createStudy(const QString &folder, const QString &geometry, double maxSize, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    const QFileInfo part(geometry);
    if (!part.isFile())
        return fail("no geometry at " + geometry);
    QDir dir(folder);
    if (dir.exists("study.json"))
        return fail("there is a study in " + folder + " already");
    if (!dir.mkpath("inputs"))
        return fail("cannot make " + dir.filePath("inputs"));
    const QString name = part.fileName();
    if (!QFile::copy(geometry, dir.filePath("inputs/" + name)))
        return fail("cannot copy " + geometry + " into " + dir.filePath("inputs"));
    // The group file FreeCAD's macro writes beside its .brep.
    const QString groups = part.dir().filePath(part.completeBaseName() + ".groups.json");
    if (QFile::exists(groups) && !QFile::copy(groups, dir.filePath("inputs/" + QFileInfo(groups).fileName())))
        return fail("cannot copy " + groups + " into " + dir.filePath("inputs"));
    const QJsonObject study{
        {"format", "gridforge-study"},
        {"version", "1.0"},
        {"steps", QJsonArray{
             QJsonObject{{"id", "part"}, {"kind", "input"}, {"file", "inputs/" + name}},
             QJsonObject{{"id", "mesh"}, {"kind", "mesh"}, {"by", "gridforge"},
                         {"inputs", QJsonObject{{"geometry", "part.file"}}},
                         {"parameters", QJsonObject{{"maxSize", maxSize}, {"order", 2}}}},
             QJsonObject{{"id", "solve"}, {"kind", "solve"}, {"by", QJsonObject{{"toolkit", "calculix"}}},
                         {"inputs", QJsonObject{{"mesh", "mesh.mesh"}}},
                         {"parameters", QJsonObject{{"kind", "structural.static"}}}}}},
        {"setup", QJsonObject{{"units", "mm-N-s"},
                              {"groups", QJsonObject{}},
                              {"materials", QJsonObject{}},
                              {"assign", QJsonObject{}},
                              {"supports", QJsonArray{}},
                              {"steps", QJsonArray{QJsonObject{
                                            {"name", "Step 1"},
                                            {"kind", "static"},
                                            {"output", QJsonArray{"displacement", "reaction", "stress"}}}}}}}};
    QSaveFile out(dir.filePath("study.json"));
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(study).toJson()) < 0 || !out.commit())
        return fail("cannot write " + dir.filePath("study.json"));
    if (error)
        error->clear();
    return true;
}

bool addGroup(const QString &folder, const Group &group, QString *error)
{
    const QString named = group.name.trimmed();
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
            if (group.faces.empty() && group.solids.empty())
                return fail("a group needs faces or solids");
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject groups = setup["groups"].toObject();
            if (groups.contains(named))
                return fail(QString("there is a group named %1 already").arg(named));
            QJsonObject g;
            auto list = [](const std::vector<int> &n) {
                QJsonArray a;
                for (int i : std::set<int>(n.begin(), n.end()))
                    a.append(i);
                return a;
            };
            if (!group.faces.empty())
                g["faces"] = list(group.faces);
            if (!group.solids.empty())
                g["solids"] = list(group.solids);
            QJsonObject signed_;
            for (int f : group.faces)
                if (const auto at = group.signatures.find(f); at != group.signatures.end())
                    signed_[QString::number(f)] = faceSignatureJson(at->second);
            if (!signed_.isEmpty())
                g["signatures"] = signed_;
            groups[named] = g;
            setup["groups"] = groups;
            root["setup"] = setup;
            return root;
        },
        error);
}

bool addMaterial(const QString &folder, const QString &name, const std::map<QString, double> &values,
                 const QString &card, QString *error)
{
    const QString named = name.trimmed();
    auto refuse = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (named.isEmpty())
        return refuse("a material needs a name");
    if (card.isEmpty() && !values.count("YoungsModulus"))
        return refuse("a material needs a FreeCAD card or its Young's modulus");
    const Study before = readStudy(folder, error);
    if (before.setup.material(named))
        return refuse(QString("there is a material named %1 already").arg(named));
    // The card copied into inputs/, so the study stays whole (spec 4.3).
    QString kept;
    if (!card.isEmpty()) {
        if (!QFileInfo(card).isFile())
            return refuse("no card at " + card);
        const QDir dir(folder);
        kept = "inputs/" + QFileInfo(card).fileName();
        const QString target = dir.filePath(kept);
        if (QFile::exists(target)) {
            QFile a(card), b(target);
            if (!a.open(QIODevice::ReadOnly) || !b.open(QIODevice::ReadOnly) || a.readAll() != b.readAll())
                return refuse(QString("%1 holds another card already").arg(kept));
        } else if (!dir.mkpath("inputs") || !QFile::copy(card, target)) {
            return refuse("cannot copy " + card + " into " + dir.filePath("inputs"));
        }
    }
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *) {
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject materials = setup["materials"].toObject();
            QJsonObject m;
            if (!kept.isEmpty())
                m["card"] = kept;
            QJsonObject v;
            for (const auto &[k, x] : values)
                v[k] = x;
            if (!v.isEmpty())
                m["values"] = v;
            materials[named] = m;
            setup["materials"] = materials;
            root["setup"] = setup;
            return root;
        },
        error);
}

bool assignMaterial(const QString &folder, const QString &group, const QString &material, QString *error)
{
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            auto fail = [&](const QString &why) {
                if (e)
                    *e = why;
                return QJsonObject();
            };
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            const QJsonObject groups = setup["groups"].toObject();
            if (!groups.contains(group))
                return fail(QString("there is no group named %1").arg(group));
            if (groups[group].toObject()["solids"].toArray().isEmpty())
                return fail(QString("group %1 holds no solids").arg(group));
            if (!setup["materials"].toObject().contains(material))
                return fail(QString("there is no material named %1").arg(material));
            QJsonObject assign = setup["assign"].toObject();
            assign[group] = material;
            setup["assign"] = assign;
            root["setup"] = setup;
            return root;
        },
        error);
}

bool signGroup(const QString &folder, const QString &name, const std::map<int, FaceSignature> &signatures,
               QString *error)
{
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject groups = setup["groups"].toObject();
            if (!groups.contains(name)) {
                if (e)
                    *e = QString("there is no group named %1").arg(name);
                return QJsonObject();
            }
            QJsonObject group = groups[name].toObject();
            QJsonObject signed_;
            for (const QJsonValue &f : group["faces"].toArray())
                if (const auto at = signatures.find(f.toInt()); at != signatures.end())
                    signed_[QString::number(f.toInt())] = faceSignatureJson(at->second);
            if (signed_.isEmpty())
                group.remove("signatures");
            else
                group["signatures"] = signed_;
            groups[name] = group;
            setup["groups"] = groups;
            root["setup"] = setup;
            return root;
        },
        error);
}

bool addFaceGroup(const QString &folder, const QString &name, const std::vector<int> &faces, QString *error,
                  const std::map<int, FaceSignature> &signatures)
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
            QJsonObject group{{"faces", list}};
            QJsonObject signed_;
            for (int f : once)
                if (const auto at = signatures.find(f); at != signatures.end())
                    signed_[QString::number(f)] = faceSignatureJson(at->second);
            if (!signed_.isEmpty())
                group["signatures"] = signed_;
            groups[named] = group;
            setup["groups"] = groups;
            root["setup"] = setup;
            return root;
        },
        error);
}

namespace {

// Each support and load that names a group, as where it is said to be
// ("the supports", "step Pull's displacement"), with a way to rename it.
// Walks setup.supports and each step's supports and loads. (The material
// assignment names groups as its keys; renameGroup and deleteGroup see to
// it themselves.)
void eachUse(QJsonObject &setup, const QString &group,
             const std::function<void(const QString &where, QJsonObject &item)> &use)
{
    if (!setup.contains("supports") && !setup.contains("steps"))
        return;
    QJsonArray supports = setup["supports"].toArray();
    for (int i = 0; i < supports.size(); ++i) {
        QJsonObject o = supports[i].toObject();
        if (o["group"].toString() == group) {
            use("the supports", o);
            supports[i] = o;
        }
    }
    if (setup.contains("supports"))
        setup["supports"] = supports;
    QJsonArray steps = setup["steps"].toArray();
    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject step = steps[i].toObject();
        const QString name = step["name"].toString();
        for (const char *list : {"supports", "loads"}) {
            QJsonArray items = step[list].toArray();
            for (int k = 0; k < items.size(); ++k) {
                QJsonObject o = items[k].toObject();
                if (o["group"].toString() != group)
                    continue;
                QString what = list;
                for (const char *kind : {"displacement", "force", "pressure", "gravity"})
                    if (QString(list) == "loads" && o.contains(kind))
                        what = kind;
                use(QString("step %1's %2").arg(name, what), o);
                items[k] = o;
            }
            if (step.contains(list))
                step[list] = items;
        }
        steps[i] = step;
    }
    setup["steps"] = steps;
}

} // namespace

// The GridForge mesh step's "refine" (study spec 2.1), each group in it
// to `change` (which may rename or drop it), written back; an empty one
// taken out.
static void eachRefine(QJsonObject &root, const std::function<void(QJsonObject &refine)> &change)
{
    QJsonArray steps = root["steps"].toArray();
    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject step = steps[i].toObject();
        if (step["kind"].toString() != "mesh" || step["by"].toString() != "gridforge")
            continue;
        QJsonObject parameters = step["parameters"].toObject();
        QJsonObject refine = parameters["refine"].toObject();
        change(refine);
        if (refine.isEmpty())
            parameters.remove("refine");
        else
            parameters["refine"] = refine;
        step["parameters"] = parameters;
        steps[i] = step;
    }
    root["steps"] = steps;
}

bool renameGroup(const QString &folder, const QString &from, const QString &to, QString *error)
{
    const QString named = to.trimmed();
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            auto fail = [&](const QString &why) {
                if (e)
                    *e = why;
                return QJsonObject();
            };
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject groups = setup["groups"].toObject();
            if (!groups.contains(from))
                return fail(QString("there is no group named %1").arg(from));
            if (named.isEmpty())
                return fail("a group needs a name");
            if (named != from && groups.contains(named))
                return fail(QString("there is a group named %1 already").arg(named));
            groups[named] = groups.take(from);
            setup["groups"] = groups;
            QJsonObject assign = setup["assign"].toObject();
            if (assign.contains(from)) {
                assign[named] = assign.take(from);
                setup["assign"] = assign;
            }
            eachUse(setup, from, [&](const QString &, QJsonObject &item) { item["group"] = named; });
            root["setup"] = setup;
            eachRefine(root, [&](QJsonObject &refine) {
                if (refine.contains(from))
                    refine[named] = refine.take(from);
            });
            return root;
        },
        error);
}

bool deleteGroup(const QString &folder, const QString &name, QString *error)
{
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            auto fail = [&](const QString &why) {
                if (e)
                    *e = why;
                return QJsonObject();
            };
            QJsonObject root = study;
            QJsonObject setup = root["setup"].toObject();
            QJsonObject groups = setup["groups"].toObject();
            if (!groups.contains(name))
                return fail(QString("there is no group named %1").arg(name));
            QString usedBy = setup["assign"].toObject().contains(name) ? QString("the material assignment") : QString();
            QJsonObject probe = setup;
            QJsonObject probeRoot = root;
            eachUse(probe, name, [&](const QString &where, QJsonObject &) {
                if (usedBy.isEmpty())
                    usedBy = where;
            });
            eachRefine(probeRoot, [&](QJsonObject &refine) {
                if (usedBy.isEmpty() && refine.contains(name))
                    usedBy = "the mesh's finer size";
            });
            if (!usedBy.isEmpty())
                return fail(QString("%1 is used by %2; take that out first").arg(name, usedBy));
            groups.remove(name);
            setup["groups"] = groups;
            root["setup"] = setup;
            return root;
        },
        error);
}

namespace {

// Where an item lives in "setup": a list ("supports" or "loads") at the
// start (step -1, supports only) or in a step, and its place in it.
struct Slot {
    int step = -1;
    QString list;
    int index = -1;
};

std::optional<Slot> slotOf(const QString &key)
{
    const QStringList p = partsOf(key);
    if (p.value(0) == "support" && p.size() == 2)
        return Slot{-1, "supports", p[1].toInt()};
    if (p.value(0) == "support" && p.size() == 3)
        return Slot{p[1].toInt(), "supports", p[2].toInt()};
    if (p.value(0) == "load" && p.size() == 3)
        return Slot{p[1].toInt(), "loads", p[2].toInt()};
    return std::nullopt;
}

QString keyOf(const Slot &s)
{
    if (s.step < 0)
        return QString("support:%1").arg(s.index);
    return QString("%1:%2:%3").arg(s.list == "loads" ? "load" : "support").arg(s.step).arg(s.index);
}

QJsonArray listAt(const QJsonObject &setup, const Slot &s)
{
    if (s.step < 0)
        return setup["supports"].toArray();
    return setup["steps"].toArray().at(s.step).toObject()[s.list].toArray();
}

void setListAt(QJsonObject &setup, const Slot &s, const QJsonArray &list)
{
    if (s.step < 0) {
        setup["supports"] = list;
        return;
    }
    QJsonArray steps = setup["steps"].toArray();
    QJsonObject step = steps[s.step].toObject();
    if (list.isEmpty())
        step.remove(s.list);
    else
        step[s.list] = list;
    steps[s.step] = step;
    setup["steps"] = steps;
}

bool exists(const QJsonObject &setup, const Slot &s)
{
    if (s.step >= setup["steps"].toArray().size())
        return false;
    return s.index >= 0 && s.index < listAt(setup, s).size();
}

// Which list of a place an entry goes in, or empty with *error.
QString listFor(const QJsonObject &entry, int step, QString *error)
{
    const bool support = entry.contains("fixed") || entry.contains("symmetry");
    if (step >= 0)
        return support ? "supports" : "loads";
    for (const char *kind : {"force", "pressure", "gravity"})
        if (entry.contains(kind)) {
            if (error)
                *error = QString("a %1 is given in a step, not among the supports").arg(kind);
            return {};
        }
    return "supports";
}

// The study with `change` made to its "setup".
QJsonObject withSetup(const QJsonObject &study, const std::function<bool(QJsonObject &)> &change)
{
    QJsonObject root = study;
    QJsonObject setup = root["setup"].toObject();
    if (!change(setup))
        return {};
    root["setup"] = setup;
    return root;
}

} // namespace

QString addItem(const QString &folder, const QString &place, const QJsonObject &entry, QString *error)
{
    QString key;
    const bool done = rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            return withSetup(study, [&](QJsonObject &setup) {
                Slot s;
                if (place.startsWith("step:")) {
                    s.step = place.section(':', 1).toInt();
                    if (s.step < 0 || s.step >= setup["steps"].toArray().size()) {
                        if (e)
                            *e = QString("there is no step %1").arg(s.step + 1);
                        return false;
                    }
                } else if (place != "supports") {
                    if (e)
                        *e = "no place called " + place;
                    return false;
                }
                s.list = listFor(entry, s.step, e);
                if (s.list.isEmpty())
                    return false;
                QJsonArray list = listAt(setup, s);
                s.index = list.size();
                list.append(entry);
                setListAt(setup, s, list);
                key = keyOf(s);
                return true;
            });
        },
        error);
    return done ? key : QString();
}

QString replaceItem(const QString &folder, const QString &key, const QJsonObject &entry, QString *error)
{
    QString now;
    const bool done = rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            return withSetup(study, [&](QJsonObject &setup) {
                const std::optional<Slot> s = slotOf(key);
                if (!s || !exists(setup, *s)) {
                    if (e)
                        *e = "nothing at " + key;
                    return false;
                }
                Slot to = *s;
                to.list = listFor(entry, s->step, e);
                if (to.list.isEmpty())
                    return false;
                QJsonArray list = listAt(setup, *s);
                if (to.list == s->list) {
                    list[s->index] = entry;
                    setListAt(setup, *s, list);
                } else {
                    list.removeAt(s->index);
                    setListAt(setup, *s, list);
                    QJsonArray other = listAt(setup, to);
                    to.index = other.size();
                    other.append(entry);
                    setListAt(setup, to, other);
                }
                now = keyOf(to);
                return true;
            });
        },
        error);
    return done ? now : QString();
}

bool removeItem(const QString &folder, const QString &key, QString *error)
{
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            return withSetup(study, [&](QJsonObject &setup) {
                const std::optional<Slot> s = slotOf(key);
                if (!s || !exists(setup, *s)) {
                    if (e)
                        *e = "nothing at " + key;
                    return false;
                }
                QJsonArray list = listAt(setup, *s);
                list.removeAt(s->index);
                setListAt(setup, *s, list);
                return true;
            });
        },
        error);
}

QJsonObject itemEntry(const QJsonObject &setup, const QString &key)
{
    const std::optional<Slot> s = slotOf(key);
    if (!s || !exists(setup, *s))
        return {};
    return listAt(setup, *s).at(s->index).toObject();
}

bool setFinerMesh(const QString &folder, const QString &group, double size, QString *error)
{
    return rewriteStudy(
        folder,
        [&](const QJsonObject &study, QString *e) {
            auto fail = [&](const QString &why) {
                if (e)
                    *e = why;
                return QJsonObject();
            };
            QJsonObject root = study;
            const QJsonObject g = root["setup"].toObject()["groups"].toObject()[group].toObject();
            if (!root["setup"].toObject()["groups"].toObject().contains(group))
                return fail(QString("there is no group named %1").arg(group));
            if (!g.contains("faces"))
                return fail(QString("%1 is solids; a finer mesh is set on faces").arg(group));
            if (!(size >= 0))
                return fail("a size is above 0 (0 takes it out)");
            bool found = false;
            eachRefine(root, [&](QJsonObject &refine) {
                found = true;
                if (size > 0)
                    refine[group] = size;
                else
                    refine.remove(group);
            });
            if (!found)
                return fail("the study has no GridForge mesh step");
            return root;
        },
        error);
}

} // namespace gf::core
