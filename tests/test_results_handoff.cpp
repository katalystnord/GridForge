// The hand-off to ParaView: .frd results as VTK grids, then as .vtu files
// with a .pvd time series. The grids must keep ccx's numbers and names,
// put every node where VTK's cell definition expects it, say what was
// derived, and carry their provenance into the files.

#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>
#include <vtkCompositeDataSet.h>
#include <vtkDataAssembly.h>
#include <vtkHDFReader.h>
#include <vtkInformation.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkXMLUnstructuredGridReader.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <vector>

using namespace gf::calculix;
using gf::core::ResultsSeries;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif

namespace {

int failures = 0;
std::vector<std::pair<const char *, std::function<void()>>> &cases()
{
    static std::vector<std::pair<const char *, std::function<void()>>> c;
    return c;
}
struct Register {
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define CASE(name)                                  \
    void name();                                    \
    static Register reg_##name(#name, name);        \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

QString fixture(const char *rel) { return QString(GF_FIXTURES) + "/calculix/" + rel; }

ResultsSeries convert(const char *rel, QString *error = nullptr)
{
    QString e;
    const FrdFile f = readFrd(fixture(rel));
    ResultsSeries s = toSeries(f, QFileInfo(fixture(rel)).fileName(), &e);
    if (!e.isEmpty())
        std::printf("  (%s: %s)\n", rel, qPrintable(e));
    if (error)
        *error = e;
    return s;
}

const ResultsSeries &bar()
{
    static const ResultsSeries s = convert("bar-tension/bar.frd");
    return s;
}
const ResultsSeries &cantilever()
{
    static const ResultsSeries s = convert("cantilever-te10/cantilever.frd");
    return s;
}
const ResultsSeries &increments()
{
    static const ResultsSeries s = convert("bar-increments/bar.frd");
    return s;
}

vtkUnstructuredGrid *grid(const ResultsSeries &s, size_t i = 0)
{
    return i < s.points.size() ? s.points[i].grid.Get() : nullptr;
}

// The point holding ccx's node `id` (point data "node id").
vtkIdType pointOf(vtkUnstructuredGrid *g, int id)
{
    auto *ids = g ? vtkIntArray::SafeDownCast(g->GetPointData()->GetArray("node id")) : nullptr;
    for (vtkIdType p = 0; ids && p < ids->GetNumberOfTuples(); ++p)
        if (ids->GetValue(p) == id)
            return p;
    return -1;
}

double value(vtkUnstructuredGrid *g, const char *array, int node, int component)
{
    const vtkIdType p = pointOf(g, node);
    vtkDataArray *a = g ? g->GetPointData()->GetArray(array) : nullptr;
    if (p < 0 || !a || component >= a->GetNumberOfComponents())
        return NAN;
    return a->GetComponent(p, component);
}

bool near(double a, double b, double rel) { return std::abs(a - b) <= rel * std::abs(b); }

std::array<double, 3> point(vtkUnstructuredGrid *g, vtkIdType p)
{
    std::array<double, 3> x{};
    g->GetPoint(p, x.data());
    return x;
}

// Every face of every cell, as VTK defines its faces, has its normal
// pointing away from the cell's centre: the node order is VTK's, not
// mirrored or twisted. For quadratic cells, every edge's middle node sits
// halfway between its ends, as VTK's edge definition expects.
int misplacedCells(vtkUnstructuredGrid *g)
{
    int bad = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        vtkCell *cell = g->GetCell(c);
        std::array<double, 3> centre{};
        int corners = cell->GetNumberOfPoints();
        switch (cell->GetCellType()) {
        case VTK_QUADRATIC_TETRA: corners = 4; break;
        case VTK_QUADRATIC_WEDGE: corners = 6; break;
        case VTK_QUADRATIC_HEXAHEDRON: corners = 8; break;
        }
        for (int k = 0; k < corners; ++k)
            for (int d = 0; d < 3; ++d)
                centre[d] += point(g, cell->GetPointId(k))[d] / corners;
        bool ok = true;
        for (int f = 0; f < cell->GetNumberOfFaces(); ++f) {
            vtkCell *face = cell->GetFace(f);
            const auto a = point(g, face->GetPointId(0)), b = point(g, face->GetPointId(1)),
                       e = point(g, face->GetPointId(2));
            const std::array<double, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                v{e[0] - a[0], e[1] - a[1], e[2] - a[2]};
            const std::array<double, 3> n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                          u[0] * v[1] - u[1] * v[0]};
            const double out = n[0] * (a[0] - centre[0]) + n[1] * (a[1] - centre[1])
                               + n[2] * (a[2] - centre[2]);
            ok = ok && out > 0;
        }
        if (cell->GetCellType() == VTK_QUADRATIC_EDGE) {
            // A quadratic edge is its own edge: its third node sits halfway.
            const auto a = point(g, cell->GetPointId(0)), b = point(g, cell->GetPointId(1)),
                       m = point(g, cell->GetPointId(2));
            for (int d = 0; d < 3; ++d)
                ok = ok && std::abs(m[d] - (a[d] + b[d]) / 2) < 1e-9;
        }
        if (cell->GetCellDimension() == 2) {
            // A surface cell's corners turn one way round: not twisted.
            const int n = cell->GetCellType() == VTK_QUADRATIC_TRIANGLE || cell->GetCellType() == VTK_TRIANGLE ? 3 : 4;
            std::array<double, 3> first{};
            for (int k = 0; k < n; ++k) {
                const auto a = point(g, cell->GetPointId(k)), b = point(g, cell->GetPointId((k + 1) % n)),
                           e = point(g, cell->GetPointId((k + 2) % n));
                const std::array<double, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                    v{e[0] - b[0], e[1] - b[1], e[2] - b[2]};
                const std::array<double, 3> t{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                              u[0] * v[1] - u[1] * v[0]};
                if (k == 0)
                    first = t;
                ok = ok && t[0] * first[0] + t[1] * first[1] + t[2] * first[2] > 0;
            }
        }
        for (int k = 0; k < cell->GetNumberOfEdges(); ++k) {
            vtkCell *edge = cell->GetEdge(k);
            if (edge->GetNumberOfPoints() != 3)
                continue;
            const auto a = point(g, edge->GetPointId(0)), b = point(g, edge->GetPointId(1)),
                       m = point(g, edge->GetPointId(2));
            for (int d = 0; d < 3; ++d)
                ok = ok && std::abs(m[d] - (a[d] + b[d]) / 2) < 1e-9;
        }
        bad += ok ? 0 : 1;
    }
    return bad;
}

