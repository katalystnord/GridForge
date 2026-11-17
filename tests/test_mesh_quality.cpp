// The mesh quality report against tetrahedra whose measures are known by
// hand, then against meshes from gf_mesh.
//
//   regular tetrahedron   mean ratio 1; dihedral angle acos(1/3) = 70.5288
//   corner tetrahedron    (0,0,0) (1,0,0) (0,1,0) (0,0,1): volume 1/6, the
//                         squared edges 1+1+1+2+2+2 = 9, so the mean ratio
//                         is 12 (1/2)^(2/3) / 9 = 0.8399474; its smallest
//                         dihedral angle is at the slanted face,
//                         acos(1/sqrt 3) = 54.7356
//   a quadratic edge      its middle node at a fraction m along the edge
//                         keeps the Jacobian positive for 1/4 < m < 3/4

#include "mesh/Mesher.h"
#include "mesh/Quality.h"

#include <QJsonArray>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::mesh;

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

const double pi = 3.14159265358979323846;
const double regularDihedral = std::acos(1.0 / 3.0) * 180 / pi;
const double cornerRatio = 12 * std::pow(0.5, 2.0 / 3.0) / 9;
const double cornerDihedral = std::acos(1 / std::sqrt(3.0)) * 180 / pi;

using P = std::array<double, 3>;

// A grid of the cells given, each a list of points (4 or 10), numbered
// 101, 102, ... as "element id".
vtkSmartPointer<vtkUnstructuredGrid> grid(const std::vector<std::vector<P>> &cells,
                                          int extraType = 0)
{
    auto g = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkNew<vtkPoints> points;
    vtkNew<vtkIntArray> ids;
    ids->SetName("element id");
    for (const auto &c : cells) {
        std::vector<vtkIdType> pts;
        for (const P &p : c)
            pts.push_back(points->InsertNextPoint(p.data()));
        const int type = c.size() == 10 ? VTK_QUADRATIC_TETRA : c.size() == 8 ? VTK_HEXAHEDRON : VTK_TETRA;
        g->InsertNextCell(extraType ? extraType : type, static_cast<vtkIdType>(pts.size()), pts.data());
        ids->InsertNextValue(101 + static_cast<int>(g->GetNumberOfCells()) - 1);
    }
    g->SetPoints(points);
    g->GetCellData()->AddArray(ids);
    return g;
}

const std::vector<P> regular = {{1, 1, 1}, {-1, 1, -1}, {1, -1, -1}, {-1, -1, 1}};
const std::vector<P> corner = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

// The corner tetrahedron, quadratic, the middle node of edge 01 at m.
std::vector<P> quadraticCorner(double m)
{
    return {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
            {m, 0, 0}, {.5, .5, 0}, {0, .5, 0}, {0, 0, .5}, {.5, 0, .5}, {0, .5, .5}};
}

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }

} // namespace

CASE(the_regular_tetrahedron_measures_one)
{
    const Quality q = measure(grid({regular}));
    CHECK(q.tetrahedra == 1);
    CHECK(near(q.minMeanRatio, 1, 1e-12));
    CHECK(near(q.minDihedral, regularDihedral));
    CHECK(q.inverted == 0 && q.folded == 0);
    CHECK(q.solverReady());
}

CASE(the_corner_tetrahedron_measures_as_by_hand)
{
    const Quality q = measure(grid({corner}));
    CHECK(near(q.minMeanRatio, cornerRatio, 1e-12));
    CHECK(near(q.averageMeanRatio, cornerRatio, 1e-12));
    CHECK(near(q.minDihedral, cornerDihedral));
    CHECK(q.meanRatio.size() == 1 && near(q.meanRatio[0], cornerRatio, 1e-12));
}

CASE(a_tetrahedron_turned_the_wrong_way_is_inverted)
{
    const Quality q = measure(grid({{corner[0], corner[2], corner[1], corner[3]}, regular}));
    CHECK(q.inverted == 1);
    CHECK(!q.solverReady());
}

CASE(a_flat_tetrahedron_is_inverted_and_measures_zero)
{
    const Quality q = measure(grid({{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}}));
    CHECK(q.inverted == 1);
    CHECK(near(q.minMeanRatio, 0, 1e-12));
    CHECK(q.meanRatioBelow01 == 1 && q.meanRatioBelow03 == 1);
}

CASE(a_middle_node_pushed_too_far_folds_the_element)
{
    CHECK(measure(grid({quadraticCorner(0.5)})).folded == 0);
    CHECK(measure(grid({quadraticCorner(0.6)})).folded == 0);
    CHECK(measure(grid({quadraticCorner(0.7)})).folded == 0);
    const Quality q = measure(grid({quadraticCorner(0.8)}));
    CHECK(q.folded == 1);
    CHECK(q.inverted == 0);  // its corners are fine
    CHECK(!q.solverReady());
    CHECK(measure(grid({quadraticCorner(0.2)})).folded == 1);
    // The corners alone set the shape measures.
    CHECK(near(q.minMeanRatio, cornerRatio, 1e-12));
}

