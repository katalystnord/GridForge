#include "core/Setup.h"

#include <QJsonArray>


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
        Group group{it.key(), numbers(g["faces"], where, "face"), numbers(g["solids"], where, "solid")};
        if (group.faces.empty() && group.solids.empty())
            fault(where, "names no faces or solids");
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
        const QJsonArray stepSupports = st["supports"].toArray();
        for (int k = 0; k < stepSupports.size(); ++k)
            step.supports.push_back(support(s, stepSupports[k].toObject(), where + QString(".supports[%1]").arg(k)));
        const QJsonArray loads = st["loads"].toArray();
        for (int k = 0; k < loads.size(); ++k) {
            const QString at = where + QString(".loads[%1]").arg(k);
            const QJsonObject l = loads[k].toObject();
            Load load;
            load.group = l["group"].toString();
            needGroup(s, load.group, at);
            if (!l.contains("displacement"))
                fault(at, "no displacement; forces, pressure and gravity are not written yet");
            load.displacement = movement(l["displacement"], at + ".displacement");
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

} // namespace gf::core