QStringList fieldStrings(vtkUnstructuredGrid *g, const char *name)
{
    QStringList out;
    auto *a = g ? vtkStringArray::SafeDownCast(g->GetFieldData()->GetAbstractArray(name)) : nullptr;
    for (vtkIdType i = 0; a && i < a->GetNumberOfValues(); ++i)
        out << QString::fromStdString(a->GetValue(i));
    return out;
}

} // namespace

CASE(the_bar_becomes_one_time_point_of_twenty_points_and_four_hexahedra)
{
    vtkUnstructuredGrid *g = grid(bar());
    CHECK(bar().points.size() == 1 && bar().points[0].time == 1.0);
    CHECK(g && g->GetNumberOfPoints() == 20 && g->GetNumberOfCells() == 4);
    for (vtkIdType c = 0; g && c < g->GetNumberOfCells(); ++c)
        CHECK(g->GetCellType(c) == VTK_HEXAHEDRON);
    auto *ids = g ? vtkIntArray::SafeDownCast(g->GetCellData()->GetArray("element id")) : nullptr;
    CHECK(ids && ids->GetValue(3) == 4);
}

CASE(every_cell_has_its_nodes_where_vtk_expects_them)
{
    vtkUnstructuredGrid *b = grid(bar());
    vtkUnstructuredGrid *c = grid(cantilever());
    CHECK(b && misplacedCells(b) == 0);
    CHECK(c && c->GetNumberOfCells() == 480 && misplacedCells(c) == 0);
    for (vtkIdType k = 0; c && k < c->GetNumberOfCells(); ++k)
        CHECK(c->GetCellType(k) == VTK_QUADRATIC_TETRA);
}