CASE(an_element_folded_only_at_its_centre_is_folded)
{
    // Every middle node pulled past the centre of the corner tetrahedron,
    // 1.2 times the way from its edge's middle: the Jacobian is positive
    // at all ten nodes (0.28 at least) and negative at the centre
    // (-0.008), found by solving for it beforehand.
    std::vector<P> cell = quadraticCorner(0.5);
    for (int k = 4; k < 10; ++k)
        for (int d = 0; d < 3; ++d)
            cell[k][d] += 1.2 * (0.25 - cell[k][d]);
    CHECK(measure(grid({cell})).folded == 1);
}

CASE(cells_other_than_tetrahedra_are_counted_not_measured)
{
    const Quality q = measure(grid({{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                     {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
                                    corner}));
    CHECK(q.tetrahedra == 1 && q.notMeasured == 1);
    CHECK(q.meanRatio.size() == 2 && q.meanRatio[0] == -1);
}

CASE(the_worst_cells_come_first)
{
    // flat: mean ratio about 0.04; low (height 0.05): 0.192.
    const std::vector<P> flat = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {.3, .3, .01}};
    const std::vector<P> low = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {.3, .3, .05}};
    const Quality q = measure(grid({regular, flat, corner, low}));
    CHECK(q.worst.size() == 4);
    CHECK(q.worst.size() == 4 && q.worst[0] == 1 && q.worst[1] == 3 && q.worst[2] == 2
          && q.worst[3] == 0);
    CHECK(q.meanRatioBelow03 == 2 && q.meanRatioBelow01 == 1);
}

CASE(the_report_for_agents_names_cells_by_their_element_id)
{
    auto g = grid({regular, {corner[0], corner[2], corner[1], corner[3]}});
    const QJsonObject j = toJson(measure(g), g);
    CHECK(j["format"].toString() == "gridforge-mesh-quality");
    CHECK(j["version"].toString() == "1.0");
    CHECK(j["solver ready"].toBool() == false);
    CHECK(j["tetrahedra"].toInteger() == 2);
    CHECK(j["inverted"].toInteger() == 1);
    CHECK(j["folded"].toInteger() == 0);
    CHECK(j["not measured"].toInteger() == 0);
    CHECK(near(j["mean ratio"].toObject()["min"].toDouble(), cornerRatio, 1e-12));
    CHECK(near(j["mean ratio"].toObject()["average"].toDouble(), (1 + cornerRatio) / 2, 1e-12));
    CHECK(j["mean ratio"].toObject()["below 0.1"].toInteger() == 0);
    CHECK(near(j["smallest dihedral angle"].toDouble(), cornerDihedral));
    const QJsonArray worst = j["worst"].toArray();
    CHECK(worst.size() == 2 && worst[0].toObject()["element id"].toInteger() == 102);
    CHECK(worst.size() == 2 && near(worst[0].toObject()["mean ratio"].toDouble(), cornerRatio, 1e-12));
}

CASE(the_report_for_people_gives_the_verdict_first)
{
    const QStringList ok = summary(measure(grid({regular, corner})));
    CHECK(ok.value(0) == "ready for a solver: no inverted or folded elements");
    CHECK(ok.value(1) == "2 tetrahedra; mean ratio 0.840 at worst, 0.920 on average; "
                         "smallest dihedral angle 54.7 degrees");
    CHECK(ok.value(2) == "below 0.3: none; below 0.1: none");
    const QStringList bad = summary(measure(grid({{corner[0], corner[2], corner[1], corner[3]},
                                                  quadraticCorner(0.8)})));
    CHECK(bad.value(0) == "NOT ready for a solver: 1 inverted element, 1 folded element");
}

CASE(netgens_meshes_are_ready_for_a_solver)
{
    for (const char *file : {"box.step", "dogbone.step", "twoboxes.step"})
        for (int order : {1, 2}) {
            QString error;
            const auto s = meshStep(QString(GF_FIXTURES) + "/step/" + file, Recipe{2, order}, &error);
            CHECK(error.isEmpty() && !s.points.empty());
            if (s.points.empty())
                continue;
            vtkUnstructuredGrid *g = s.points.front().grid;
            const Quality q = measure(g);
            CHECK(q.solverReady());
            CHECK(q.tetrahedra == g->GetNumberOfCells() && q.notMeasured == 0);
            CHECK(q.minMeanRatio > 0.1);
        }
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
