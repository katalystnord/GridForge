#include "core/Setup.h"

#include <QJsonArray>

#include <algorithm>


namespace gf::core {

const Group *Setup::group(const QString &name) const
{
    for (const Group &g : groups)
        if (g.name == name)
            return &g;
    return nullptr;
}

const Material *Setup::material(const QString &name) const
{
    for (const Material &m : materials)
        if (m.name == name)
            return &m;
    return nullptr;
}

namespace {

// A fault found while reading: what, and where.
struct Fault {
    QString text;
};

[[noreturn]] void fault(const QString &where, const QString &what) { throw Fault{where + ": " + what}; }

std::vector<int> numbers(const QJsonValue &v, const QString &where, const char *what)
{
    std::vector<int> out;
    for (const QJsonValue &n : v.toArray()) {
        if (!n.isDouble() || n.toDouble() != static_cast<int>(n.toDouble()))
            fault(where, QString("a %1 number is not a whole number").arg(what));
        if (n.toInt() < 1)
            fault(where, QString("%1 %2; %1s are numbered from 1").arg(what).arg(n.toInt()));
        out.push_back(n.toInt());
    }
    return out;
}

// Each face's signature when the group was made (study spec 4.2), by face
// number, for the group's own faces only.
std::map<int, FaceSignature> signatures(const QJsonValue &v, const Group &group, const QString &where)
{
    if (!v.isObject())
        fault(where, "not an object of faces");
    std::map<int, FaceSignature> out;
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it) {
        bool number = false;
        const int face = it.key().toInt(&number);
        if (!number)
            fault(where, QString("\"%1\" is not a face number").arg(it.key()));
        if (std::find(group.faces.begin(), group.faces.end(), face) == group.faces.end())
            fault(where, QString("face %1 is not in the group").arg(face));
        const QString at = where + "." + it.key();
        const QJsonObject s = it.value().toObject();
        FaceSignature f;
        if (!s["kind"].isString() || s["kind"].toString().isEmpty())
            fault(at + ".kind", "missing");
        f.kind = s["kind"].toString();
        if (!s["area"].isDouble() || s["area"].toDouble() <= 0)
            fault(at + ".area", "not a number above 0");
        f.area = s["area"].toDouble();
        const QJsonArray c = s["centre"].toArray();
        if (c.size() != 3 || !c[0].isDouble() || !c[1].isDouble() || !c[2].isDouble())
            fault(at + ".centre", "not three numbers");
        f.centre = {c[0].toDouble(), c[1].toDouble(), c[2].toDouble()};
        out[face] = f;
    }
    return out;
}

Movement movement(const QJsonValue &v, const QString &where)
{
    Movement m;
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!it.value().isDouble())
            fault(where + "." + it.key(), "not a number");
        const double x = it.value().toDouble();
        if (it.key() == "x")
            m.x = x;
        else if (it.key() == "y")
            m.y = x;
        else if (it.key() == "z")
            m.z = x;
        else
            fault(where, QString("\"%1\" is not x, y or z").arg(it.key()));
    }
    return m;
}

// How a displacement or force was given (study spec 4.6). One along an
// axis is checked against its numbers here; a face's normal or an edge
// needs the geometry, and is measured where the window makes it.
Direction direction(const Load &load, const QJsonValue &v, const QString &where)
{
    if (load.kind != Load::Kind::Displacement && load.kind != Load::Kind::Force)
        fault(where, "a direction is for a displacement or a force");
    const std::optional<Direction> d = parseDirection(v.toString());
    if (!d)
        fault(where + ".direction",
              QString("\"%1\" is not +x, -x, +y, -y, +z, -z, \"outward normal of face <n>\", \"inward normal of "
                      "face <n>\", \"along edge <n>\" or \"against edge <n>\"")
                  .arg(v.toString()));
    if (d->from == Direction::From::Axis) {
        const bool displacement = load.kind == Load::Kind::Displacement;
        const Movement &m = displacement ? load.displacement : load.vector;
        const QString what = displacement ? "displacement" : "force";
        const std::optional<double> parts[3] = {m.x, m.y, m.z};
        const QString says = directionText(*d);
        for (int i = 0; i < 3; ++i)
            if (i != d->axis && parts[i] && *parts[i] != 0)
                fault(where, QString("says %1, but its %2 has %3 %4").arg(says, what, QChar('x' + i)).arg(*parts[i]));
        const std::optional<double> along = parts[d->axis];
        if (!along)
            fault(where, QString("says %1, but its %2 gives no %3").arg(says, what, QChar('x' + d->axis)));
        if (*along * (d->reversed ? -1 : 1) < 0)
            fault(where, QString("says %1, but its %2's %3 is %4").arg(says, what, QChar('x' + d->axis)).arg(*along));
    }
    return *d;
}