CASE(the_solvers_results_keep_their_names_and_numbers)
{
    vtkUnstructuredGrid *g = grid(bar());
    vtkDataArray *disp = g ? g->GetPointData()->GetArray("DISP") : nullptr;
    vtkDataArray *stress = g ? g->GetPointData()->GetArray("STRESS") : nullptr;
    CHECK(disp && disp->GetNumberOfComponents() == 3
          && QString(disp->GetComponentName(1)) == "D2");
    CHECK(stress && stress->GetNumberOfComponents() == 6
          && QString(stress->GetComponentName(5)) == "SZX");
    CHECK(value(g, "DISP", 20, 1) == -3.0e-3);
    CHECK(value(g, "DISP", 20, 2) == 1.0e-1);
    CHECK(value(g, "STRESS", 1, 2) == 2.09992e+02);
    CHECK(value(g, "TOSTRAIN", 20, 0) == -2.99988e-04);
    CHECK(value(g, "FORC", 17, 2) == 5.25e+03);
    CHECK(g && g->GetPointData()->GetArray("ERROR"));
}

// Uniaxial: von Mises and the first principal stress are szz itself.
CASE(derived_stresses_are_computed_and_say_derived)
{
    vtkUnstructuredGrid *g = grid(bar());
    CHECK(near(value(g, "von Mises stress (derived)", 1, 0), 2.09992e+02, 1e-12));
    CHECK(near(value(g, "principal stresses (derived)", 1, 0), 2.09992e+02, 1e-12));
    CHECK(std::abs(value(g, "principal stresses (derived)", 1, 2)) < 1e-9);
    const QStringList p = bar().provenance;
    CHECK(p.contains("derived by GridForge: von Mises stress (derived), principal stresses "
                     "(derived), from STRESS"));
}

// The cantilever's stresses have shear, so a component taken from the
// wrong place shows: at every node the principal values must sum to the
// trace and multiply to the determinant of ccx's own tensor.
CASE(derived_stresses_keep_the_invariants_of_the_solvers_tensor)
{
    vtkUnstructuredGrid *g = grid(cantilever());
    vtkDataArray *s = g ? g->GetPointData()->GetArray("STRESS") : nullptr;
    vtkDataArray *p = g ? g->GetPointData()->GetArray("principal stresses (derived)") : nullptr;
    vtkDataArray *m = g ? g->GetPointData()->GetArray("von Mises stress (derived)") : nullptr;
    CHECK(s && p && m);
    int bad = 0;
    double largestShear = 0;
    for (vtkIdType i = 0; s && p && m && i < s->GetNumberOfTuples(); ++i) {
        double t[6], v[3];
        s->GetTuple(i, t);
        p->GetTuple(i, v);
        const double xx = t[0], yy = t[1], zz = t[2], xy = t[3], yz = t[4], zx = t[5];
        const double trace = xx + yy + zz;
        const double det = xx * (yy * zz - yz * yz) - xy * (xy * zz - yz * zx)
                           + zx * (xy * yz - yy * zx);
        const double scale = std::max({std::abs(v[0]), std::abs(v[2]), 1.0});
        const double mises = std::sqrt(((v[0] - v[1]) * (v[0] - v[1]) + (v[1] - v[2]) * (v[1] - v[2])
                                        + (v[2] - v[0]) * (v[2] - v[0])) / 2);
        largestShear = std::max({largestShear, std::abs(xy), std::abs(yz), std::abs(zx)});
        if (std::abs(v[0] + v[1] + v[2] - trace) > 1e-9 * scale
            || std::abs(v[0] * v[1] * v[2] - det) > 1e-9 * scale * scale * scale
            || std::abs(m->GetTuple1(i) - mises) > 1e-9 * scale || v[0] < v[1] || v[1] < v[2])
            ++bad;
    }
    CHECK(bad == 0);
    CHECK(largestShear > 1.0);  // the check means something only with shear
}

