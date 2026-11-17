#include "core/StudyOutline.h"

#include <QFileInfo>

namespace gf::core {

namespace {

QString number(double x) { return QString::number(x, 'g', 12); }

struct UnitNames {
    QString length, force, stress, acceleration;
};

UnitNames unitsOf(Units u)
{
    if (u == Units::MetreNewtonSecond)
        return {"m", "N", "Pa", "m/s2"};
    return {"mm", "N", "MPa", "mm/s2"};
}

// "x 1.5, y 0, z 0" for the directions given.
QString components(const Movement &m)
{
    QStringList parts;
    if (m.x)
        parts << "x " + number(*m.x);
    if (m.y)
        parts << "y " + number(*m.y);
    if (m.z)
        parts << "z " + number(*m.z);
    return parts.join(", ");
}

QString numbers(const char *one, const char *many, const std::vector<int> &n)
{
    if (n.empty())
        return {};
    QStringList list;
    for (int k : n)
        list << QString::number(k);
    return QString("%1 %2").arg(n.size() == 1 ? one : many, list.join(", "));
}

QString toolkitName(const QString &id)
{
    if (id == "calculix")
        return "CalculiX";
    if (id == "openfoam")
        return "OpenFOAM";
    return id;
}

OutlineItem support(const Support &s, const UnitNames &u, const QString &key)
{
    if (s.fixed)
        return {"Fixed: " + s.group, {}, "support", key, {}};
    if (s.symmetry)
        return {"Symmetry: " + s.group, {}, "support", key, {}};
    return {"Displacement: " + s.group, components(s.displacement) + " " + u.length, "support", key, {}};
}

} // namespace

std::vector<OutlineItem> studyOutline(const Study &study)
{
    const Setup &setup = study.setup;
    const UnitNames u = unitsOf(setup.units);
    std::vector<OutlineItem> out;

    QStringList files;
    for (const StudyStep *s : study.stepsOf("input"))
        files << QFileInfo(s->file).fileName();
    out.push_back({"Geometry", files.join(", "), "geometry", "geometry", {}});

    for (const StudyStep *m : study.stepsOf("mesh")) {
        QString detail;
        if (m->by == "gridforge")
            detail = QString("GridForge (Netgen), largest element %1 %2, %3")
                         .arg(number(m->parameters["maxSize"].toDouble()), u.length,
                              m->parameters["order"].toInt() == 2 ? "quadratic" : "linear");
        else
            detail = toolkitName(m->by);
        out.push_back({"Mesh", detail, "mesh", "mesh:" + m->id, {}});
    }

    OutlineItem materials{"Materials", {}, "materials", "materials", {}};
    bool anyPlastic = false;
    for (const Material &m : setup.materials) {
        QStringList detail;
        detail << (m.card.isEmpty() ? QString("values in the study") : "card " + QFileInfo(m.card).fileName());
        if (!m.plastic.isEmpty()) {
            detail << "plastic";
            anyPlastic = true;
        }
        materials.children.push_back({m.name, detail.join(", "), "material", "material:" + m.name, {}});
    }
    out.push_back(materials);

    OutlineItem groups{"Groups", {}, "groups", "groups", {}};
    for (const Group &g : setup.groups) {
        QStringList detail;
        if (!g.faces.empty())
            detail << numbers("face", "faces", g.faces);
        if (!g.solids.empty())
            detail << numbers("solid", "solids", g.solids);
        groups.children.push_back({g.name, detail.join("; "), "group", "group:" + g.name, {}});
    }
    out.push_back(groups);

    if (!setup.supports.empty()) {
        OutlineItem supports{"Supports", "in every step", "supports", "supports", {}};
        for (size_t i = 0; i < setup.supports.size(); ++i)
            supports.children.push_back(support(setup.supports[i], u, QString("support:%1").arg(i)));
        out.push_back(supports);
    }

    OutlineItem steps{"Steps", {}, "steps", "steps", {}};
    for (size_t i = 0; i < setup.steps.size(); ++i) {
        const AnalysisStep &st = setup.steps[i];
        QString detail = st.kind;
        if (st.nonlinearGeometry.value_or(anyPlastic))
            detail += ", large displacements";
        OutlineItem item{st.name, detail, "step", QString("step:%1").arg(i), {}};
        for (size_t k = 0; k < st.supports.size(); ++k)
            item.children.push_back(support(st.supports[k], u, QString("support:%1:%2").arg(i).arg(k)));
        for (size_t k = 0; k < st.loads.size(); ++k) {
            const Load &l = st.loads[k];
            const QString key = QString("load:%1:%2").arg(i).arg(k);
            switch (l.kind) {
            case Load::Kind::Displacement:
                item.children.push_back({"Displacement: " + l.group, components(l.displacement) + " " + u.length,
                                         "load", key, {}});
                break;
            case Load::Kind::Force:
                item.children.push_back({"Force: " + l.group, components(l.vector) + " " + u.force, "load", key, {}});
                break;
            case Load::Kind::Pressure:
                item.children.push_back({"Pressure: " + l.group, number(l.pressure) + " " + u.stress, "load", key, {}});
                break;
            case Load::Kind::Gravity:
                item.children.push_back({"Gravity: " + (l.group.isEmpty() ? QString("every solid") : l.group),
                                         components(l.vector) + " " + u.acceleration, "load", key, {}});
                break;
            }
        }
        steps.children.push_back(item);
    }
    out.push_back(steps);

    OutlineItem jobs{"Jobs", {}, "jobs", "jobs", {}};
    for (const StudyStep *s : study.stepsOf("solve"))
        jobs.children.push_back({s->id, toolkitName(s->by) + ", not run", "job", "job:" + s->id, {}});
    out.push_back(jobs);
    out.push_back({"Results", "none yet", "results", "results", {}});
    return out;
}

} // namespace gf::core
