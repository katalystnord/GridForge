#include "calculix/DeckWriter.h"

#include "calculix/InpWriter.h"
#include "core/ApplicationInfo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>

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

// A tetrahedron's face on the outside of the mesh: the cell, ccx's face
// number (S1: corners 1 2 3, S2: 1 4 2, S3: 2 4 3, S4: 3 4 1, as in the
// ccx manual's C3D4 and C3D10 faces; VTK's node order is ccx's), the
// face's corners and, for C3D10, its middle nodes, and its area.
struct OuterFace {
    vtkIdType cell;
    int side;
    std::vector<vtkIdType> corners, middles;
    double area;
};

std::vector<OuterFace> outerFaces(const gf::core::ResultsSeries &mesh, const QString &group)
{
    static const int corner[4][3] = {{0, 1, 2}, {0, 1, 3}, {1, 2, 3}, {0, 2, 3}};
    static const int middle[4][3] = {{4, 5, 6}, {4, 8, 7}, {5, 9, 8}, {6, 9, 7}};
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    const gf::core::NamedSet *set = find(mesh, group, true);
    std::vector<bool> on(grid->GetNumberOfPoints(), false);
    for (vtkIdType p : set ? set->members : std::vector<vtkIdType>{})
        on[p] = true;
    // Faces used once are outside.
    std::map<std::array<vtkIdType, 3>, int> uses;
    std::vector<OuterFace> candidates;
    for (vtkIdType c = 0; c < grid->GetNumberOfCells(); ++c) {
        vtkIdType n;
        const vtkIdType *pts;
        grid->GetCellPoints(c, n, pts);
        for (int f = 0; f < 4; ++f) {
            std::array<vtkIdType, 3> key{pts[corner[f][0]], pts[corner[f][1]], pts[corner[f][2]]};
            std::sort(key.begin(), key.end());
            ++uses[key];
            if (!on[key[0]] || !on[key[1]] || !on[key[2]])
                continue;
            OuterFace face{c, f + 1, {key.begin(), key.end()}, {}, 0};
            if (n == 10)
                for (int k : middle[f])
                    face.middles.push_back(pts[k]);
            double a[3], b[3], d[3];
            grid->GetPoint(pts[corner[f][0]], a);
            grid->GetPoint(pts[corner[f][1]], b);
            grid->GetPoint(pts[corner[f][2]], d);
            const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {d[0] - a[0], d[1] - a[1], d[2] - a[2]};
            face.area = 0.5 * std::sqrt(std::pow(u[1] * v[2] - u[2] * v[1], 2) + std::pow(u[2] * v[0] - u[0] * v[2], 2)
                                        + std::pow(u[0] * v[1] - u[1] * v[0], 2));
            candidates.push_back(face);
        }
    }
    std::vector<OuterFace> out;
    for (const OuterFace &f : candidates) {
        std::array<vtkIdType, 3> key{f.corners[0], f.corners[1], f.corners[2]};
        if (uses[key] == 1)
            out.push_back(f);
    }
    return out;
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
            if (m.values.count(key)) {
                const auto source = m.source.find(key);
                given << QString("%1 %2 (%3)")
                             .arg(key, inpNumber(m.values.at(key)),
                                  source != m.source.end() ? source->second : QString("entered in the study"));
            }
        for (const char *key : {"YoungsModulus", "PoissonRatio"})
            if (!m.values.count(key))
                fault(m.card.isEmpty()
                          ? QString("material %1: no %2 given in the study, and it has no card").arg(m.name, key)
                          : QString("material %1: no %2, neither in the study nor in its card").arg(m.name, key));
        if (!given.isEmpty())
            text += QString("** material %1: %2\n").arg(m.name, given.join(", "));
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
        // Increments given: the *STATIC data line, and room for ccx to take
        // the smallest over the whole period (it stops a step at 100
        // increments unless told; its own smallest is 1e-5 of the period).
        const gf::core::Increments &inc = step.increments;
        const bool given = inc.initial || inc.period || inc.min || inc.max;
        const double period = inc.period.value_or(1);
        QString stepLine = nlgeom ? "*STEP, NLGEOM" : "*STEP";
        QString staticLines = "*STATIC\n";
        if (given) {
            const double smallest = inc.min.value_or(1e-5 * period);
            stepLine += QString(", INC=%1").arg(static_cast<long long>(std::ceil(period / smallest)));
            QStringList fields{inpNumber(inc.initial.value_or(period)), inpNumber(period)};
            if (inc.min || inc.max)
                fields << inpNumber(smallest) << inpNumber(inc.max.value_or(period));
            staticLines += fields.join(", ") + "\n";
        }
        deck += stepLine + "\n" + staticLines;
        QString moves;
        for (size_t i = 0; i < step.supports.size(); ++i) {
            const gf::core::Support &s = step.supports[i];
            moves += boundary(mesh, s.group, s.fixed, s.symmetry, s.displacement,
                              QString("step %1, supports[%2]").arg(step.name).arg(i));
        }
        QString cloads, dloads;
        vtkDataArray *nodeIds = mesh.points.front().grid->GetPointData()->GetArray("node id");
        vtkDataArray *elementIds = mesh.points.front().grid->GetCellData()->GetArray("element id");
        auto nodeId = [&](vtkIdType p) { return nodeIds ? static_cast<long long>(nodeIds->GetTuple1(p)) : p + 1; };
        auto elementId = [&](vtkIdType c) {
            return elementIds ? static_cast<long long>(elementIds->GetTuple1(c)) : c + 1;
        };
        for (const gf::core::Load &l : step.loads) {
            using Kind = gf::core::Load::Kind;
            if ((l.kind == Kind::Pressure || l.kind == Kind::Force) && outerFaces(mesh, l.group).empty())
                fault(QString("step %1: group \"%2\" has no face on the outside of the mesh for the load to act on")
                          .arg(step.name, l.group));
            if (l.kind == Kind::Displacement) {
                moves += boundary(mesh, l.group, false, false, l.displacement, "step " + step.name);
            } else if (l.kind == Kind::Pressure) {
                for (const OuterFace &f : outerFaces(mesh, l.group))
                    dloads += QString("%1, P%2, %3\n").arg(elementId(f.cell)).arg(f.side).arg(inpNumber(l.pressure));
            } else if (l.kind == Kind::Force) {
                // Each face's share by its area: a third to each corner of a
                // linear face, a third to each middle node of a quadratic one
                // (its corners take none under a uniform load).
                std::map<vtkIdType, double> weight;
                double total = 0;
                for (const OuterFace &f : outerFaces(mesh, l.group)) {
                    for (vtkIdType p : f.middles.empty() ? f.corners : f.middles)
                        weight[p] += f.area / 3;
                    total += f.area;
                }
                std::map<long long, vtkIdType> byId;
                for (const auto &[p, w] : weight)
                    byId[nodeId(p)] = p;
                const std::optional<double> by[3] = {l.vector.x, l.vector.y, l.vector.z};
                for (const auto &[id, p] : byId)
                    for (int d = 0; d < 3; ++d)
                        if (by[d] && *by[d] != 0)
                            cloads += QString("%1, %2, %3\n").arg(id).arg(d + 1).arg(inpNumber(*by[d] * weight[p] / total));
            } else {
                const double x = l.vector.x.value_or(0), y = l.vector.y.value_or(0), z = l.vector.z.value_or(0);
                const double size = std::sqrt(x * x + y * y + z * z);
                if (size == 0)
                    fault(QString("step %1: gravity of size 0").arg(step.name));
                // Every material the gravity acts on needs a density.
                for (const auto &[group, material] : setup.assign) {
                    const gf::core::Group *g = setup.group(group);
                    bool acted = l.group.isEmpty() || l.group == group;
                    if (!acted && g)
                        for (int solid : setup.group(l.group)->solids)
                            acted = acted || std::count(g->solids.begin(), g->solids.end(), solid);
                    const gf::core::Material *m = setup.material(material);
                    if (acted && m && !m->values.count("Density"))
                        fault(QString("step %1: gravity, but material %2 has no Density").arg(step.name, m->name));
                }
                dloads += QString("%1, GRAV, %2, %3, %4, %5\n")
                              .arg(l.group.isEmpty() ? QString("EALL") : inpName(l.group), inpNumber(size),
                                   inpNumber(x / size), inpNumber(y / size), inpNumber(z / size));
            }
        }
        if (!moves.isEmpty())
            deck += "*BOUNDARY\n" + moves;
        if (!cloads.isEmpty())
            deck += "*CLOAD\n" + cloads;
        if (!dloads.isEmpty())
            deck += "*DLOAD\n" + dloads;
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