CASE(the_provenance_names_the_solver_and_the_source)
{
    const QStringList p = bar().provenance;
    CHECK(p.contains("solver: CalculiX Version 2.23"));
    CHECK(p.contains("source: bar.frd"));
    CHECK(std::any_of(p.begin(), p.end(),
                      [](const QString &l) { return l.startsWith("converted by: GridForge "); }));
    CHECK(fieldStrings(grid(bar()), "provenance") == p);
}

CASE(each_increment_becomes_a_time_point)
{
    const ResultsSeries &s = increments();
    const double tip[] = {0.025, 0.05, 0.075, 0.1};
    CHECK(s.points.size() == 4);
    for (size_t n = 0; n < s.points.size() && n < 4; ++n) {
        CHECK(s.points[n].time == 0.25 * double(n + 1));
        CHECK(value(grid(s, n), "DISP", 20, 2) == tip[n]);
        CHECK(grid(s, n) && grid(s, n)->GetPointData()->GetArray("FORC"));
    }
}

CASE(an_element_type_not_yet_proven_is_refused_by_name)
{
    FrdFile f = readFrd(fixture("bar-tension/bar.frd"));
    f.elements[0].type = static_cast<FrdElementType>(13);  // every type the reader knows is mapped
    QString error;
    const ResultsSeries s = toSeries(f, "bar.frd", &error);
    CHECK(s.points.empty());
    CHECK(error.contains("type 13") && error.contains("element 1"));
}

// A cube in uniform tension per element type (fixtures cube-*, written by
// cubes.py): each type lands on its VTK cell with every node in place, and
// the numbers are the hand answer (szz 210 MPa, uz = 1e-3 z, ux = -3e-4 x).
CASE(each_solid_element_type_lands_on_its_vtk_cell_in_place)
{
    const struct {
        const char *frd;
        int cell, count;
    } types[] = {{"cube-he20/cube.frd", VTK_QUADRATIC_HEXAHEDRON, 8},
                 {"cube-te4/cube.frd", VTK_TETRA, 48},
                 {"cube-pe6/cube.frd", VTK_WEDGE, 16},
                 {"cube-pe15/cube.frd", VTK_QUADRATIC_WEDGE, 16}};
    for (const auto &t : types) {
        std::printf("  %s\n", t.frd);
        const ResultsSeries s = convert(t.frd);
        vtkUnstructuredGrid *g = grid(s);
        CHECK(g && g->GetNumberOfCells() == t.count);
        for (vtkIdType c = 0; g && c < g->GetNumberOfCells(); ++c)
            CHECK(g->GetCellType(c) == t.cell);
        CHECK(g && misplacedCells(g) == 0);
        vtkDataArray *disp = g ? g->GetPointData()->GetArray("DISP") : nullptr;
        vtkDataArray *stress = g ? g->GetPointData()->GetArray("STRESS") : nullptr;
        CHECK(disp && stress);
        for (vtkIdType p = 0; disp && stress && p < g->GetNumberOfPoints(); ++p) {
            const auto x = point(g, p);
            CHECK(std::abs(disp->GetComponent(p, 2) - 1e-3 * x[2]) < 1e-9);
            CHECK(std::abs(disp->GetComponent(p, 0) + 3e-4 * x[0]) < 1e-9);
            CHECK(near(stress->GetComponent(p, 2), 210.0, 1e-4));
        }
    }
}