void needGroup(const Setup &s, const QString &name, const QString &where)
{
    if (!s.group(name))
        fault(where, QString("no group \"%1\"").arg(name));
}

Support support(const Setup &s, const QJsonObject &o, const QString &where)
{
    Support out;
    out.group = o["group"].toString();
    needGroup(s, out.group, where);
    out.fixed = o["fixed"].toBool();
    out.symmetry = o["symmetry"].toBool();
    if (o.contains("displacement"))
        out.displacement = movement(o["displacement"], where + ".displacement");
    else if (!out.fixed && !out.symmetry)
        fault(where, "says neither fixed, symmetry nor a displacement");
    return out;
}

Setup read(const QJsonObject &o)
{
    Setup s;
    if (!o.contains("units"))
        fault("units", "missing; mm-N-s or m-N-s");
    const QString units = o["units"].toString();
    if (units == "mm-N-s")
        s.units = Units::MillimetreNewtonSecond;
    else if (units == "m-N-s")
        s.units = Units::MetreNewtonSecond;
    else
        fault("units", QString("\"%1\" is not one of mm-N-s, m-N-s").arg(units));

    const QJsonObject groups = o["groups"].toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QString where = "groups." + it.key();
        const QJsonObject g = it.value().toObject();
        Group group{it.key(), numbers(g["faces"], where, "face"), numbers(g["solids"], where, "solid"), {}};
        if (group.faces.empty() && group.solids.empty())
            fault(where, "names no faces or solids");
        if (g.contains("signatures"))
            group.signatures = signatures(g["signatures"], group, where + ".signatures");
        s.groups.push_back(group);
    }

    const QJsonObject materials = o["materials"].toObject();
    for (auto it = materials.begin(); it != materials.end(); ++it) {
        const QString where = "materials." + it.key();
        const QJsonObject m = it.value().toObject();
        Material material{it.key(), m["card"].toString(), m["plastic"].toString(), {}};
        const QJsonObject values = m["values"].toObject();
        for (auto v = values.begin(); v != values.end(); ++v) {
            if (!v.value().isDouble())
                fault(where + ".values." + v.key(), "not a number");
            material.values[v.key()] = v.value().toDouble();
        }
        // Where the values came from, in a job's resolved set-up.
        const QJsonObject source = m["source"].toObject();
        for (auto v = source.begin(); v != source.end(); ++v)
            material.source[v.key()] = v.value().toString();
        s.materials.push_back(material);
    }

    const QJsonObject assign = o["assign"].toObject();
    std::map<int, QString> solidOwner;
    for (auto it = assign.begin(); it != assign.end(); ++it) {
        const Group *g = s.group(it.key());
        if (!g)
            fault("assign", QString("no group \"%1\"").arg(it.key()));
        if (g->solids.empty())
            fault("assign", QString("group \"%1\" holds no solids").arg(it.key()));
        const QString material = it.value().toString();
        if (!s.material(material))
            fault("assign", QString("no material \"%1\"").arg(material));
        for (int solid : g->solids) {
            const auto [at, fresh] = solidOwner.emplace(solid, it.key());
            if (!fresh)
                fault("assign", QString("solid %1 is in \"%2\" and \"%3\", both given a material")
                                    .arg(solid)
                                    .arg(std::min(at->second, it.key()), std::max(at->second, it.key())));
        }
        s.assign[it.key()] = material;
    }

    const QJsonArray supports = o["supports"].toArray();
    for (int i = 0; i < supports.size(); ++i)
        s.supports.push_back(support(s, supports[i].toObject(), QString("supports[%1]").arg(i)));

    const QJsonArray steps = o["steps"].toArray();
    if (steps.isEmpty())
        fault("steps", "none; a study needs at least one");
    for (int i = 0; i < steps.size(); ++i) {
        const QString where = QString("steps[%1]").arg(i);
        const QJsonObject st = steps[i].toObject();
        AnalysisStep step;
        step.name = st["name"].toString(QString("Step %1").arg(i + 1));
        step.kind = st["kind"].toString("static");
        if (step.kind != "static")
            fault(where, QString("kind \"%1\" is not written yet; static is").arg(step.kind));
        if (st.contains("nonlinear geometry"))
            step.nonlinearGeometry = st["nonlinear geometry"].toBool();
        step.replace = st["replace"].toBool();
        if (st.contains("increments")) {
            const QJsonObject inc = st["increments"].toObject();
            const QString at = where + ".increments";
            auto number = [&](const char *key) -> std::optional<double> {
                if (!inc.contains(key))
                    return std::nullopt;
                if (!inc[key].isDouble() || inc[key].toDouble() <= 0)
                    fault(at + "." + key, "not a number above 0");
                return inc[key].toDouble();
            };
            step.increments = {number("initial"), number("period"), number("min"), number("max")};
            const double period = step.increments.period.value_or(1);
            if (step.increments.initial && *step.increments.initial > period)
                fault(at, QString("the initial increment %1 is longer than the period %2")
                              .arg(*step.increments.initial)
                              .arg(period));
        }
        const QJsonArray stepSupports = st["supports"].toArray();
        for (int k = 0; k < stepSupports.size(); ++k)
            step.supports.push_back(support(s, stepSupports[k].toObject(), where + QString(".supports[%1]").arg(k)));
        const QJsonArray loads = st["loads"].toArray();
        for (int k = 0; k < loads.size(); ++k) {
            const QString at = where + QString(".loads[%1]").arg(k);
            const QJsonObject l = loads[k].toObject();
            Load load;
            int kinds = 0;
            for (const char *key : {"displacement", "force", "pressure", "gravity"})
                kinds += l.contains(key) ? 1 : 0;
            if (kinds == 0)
                fault(at, "says none of displacement, force, pressure or gravity");
            if (kinds > 1)
                fault(at, "says more than one of displacement, force, pressure or gravity");
            load.group = l["group"].toString();
            if (load.group.isEmpty() && !l.contains("gravity"))
                fault(at, "no group");
            if (!load.group.isEmpty())
                needGroup(s, load.group, at);
            const Group *g = load.group.isEmpty() ? nullptr : s.group(load.group);
            if (l.contains("displacement")) {
                load.displacement = movement(l["displacement"], at + ".displacement");
            } else if (l.contains("force") || l.contains("pressure")) {
                const bool force = l.contains("force");
                if (g->faces.empty())
                    fault(at, QString("a %1 acts on faces; group \"%2\" has none")
                                  .arg(force ? "force" : "pressure", load.group));
                load.kind = force ? Load::Kind::Force : Load::Kind::Pressure;
                if (force)
                    load.vector = movement(l["force"], at + ".force");
                else if (!l["pressure"].isDouble())
                    fault(at + ".pressure", "not a number");
                else
                    load.pressure = l["pressure"].toDouble();
            } else {
                if (g && g->solids.empty())
                    fault(at, QString("gravity acts on solids; group \"%1\" has none").arg(load.group));
                load.kind = Load::Kind::Gravity;
                load.vector = movement(l["gravity"], at + ".gravity");
            }
            if (l.contains("direction"))
                load.direction = direction(load, l["direction"], at);
            step.loads.push_back(load);
        }
        for (const QJsonValue &v : st["output"].toArray())
            step.output << v.toString();
        for (const QJsonValue &v : st["raw"].toArray())
            step.raw << v.toString();
        s.steps.push_back(step);
    }
    return s;
}

} // namespace

