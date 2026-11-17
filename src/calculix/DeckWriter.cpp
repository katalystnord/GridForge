#include "calculix/DeckWriter.h"

#include "calculix/InpWriter.h"
#include "core/ApplicationInfo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace gf::calculix {

namespace {

struct Fault {
    QString text;
};
[[noreturn]] void fault(const QString &text) { throw Fault{text}; }

const gf::core::NamedSet *find(const gf::core::ResultsSeries &mesh, const QString &name, bool nodes)
{
    for (const gf::core::NamedSet &s : mesh.sets)
        if (s.name == name && s.nodes == nodes)
            return &s;
    return nullptr;
}

// The axis (0, 1, 2) a group's nodes all lie across, as a plane normal to
// it; -1 if they do not.
int planeAxis(const gf::core::ResultsSeries &mesh, const QString &group)
{
    const gf::core::NamedSet *set = find(mesh, group, true);
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    if (!set || set->members.empty())
        return -1;
    double bounds[6];
    grid->GetBounds(bounds);
    const double size = std::hypot(bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]);
    std::array<double, 3> low{1e300, 1e300, 1e300}, high{-1e300, -1e300, -1e300};
    for (vtkIdType p : set->members) {
        double x[3];
        grid->GetPoint(p, x);
        for (int d = 0; d < 3; ++d) {
            low[d] = std::min(low[d], x[d]);
            high[d] = std::max(high[d], x[d]);
        }
    }
    int axis = -1;
    for (int d = 0; d < 3; ++d)
        if (high[d] - low[d] <= 1e-9 * size) {
            if (axis >= 0)
                return -1;  // flat in two directions: a line or a point
            axis = d;
        }
    return axis;
}

QString boundary(const gf::core::ResultsSeries &mesh, const QString &group, bool fixed, bool symmetry,
                 const gf::core::Movement &move, const QString &where)
{
    const QString set = inpName(group);
    if (fixed)
        return set + ", 1, 3, 0\n";
    if (symmetry) {
        const int axis = planeAxis(mesh, group);
        if (axis < 0)
            fault(QString("%1: group \"%2\" is not a plane normal to x, y or z; symmetry needs one in version 1")
                      .arg(where, group));
        return QString("%1, %2, %2, 0\n").arg(set).arg(axis + 1);
    }
    QString out;
    const std::optional<double> by[3] = {move.x, move.y, move.z};
    for (int d = 0; d < 3; ++d)
        if (by[d])
            out += QString("%1, %2, %2, %3\n").arg(set).arg(d + 1).arg(inpNumber(*by[d]));
    return out;
}