// Shells, plane elements and beams (fixtures shell-*, plane-cps4, beam-*,
// written by strips.py): with OUTPUT=2D ccx writes them on the deck's own
// nodes as surface and line cells; without it, shells as expanded solids.
// Each lands on its VTK cell in place, with the hand answer (sxx 210 MPa,
// ux = 1e-3 x), except ccx's own B32R: see beam-b32r/README.md.
CASE(each_shell_plane_and_beam_element_lands_on_its_vtk_cell_in_place)
{
    const struct {
        const char *frd;
        int cell, count;
        double sxx;
    } types[] = {{"shell-s3/strip.frd", VTK_TRIANGLE, 8, 210.0},
                 {"shell-s4/strip.frd", VTK_QUAD, 4, 210.0},
                 {"shell-s6/strip.frd", VTK_QUADRATIC_TRIANGLE, 8, 210.0},
                 {"shell-s8/strip.frd", VTK_QUADRATIC_QUAD, 4, 210.0},
                 {"shell-s8-3d/strip.frd", VTK_QUADRATIC_HEXAHEDRON, 4, 210.0},
                 {"plane-cps4/strip.frd", VTK_QUAD, 4, 210.0},
                 {"beam-b31/strip.frd", VTK_LINE, 4, 210.0},
                 {"beam-b32r/strip.frd", VTK_QUADRATIC_EDGE, 4, 202.279}};
    for (const auto &t : types) {
        std::printf("  %s\n", t.frd);
        const ResultsSeries s = convert(t.frd);
        vtkUnstructuredGrid *g = grid(s);
        CHECK(g && g->GetNumberOfCells() == t.count);
        for (vtkIdType c = 0; g && c < g->GetNumberOfCells(); ++c)
            CHECK(g->GetCellType(c) == t.cell);
        CHECK(g && misplacedCells(g) == 0);
        vtkDataArray *disp = g ? g->GetPointData()->GetArray("DISP") : nullptr;
        vtkDataArray *stress = g ? g->GetPointData()->GetArray("STRESS") : nullptr;
        CHECK(disp && stress);
        const bool exact = t.sxx == 210.0;
        for (vtkIdType p = 0; disp && stress && p < g->GetNumberOfPoints(); ++p) {
            const auto x = point(g, p);
            if (exact)
                CHECK(std::abs(disp->GetComponent(p, 0) - 1e-3 * x[0]) < 1e-9);
            CHECK(near(stress->GetComponent(p, 0), t.sxx, exact ? 1e-4 : 1e-6));
        }
    }
}

// The deck's named sets (bar-sets/README.md lists them by hand) become
// subsets of the grids: a node set by its points, an element set by its
// cells.
ResultsSeries withSets(const char *frd, const char *inp)
{
    ResultsSeries s = convert(frd);
    const InpSets sets = readInpSets(fixture(inp));
    if (!sets.ok())
        std::printf("  (%s)\n", qPrintable(sets.error));
    addSets(s, sets, QFileInfo(fixture(inp)).fileName());
    return s;
}

QList<int> idsOf(const ResultsSeries &s, const char *name, bool nodes)
{
    QList<int> out;
    vtkUnstructuredGrid *g = grid(s);
    for (const gf::core::NamedSet &set : s.sets)
        if (g && set.name == name && set.nodes == nodes) {
            auto *ids = vtkIntArray::SafeDownCast(nodes ? g->GetPointData()->GetArray("node id")
                                                        : g->GetCellData()->GetArray("element id"));
            for (vtkIdType m : set.members)
                out << (ids ? ids->GetValue(m) : -1);
        }
    return out;
}

CASE(the_decks_named_sets_become_subsets_of_the_grids)
{
    const ResultsSeries s = withSets("bar-sets/bar.frd", "bar-sets/bar.inp");
    QStringList names;
    for (const gf::core::NamedSet &set : s.sets)
        names << set.name;
    CHECK(names == QStringList({"Nall", "Bar", "Odd", "Top", "Fixed", "Pulled", "Ends", "X0", "Y0"}));
    CHECK((idsOf(s, "Pulled", true) == QList<int>{17, 18, 19, 20}));
    CHECK((idsOf(s, "Odd", false) == QList<int>{1, 3}));
    CHECK(s.provenance.contains("sets: 9, from bar.inp"));
}

// Without OUTPUT=2D a shell's results sit on nodes ccx made: the deck's
// node sets find none of them, and the provenance says so.
CASE(an_expanded_shell_keeps_its_element_sets_and_says_its_node_sets_are_lost)
{
    const ResultsSeries s = withSets("shell-s8-3d/strip.frd", "shell-s8-3d/strip.inp");
    CHECK((idsOf(s, "EALL", false) == QList<int>{1, 2, 3, 4}));
    CHECK(idsOf(s, "X0", true).isEmpty());
    CHECK(s.provenance.contains("set X0: 3 of its 3 nodes are not in the results, left out"));
}