Setup readSetup(const QJsonObject &setup, QString *error)
{
    try {
        Setup s = read(setup);
        if (error)
            error->clear();
        return s;
    } catch (const Fault &f) {
        if (error)
            *error = f.text;
        return {};
    }
}

QJsonObject meshParameters(const QJsonObject &parameters, const Setup &setup, QString *error)
{
    if (error)
        error->clear();
    if (!parameters.contains("refine"))
        return parameters;
    auto fail = [&](const QString &why) {
        if (error)
            *error = "the mesh's refine: " + why;
        return QJsonObject();
    };
    std::map<int, double> sizes;
    const QJsonObject refine = parameters["refine"].toObject();
    for (auto it = refine.begin(); it != refine.end(); ++it) {
        const Group *g = setup.group(it.key());
        if (!g)
            return fail(QString("no group \"%1\"").arg(it.key()));
        if (g->faces.empty())
            return fail(QString("group \"%1\" is solids; a size is set on faces").arg(it.key()));
        const double size = it.value().toDouble();
        if (!(size > 0))
            return fail(QString("\"%1\" asks a size of %2; a size is above 0").arg(it.key(), it.value().toVariant().toString()));
        for (int f : g->faces) {
            auto [at, added] = sizes.emplace(f, size);
            if (!added)
                at->second = std::min(at->second, size);
        }
    }
    QJsonObject out = parameters;
    out.remove("refine");
    QJsonArray faceSizes;
    for (const auto &[f, size] : sizes)
        faceSizes.append(QJsonArray{f, size});
    out["faceSizes"] = faceSizes;
    return out;
}

} // namespace gf::core
