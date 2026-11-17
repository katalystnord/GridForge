// GridForge's mesh through CalculiX and back: the box meshed by gf_mesh,
// written by the deck writer (fixtures/calculix/box-gfmesh/mesh.inp),
// solved by ccx 2.23 in a deck written by hand (tension.inp), and read back
// by the .frd reader. Uniaxial tension, so the answer is known by hand at
// every node (tension.inp says how).

#include "CellChecks.h"
#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"
#include "calculix/InpWriter.h"
#include "mesh/Mesher.h"

#include <QFile>
#include <QFileInfo>

#include <vtkPointData.h>

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

QString fixture(const char *file) { return QString(GF_FIXTURES) + "/calculix/box-gfmesh/" + file; }

const ResultsSeries &results()
{
    static const ResultsSeries s = [] {
        QString e;
        ResultsSeries r = toSeries(readFrd(fixture("tension.frd")), "tension.frd", &e);
        if (!e.isEmpty())
            std::printf("  (tension.frd: %s)\n", qPrintable(e));
        return r;
    }();
    return s;
}

} // namespace

CASE(ccx_took_the_mesh_as_written)
{
    const ResultsSeries &s = results();
    CHECK(s.points.size() == 1);
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    // The 229 quadratic tetrahedra and 508 nodes of mesh.inp, in place.
    CHECK(g->GetNumberOfCells() == 229 && g->GetNumberOfPoints() == 508);
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c)
        CHECK(g->GetCellType(c) == VTK_QUADRATIC_TETRA);
    CHECK(std::abs(gf::test::enclosedVolume(g) - 6000) < 1e-9);
    // Each node where GridForge put it, to the six digits the .frd keeps,
    // under the id the deck gave it.
    QString error;
    const ResultsSeries mesh = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", {5, 2}, &error);
    CHECK(!mesh.points.empty() && mesh.points.front().grid->GetNumberOfPoints() == 508);
    if (mesh.points.empty())
        return;
    vtkUnstructuredGrid *m = mesh.points.front().grid;
    vtkDataArray *ids = g->GetPointData()->GetArray("node id");
    double worst = 0;
    for (vtkIdType p = 0; ids && p < g->GetNumberOfPoints(); ++p) {
        const auto a = gf::test::point(g, p), b = gf::test::point(m, static_cast<vtkIdType>(ids->GetTuple1(p)) - 1);
        for (int d = 0; d < 3; ++d)
            worst = std::max(worst, std::abs(a[d] - b[d]));
    }
    CHECK(ids && worst < 5e-5);  // 30 written as 3.00000E+01: half a unit in the sixth digit
}

CASE(every_node_moves_as_by_hand)
{
    const ResultsSeries &s = results();
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *u = g->GetPointData()->GetArray("DISP");
    CHECK(u);
    if (!u)
        return;
    // ux = 0.001 x, uy = -0.0003 y, uz = -0.0003 z; the .frd keeps six
    // digits (largest value 0.01, so 5e-8 at worst).
    double worst = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        const auto x = gf::test::point(g, p);
        worst = std::max({worst, std::abs(u->GetComponent(p, 0) - 0.001 * x[0]),
                          std::abs(u->GetComponent(p, 1) + 0.0003 * x[1]),
                          std::abs(u->GetComponent(p, 2) + 0.0003 * x[2])});
    }
    CHECK(worst < 5e-8);
}

CASE(the_stress_is_210_in_x_and_nothing_else_everywhere)
{
    // To 1e-4, as the other fixtures: ccx computes 210 at the integration
    // points and writes 209.998 at the nodes, extrapolated (bar-tension's
    // README).
    const ResultsSeries &s = results();
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *stress = g->GetPointData()->GetArray("STRESS");
    CHECK(stress && stress->GetNumberOfComponents() == 6);
    if (!stress)
        return;
    double worstX = 0, worstOther = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        worstX = std::max(worstX, std::abs(stress->GetComponent(p, 0) - 210));
        for (int k = 1; k < 6; ++k)
            worstOther = std::max(worstOther, std::abs(stress->GetComponent(p, k)));
    }
    CHECK(worstX < 210e-4);
    CHECK(worstOther < 1e-3);
}

CASE(the_reaction_on_the_pulled_face_is_126000_newtons)
{
    const ResultsSeries &s = results();
    const InpSets sets = readInpSets(fixture("mesh.inp"));
    const InpSet *face2 = sets.find("Face2", true);
    CHECK(sets.ok() && face2);
    if (s.points.empty() || !face2)
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *force = g->GetPointData()->GetArray("FORC");
    vtkDataArray *ids = g->GetPointData()->GetArray("node id");
    CHECK(force && ids);
    if (!force || !ids)
        return;
    std::vector<bool> on(static_cast<size_t>(ids->GetRange()[1]) + 1, false);
    for (int id : face2->ids)
        if (id >= 0 && static_cast<size_t>(id) < on.size())
            on[id] = true;
    double fx = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p)
        if (on[static_cast<size_t>(ids->GetTuple1(p))])
            fx += force->GetComponent(p, 0);
    CHECK(std::abs(fx - 126000) < 1e-3 * 126000);
}

CASE(the_committed_mesh_is_what_gridforge_writes_today)
{
    // mesh.inp is GridForge's own output, kept because ccx solved it. The
    // same geometry and recipe must still give it, line for line, all but
    // the first (the build that wrote it).
    QString error;
    const ResultsSeries mesh = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", {5, 2}, &error);
    CHECK(error.isEmpty());
    const QStringList today = meshInp(mesh, &error).split('\n');
    QFile f(fixture("mesh.inp"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QStringList kept = QString::fromUtf8(f.readAll()).split('\n');
    CHECK(today.size() == kept.size());
    CHECK(today.mid(1) == kept.mid(1));
}

int main()
{
    for (auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