// A set may name nodes the results do not hold (ccx leaves out nodes no
// element uses): they are left out, and the provenance says so.
CASE(set_members_the_results_lack_are_left_out_and_said)
{
    ResultsSeries s = convert("bar-tension/bar.frd");
    InpSets sets;
    sets.sets.push_back({"Reference", true, {20, 99, 100}});
    addSets(s, sets, "deck.inp");
    CHECK((idsOf(s, "Reference", true) == QList<int>{20}));
    CHECK(s.provenance.contains("set Reference: 2 of its 3 nodes are not in the results, left out"));
}

// Sets name nodes and elements by id, not by place: the bar renumbered
// (ids times ten, as a mesher with gaps would) keeps its sets.
CASE(sets_follow_ids_however_the_mesh_is_numbered)
{
    FrdFile f = readFrd(fixture("bar-sets/bar.frd"));
    for (FrdNode &n : f.nodes)
        n.id *= 10;
    for (FrdElement &e : f.elements) {
        e.id *= 10;
        for (int &n : e.nodes)
            n *= 10;
    }
    for (FrdDataset &d : f.datasets)
        for (int &n : d.nodeIds)
            n *= 10;
    QString error;
    ResultsSeries s = toSeries(f, "bar.frd", &error);
    InpSets sets = readInpSets(fixture("bar-sets/bar.inp"));
    for (InpSet &set : sets.sets)
        for (int &id : set.ids)
            id *= 10;
    addSets(s, sets, "bar.inp");
    CHECK((idsOf(s, "Pulled", true) == QList<int>{170, 180, 190, 200}));
    CHECK((idsOf(s, "Odd", false) == QList<int>{10, 30}));
}

// One .vtkhdf file: every time point, the whole model as the block Model,
// each set as a block under NodeSets or ElementSets, the provenance in each.
CASE(a_series_is_written_as_one_vtkhdf_file_with_its_sets_as_blocks)
{
    const ResultsSeries s = withSets("bar-increments/bar.frd", "bar-increments/bar.inp");
    QTemporaryDir dir;
    QString error;
    const QString path = dir.filePath("bar.vtkhdf");
    CHECK(gf::core::writeVtkHdf(s, path, &error));
    if (!error.isEmpty())
        std::printf("  (%s)\n", qPrintable(error));

    vtkNew<vtkHDFReader> reader;
    reader->SetFileName(qPrintable(path));
    reader->UpdateInformation();
    vtkInformation *info = reader->GetOutputInformation(0);
    const int steps = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    const double *times = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    CHECK(steps == 4);
    for (int k = 0; times && k < steps && k < 4; ++k)
        CHECK(times[k] == 0.25 * (k + 1));

    reader->UpdateTimeStep(0.75);
    auto *c = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));
    vtkDataAssembly *a = c ? c->GetDataAssembly() : nullptr;
    CHECK(a);
    auto block = [&](const char *path) -> vtkUnstructuredGrid * {
        if (!a)
            return nullptr;
        const int node = a->GetFirstNodeByPath(
            (QString("/") + a->GetRootNodeName() + "/" + path).toUtf8().constData());
        const auto sets = node < 0 ? std::vector<unsigned int>{} : a->GetDataSetIndices(node, false);
        return sets.size() == 1 ? vtkUnstructuredGrid::SafeDownCast(c->GetPartition(sets[0], 0))
                                : nullptr;
    };
    vtkUnstructuredGrid *model = block("Model");
    vtkUnstructuredGrid *pulled = block("NodeSets/PULLED");
    vtkUnstructuredGrid *eall = block("ElementSets/EALL");
    CHECK(model && model->GetNumberOfPoints() == 20 && model->GetNumberOfCells() == 4);
    CHECK(value(model, "DISP", 20, 2) == 0.075);
    CHECK(pulled && pulled->GetNumberOfPoints() == 4 && pulled->GetNumberOfCells() == 4);
    for (vtkIdType k = 0; pulled && k < pulled->GetNumberOfCells(); ++k)
        CHECK(pulled->GetCellType(k) == VTK_VERTEX);
    CHECK(value(pulled, "DISP", 17, 2) == 0.075);
    CHECK(value(pulled, "von Mises stress (derived)", 17, 0) > 0);
    CHECK(eall && eall->GetNumberOfCells() == 4 && misplacedCells(eall) == 0);
    for (vtkUnstructuredGrid *g : {model, pulled, eall})
        CHECK(fieldStrings(g, "provenance") == s.provenance);
}

