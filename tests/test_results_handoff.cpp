// The hand-off to ParaView: .frd results as VTK grids, then as .vtu files
// with a .pvd time series. The grids must keep ccx's numbers and names,
// put every node where VTK's cell definition expects it, say what was
// derived, and carry their provenance into the files.

#include "calculix/FrdToVtk.h"

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
#include <vtkXMLUnstructuredGridReader.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
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
        const int corners = cell->GetCellType() == VTK_QUADRATIC_TETRA ? 4 : cell->GetNumberOfPoints();
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
    f.elements[0].type = FrdElementType::Pe6;
    QString error;
    const ResultsSeries s = toSeries(f, "bar.frd", &error);
    CHECK(s.points.empty());
    CHECK(error.contains("pe6") && error.contains("element 1"));
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