QString write(const gf::core::Setup &setup, const gf::core::ResultsSeries &mesh, const QString &meshFile)
{
    if (mesh.points.empty() || !mesh.points.front().grid)
        fault("no mesh");

    // Every solid of the mesh has one material.
    std::map<int, QString> materialOf;
    for (const auto &[group, material] : setup.assign)
        if (const gf::core::Group *g = setup.group(group))
            for (int solid : g->solids)
                materialOf[solid] = material;
    for (int solid = 1; find(mesh, QString("Solid%1").arg(solid), false); ++solid)
        if (!materialOf.count(solid))
            fault(QString("solid %1 has no material").arg(solid));

    QString text;
    text += QString("** written by %1 %2 (build %3)\n")
                .arg(gf::core::applicationName(), gf::core::applicationVersion(), gf::core::applicationBuild());
    text += setup.units == gf::core::Units::MillimetreNewtonSecond
                ? "** units: mm-N-s (lengths mm, forces N, stresses MPa)\n"
                : "** units: m-N-s (lengths m, forces N, stresses Pa)\n";
    QString materials;
    bool anyPlastic = false;
    QString plasticName;
    for (const gf::core::Material &m : setup.materials) {
        QStringList given;
        for (const char *key : {"YoungsModulus", "PoissonRatio", "Density"})
            if (m.values.count(key))
                given << QString("%1 %2").arg(key, inpNumber(m.values.at(key)));
        for (const char *key : {"YoungsModulus", "PoissonRatio"})
            if (!m.values.count(key))
                fault(QString("material %1: no %2 given in the study (reading cards is not written yet)")
                          .arg(m.name, key));
        if (!given.isEmpty())
            text += QString("** material %1: %2, entered in the study\n").arg(m.name, given.join(", "));
        materials += QString("*MATERIAL, NAME=%1\n*ELASTIC\n%2, %3\n")
                         .arg(inpName(m.name), inpNumber(m.values.at("YoungsModulus")),
                              inpNumber(m.values.at("PoissonRatio")));
        if (m.values.count("Density"))
            materials += QString("*DENSITY\n%1\n").arg(inpNumber(m.values.at("Density")));
        if (!m.plastic.isEmpty()) {
            materials += QString("*INCLUDE, INPUT=%1\n").arg(m.plastic);
            bool used = false;
            for (const auto &[group, material] : setup.assign)
                used = used || material == m.name;
            if (used && !anyPlastic) {
                anyPlastic = true;
                plasticName = m.name;
            }
        }
    }

    QString deck = QString("*INCLUDE, INPUT=%1\n").arg(meshFile) + materials;
    for (const auto &[group, material] : setup.assign)
        deck += QString("*SOLID SECTION, ELSET=%1, MATERIAL=%2\n").arg(inpName(group), inpName(material));

    if (!setup.supports.empty()) {
        deck += "*BOUNDARY\n";
        for (size_t i = 0; i < setup.supports.size(); ++i) {
            const gf::core::Support &s = setup.supports[i];
            deck += boundary(mesh, s.group, s.fixed, s.symmetry, s.displacement, QString("supports[%1]").arg(i));
        }
    }

    static const std::map<QString, std::pair<bool, QString>> outputs = {
        {"displacement", {true, "U"}}, {"reaction", {true, "RF"}}, {"stress", {false, "S"}},
        {"strain", {false, "E"}},      {"plastic strain", {false, "PEEQ"}}};
    for (const gf::core::AnalysisStep &step : setup.steps) {
        if (step.replace)
            fault(QString("step %1: replacing what came before is not written yet").arg(step.name));
        deck += QString("** step %1\n").arg(step.name);
        const bool nlgeom = step.nonlinearGeometry.value_or(anyPlastic);
        if (nlgeom && !step.nonlinearGeometry)
            deck += QString("** step %1: large displacements (NLGEOM), turned on by itself: %2 is plastic\n")
                        .arg(step.name, plasticName);
        deck += nlgeom ? "*STEP, NLGEOM\n*STATIC\n" : "*STEP\n*STATIC\n";
        QString moves;
        for (size_t i = 0; i < step.supports.size(); ++i) {
            const gf::core::Support &s = step.supports[i];
            moves += boundary(mesh, s.group, s.fixed, s.symmetry, s.displacement,
                              QString("step %1, supports[%2]").arg(step.name).arg(i));
        }
        for (const gf::core::Load &l : step.loads)
            moves += boundary(mesh, l.group, false, false, l.displacement, "step " + step.name);
        if (!moves.isEmpty())
            deck += "*BOUNDARY\n" + moves;
        QStringList node, element;
        for (const QString &o : step.output) {
            const auto it = outputs.find(o);
            if (it == outputs.end())
                fault(QString("step %1: output \"%2\" is not one of displacement, reaction, stress, strain, "
                              "plastic strain")
                          .arg(step.name, o));
            (it->second.first ? node : element) << it->second.second;
        }
        if (!node.isEmpty())
            deck += "*NODE FILE\n" + node.join(", ") + "\n";
        if (!element.isEmpty())
            deck += "*EL FILE\n" + element.join(", ") + "\n";
        if (!step.raw.isEmpty())
            deck += "** given by hand in the study:\n" + step.raw.join('\n') + "\n";
        deck += "*END STEP\n";
    }
    return text + deck;
}

} // namespace

QString analysisInp(const gf::core::Setup &setup, const gf::core::ResultsSeries &mesh, const QString &meshFile,
                    QString *error)
{
    try {
        QString text = write(setup, mesh, meshFile);
        if (error)
            error->clear();
        return text;
    } catch (const Fault &f) {
        if (error)
            *error = f.text;
        return {};
    }
}

} // namespace gf::calculix