CASE(a_vtkhdf_file_that_cannot_be_written_is_refused_by_name)
{
    QString error;
    CHECK(!gf::core::writeVtkHdf(increments(), "/proc/no-such-dir/bar.vtkhdf", &error));
    CHECK(error.contains("/proc/no-such-dir/bar.vtkhdf"));
}

// What ParaView is given matches what cgx shows, node by node: the solver's
// displacements and stresses, and the derived von Mises and principal
// stresses against cgx's own (fixtures */cgx-reference.txt, written by
// cgx-reference.py with cgx 2.21; six decimals, as cgx prints them).
CASE(the_numbers_match_what_cgx_shows)
{
    const char *fixtures[] = {"bar-tension/bar.frd",     "bar-binary/bar.frd",
                              "bar-increments/bar.frd",  "cantilever-te10/cantilever.frd",
                              "cube-he20/cube.frd",      "cube-te4/cube.frd",
                              "cube-pe6/cube.frd",       "cube-pe15/cube.frd",
                              "shell-s3/strip.frd",      "shell-s4/strip.frd",
                              "shell-s6/strip.frd",      "shell-s8/strip.frd",
                              "shell-s8-3d/strip.frd",   "plane-cps4/strip.frd",
                              "beam-b31/strip.frd",      "beam-b32r/strip.frd"};
    // GridForge's array and component for each of cgx's columns.
    const std::map<QString, std::pair<const char *, int>> column = {
        {"D1", {"DISP", 0}},    {"D2", {"DISP", 1}},    {"D3", {"DISP", 2}},
        {"SXX", {"STRESS", 0}}, {"SYY", {"STRESS", 1}}, {"SZZ", {"STRESS", 2}},
        {"SXY", {"STRESS", 3}}, {"SYZ", {"STRESS", 4}}, {"SZX", {"STRESS", 5}},
        {"Mises", {"von Mises stress (derived)", 0}},
        {"PS1", {"principal stresses (derived)", 0}},
        {"PS2", {"principal stresses (derived)", 1}},
        {"PS3", {"principal stresses (derived)", 2}}};
    for (const char *frd : fixtures) {
        const ResultsSeries s = convert(frd);
        vtkUnstructuredGrid *g = s.points.empty() ? nullptr : s.points.back().grid.Get();
        QFile ref(QFileInfo(fixture(frd)).dir().filePath("cgx-reference.txt"));
        CHECK(g && ref.open(QIODevice::ReadOnly | QIODevice::Text));
        QStringList header;
        int rows = 0, off = 0;
        double worst = 0;
        while (g && !ref.atEnd()) {
            const QStringList f = QString(ref.readLine()).split(' ', Qt::SkipEmptyParts);
            if (f.isEmpty() || f[0].startsWith('#'))
                continue;
            if (header.isEmpty()) {
                header = f;
                for (QString &h : header)
                    h = h.trimmed();
                continue;
            }
            ++rows;
            // cgx derives in single precision, so an eigenvalue it shows is
            // off by a few float epsilons of the tensor's size, not of its
            // own (uniaxial: PS2 = 0 shown as 0.000001 at 210 MPa).
            double size = 0;
            for (qsizetype c = 1; c < f.size() && c < header.size(); ++c)
                if (header[c] == "Mises" || header[c].startsWith("PS"))
                    size = std::max(size, std::abs(f[c].toDouble()));
            for (qsizetype c = 1; c < f.size() && c < header.size(); ++c) {
                const auto it = column.find(header[c]);
                const double want = f[c].toDouble();
                const double got = it == column.end()
                                       ? NAN
                                       : value(g, it->second.first, f[0].toInt(), it->second.second);
                const bool derived = header[c] == "Mises" || header[c].startsWith("PS");
                // Six decimals as printed; derived values within 4 float
                // epsilons of the tensor's size.
                const double tolerance = 5.1e-7 + (derived ? 4.8e-7 * size : 1e-6 * std::abs(want));
                const double diff = std::abs(got - want);
                worst = std::max(worst, diff / tolerance);
                if (!(diff <= tolerance)) {
                    if (off < 6)
                        std::printf("    node %s %s: cgx %s, GridForge %.9g\n", qPrintable(f[0]),
                                    qPrintable(header[c]), qPrintable(f[c].trimmed()), got);
                    ++off;
                }
            }
        }
        std::printf("  %s: %d nodes, %d values off, worst at %.2g of the tolerance\n", frd, rows, off, worst);
        CHECK(rows == g->GetNumberOfPoints() && header.size() == 14 && off == 0);
    }
}

