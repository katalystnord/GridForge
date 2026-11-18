#include "core/StudyOutline.h"

#include <QFileInfo>

#include <cmath>
#include <map>

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

// A displacement or force as the user gave it: "2 mm along +z, others
// free", "50 N, inward normal of face 6", or by components as typed.
QString directed(const Movement &m, const std::optional<Direction> &d, const QString &unit, bool displacement)
{
    if (!d)
        return components(m) + " " + unit;
    const double x = m.x.value_or(0), y = m.y.value_or(0), z = m.z.value_or(0);
    QString text = number(std::sqrt(x * x + y * y + z * z)) + " " + unit;
    text += d->from == Direction::From::Normal ? ", " + directionText(*d)
            : d->from == Direction::From::Edge ? " " + directionText(*d)
                                               : " along " + directionText(*d);
    if (!displacement)
        return text;
    // The other directions: free, held, or as given.
    const int along = d->from == Direction::From::Axis ? d->axis
                      : std::abs(x) >= std::abs(y) && std::abs(x) >= std::abs(z) ? 0
                      : std::abs(y) >= std::abs(z) ? 1 : 2;
    const std::optional<double> parts[3] = {m.x, m.y, m.z};
    int held = 0, free = 0;
    Movement others;
    for (int i = 0; i < 3; ++i) {
        if (i == along)
            continue;
        held += parts[i] && *parts[i] == 0;
        free += !parts[i];
        (i == 0 ? others.x : i == 1 ? others.y : others.z) = parts[i];
    }
    if (free == 2)
        return text + ", others free";
    if (held == 2)
        return text + ", others held";
    return text + ", " + components(others);
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

    // Groups meshed finer (the GridForge mesh step's "refine"): group -> size.
    std::map<QString, double> finer;
    for (const StudyStep *m : study.stepsOf("mesh")) {
        QString detail;
        if (m->by == "gridforge") {
            detail = QString("GridForge (Netgen), largest element %1 %2, %3")
                         .arg(number(m->parameters["maxSize"].toDouble()), u.length,
                              m->parameters["order"].toInt() == 2 ? "quadratic" : "linear");
            const QJsonObject refine = m->parameters["refine"].toObject();
            if (!refine.isEmpty())
                detail += ", finer on " + refine.keys().join(", ");
            for (auto it = refine.begin(); it != refine.end(); ++it)
                finer[it.key()] = it.value().toDouble();
        } else {
            detail = toolkitName(m->by);
        }
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
            detail << numbers("face", "faces", g.faces)
                          + (finer.count(g.name) ? QString(", meshed finer: %1 %2").arg(number(finer.at(g.name)), u.length)
                                                 : QString());
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
                item.children.push_back({"Displacement: " + l.group, directed(l.displacement, l.direction, u.length, true),
                                         "load", key, {}});
                break;
            case Load::Kind::Force:
                item.children.push_back({"Force: " + l.group, directed(l.vector, l.direction, u.force, false), "load", key, {}});
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

StepTable stepTable(const Study &study)
{
    const Setup &setup = study.setup;
    const UnitNames u = unitsOf(setup.units);
    const int n = int(setup.steps.size());
    StepTable t;
    for (const AnalysisStep &st : setup.steps)
        t.steps << st.name;

    auto supportText = [&](const Support &s) {
        return s.fixed ? QString("fixed") : s.symmetry ? QString("symmetry") : components(s.displacement) + " " + u.length;
    };
    // Supports at the start hold in every step.
    for (size_t i = 0; i < setup.supports.size(); ++i) {
        const OutlineItem item = support(setup.supports[i], u, {});
        StepTable::Row row{item.title, {}};
        for (int k = 0; k < n; ++k)
            row.cells.push_back({supportText(setup.supports[i]), QString("support:%1").arg(i), false});
        t.rows.push_back(row);
    }

    // Loads and a step's supports, a row per title (kind and group), in
    // the order first met; a value set stays for the steps after.
    auto rowFor = [&](const QString &title) -> StepTable::Row & {
        for (StepTable::Row &r : t.rows)
            if (r.title == title)
                return r;
        t.rows.push_back({title, std::vector<StepCell>(n)});
        return t.rows.back();
    };
    std::map<QString, StepCell> standing;  // title -> what holds now
    for (int i = 0; i < n; ++i) {
        const AnalysisStep &st = setup.steps[i];
        if (st.replace)
            standing.clear();  // ccx's OP=NEW: what came before ends here
        for (size_t k = 0; k < st.supports.size(); ++k) {
            const OutlineItem item = support(st.supports[k], u, {});
            rowFor(item.title);
            standing[item.title] = {supportText(st.supports[k]), QString("support:%1:%2").arg(i).arg(k), false};
        }
        for (size_t k = 0; k < st.loads.size(); ++k) {
            const Load &l = st.loads[k];
            QString title, text;
            switch (l.kind) {
            case Load::Kind::Displacement:
                title = "Displacement: " + l.group;
                text = directed(l.displacement, l.direction, u.length, true);
                break;
            case Load::Kind::Force:
                title = "Force: " + l.group;
                text = directed(l.vector, l.direction, u.force, false);
                break;
            case Load::Kind::Pressure:
                title = "Pressure: " + l.group;
                text = number(l.pressure) + " " + u.stress;
                break;
            case Load::Kind::Gravity:
                title = "Gravity: " + (l.group.isEmpty() ? QString("every solid") : l.group);
                text = components(l.vector) + " " + u.acceleration;
                break;
            }
            rowFor(title);
            standing[title] = {text, QString("load:%1:%2").arg(i).arg(k), false};
        }
        for (StepTable::Row &r : t.rows) {
            // Rows of the start's supports are filled already; the rest
            // take what holds now.
            const auto at = standing.find(r.title);
            if (at != standing.end() && i < int(r.cells.size()) && r.cells[i].key.isEmpty()) {
                StepCell c = at->second;
                // Set in an earlier step: kept.
                c.kept = !c.key.startsWith(QString("load:%1:").arg(i)) && !c.key.startsWith(QString("support:%1:").arg(i));
                r.cells[i] = c;
            }
        }
    }
    return t;
}

} // namespace gf::core