CASE(a_series_is_written_as_vtu_files_and_a_pvd_that_lists_them)
{
    QTemporaryDir dir;
    QString error;
    const QStringList written = gf::core::writeVtuSeries(increments(), dir.path(), "bar", &error);
    CHECK(error.isEmpty());
    if (!error.isEmpty())
        std::printf("  (%s)\n", qPrintable(error));
    const QStringList names = {"bar.pvd", "bar_0001.vtu", "bar_0002.vtu", "bar_0003.vtu",
                               "bar_0004.vtu"};
    for (const QString &n : names)
        CHECK(written.contains(dir.filePath(n)) && QFile::exists(dir.filePath(n)));

    QFile pvd(dir.filePath("bar.pvd"));
    QStringList files, times;
    if (pvd.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(&pvd);
        while (!xml.atEnd())
            if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == u"DataSet") {
                files << xml.attributes().value("file").toString();
                times << xml.attributes().value("timestep").toString();
            }
        CHECK(!xml.hasError());
    }
    CHECK(files == names.mid(1));
    CHECK((times == QStringList{"0.25", "0.5", "0.75", "1"}));

    vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(qPrintable(dir.filePath("bar_0003.vtu")));
    reader->Update();
    vtkUnstructuredGrid *g = reader->GetOutput();
    CHECK(g->GetNumberOfPoints() == 20 && g->GetNumberOfCells() == 4);
    CHECK(value(g, "DISP", 20, 2) == 0.075);
    CHECK(fieldStrings(g, "provenance") == increments().provenance);
}

// The writer puts the provenance in every file itself, whatever the back
// end left on the grid.
CASE(the_writer_puts_the_provenance_in_every_file)
{
    vtkUnstructuredGrid *source = grid(bar());
    ResultsSeries s;
    for (double time : {1.0, 2.0}) {
        gf::core::TimePoint t;
        t.time = time;
        t.grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        t.grid->CopyStructure(source);
        s.points.push_back(t);
    }
    s.provenance = QStringList{"solver: none", "source: made in a test"};
    QTemporaryDir dir;
    QString error;
    gf::core::writeVtuSeries(s, dir.path(), "plain", &error);
    CHECK(error.isEmpty());
    for (const char *name : {"plain_0001.vtu", "plain_0002.vtu"}) {
        vtkNew<vtkXMLUnstructuredGridReader> reader;
        reader->SetFileName(qPrintable(dir.filePath(name)));
        reader->Update();
        CHECK(fieldStrings(reader->GetOutput(), "provenance") == s.provenance);
    }
}

CASE(a_folder_that_cannot_be_written_is_refused_by_name)
{
    QString error;
    const QStringList written =
        gf::core::writeVtuSeries(bar(), "/proc/gridforge-cannot-write-here", "bar", &error);
    CHECK(written.isEmpty());
    CHECK(error.contains("/proc/gridforge-cannot-write-here"));
}

int main()
{
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d checks failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
