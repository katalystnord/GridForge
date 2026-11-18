// gf_mesh: STEP geometry meshed with tetrahedra through Netgen, handed on
// as a grid with the geometry's solids and faces as sets. The fixtures'
// answers are known without any mesher (fixtures/step/README.md): the box
// is 10 x 20 x 30 mm, the two boxes 1000 mm3 each, the dogbone's volume is
// OpenCASCADE's.

#include "CellChecks.h"
#include "mesh/Mesher.h"
#include "mesh/Surface.h"

#include <QJsonObject>
#include <QRegularExpression>

#include <vtkFieldData.h>
#include <vtkStringArray.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>
#include <thread>
#include <vector>

using namespace gf::mesh;
using namespace gf::test;
using gf::core::NamedSet;
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

const double dogboneVolume = 8699.688877463912;

ResultsSeries mesh(const char *file, double size, int order, QString *error = nullptr)
{
    QString e;
    ResultsSeries s = meshStep(QString(GF_FIXTURES) + "/step/" + file, Recipe{size, order}, &e);
    if (!e.isEmpty() && !error)
        std::printf("  (%s: %s)\n", file, qPrintable(e));
    if (error)
        *error = e;
    return s;
}

vtkUnstructuredGrid *grid(const ResultsSeries &s)
{
    return s.points.empty() ? nullptr : s.points.front().grid.Get();
}

const NamedSet *setOf(const ResultsSeries &s, const QString &name, bool nodes)
{
    for (const NamedSet &n : s.sets)
        if (n.name == name && n.nodes == nodes)
            return &n;
    return nullptr;
}

bool allCellsAre(vtkUnstructuredGrid *g, int type)
{
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c)
        if (g->GetCellType(c) != type)
            return false;
    return g->GetNumberOfCells() > 0;
}

double relative(double a, double b) { return std::abs(a - b) / std::abs(b); }

// The volume of some cells of a grid only.
double volumeOf(vtkUnstructuredGrid *g, const std::vector<vtkIdType> &cells)
{
    auto part = vtkSmartPointer<vtkUnstructuredGrid>::New();
    part->SetPoints(g->GetPoints());
    part->Allocate(static_cast<vtkIdType>(cells.size()));
    for (vtkIdType c : cells)
        part->InsertNextCell(g->GetCellType(c), g->GetCell(c)->GetPointIds());
    return enclosedVolume(part);
}

} // namespace

CASE(a_box_is_filled_with_tetrahedra_to_its_volume)
{
    const ResultsSeries s = mesh("box.step", 3, 1);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    CHECK(s.points.size() == 1);
    CHECK(allCellsAre(g, VTK_TETRA));
    CHECK(misplacedCells(g) == 0);
    CHECK(relative(enclosedVolume(g), 6000) < 1e-12);
    double b[6];
    g->GetBounds(b);
    CHECK(std::abs(b[0]) < 1e-12 && std::abs(b[1] - 10) < 1e-12);
    CHECK(std::abs(b[2]) < 1e-12 && std::abs(b[3] - 20) < 1e-12);
    CHECK(std::abs(b[4]) < 1e-12 && std::abs(b[5] - 30) < 1e-12);
}

CASE(halving_the_size_gives_about_eight_times_the_tetrahedra)
{
    // The recipe's size is a target for Netgen, not a hard bound (at 3 its
    // longest edge in the box is 6.1): what it must do is set the
    // fineness. Half the edge length is about eight times the cells.
    const ResultsSeries coarse = mesh("box.step", 3, 1), fine = mesh("box.step", 1.5, 1);
    vtkUnstructuredGrid *c = grid(coarse), *f = grid(fine);
    CHECK(c && f);
    if (!c || !f)
        return;
    const double ratio = double(f->GetNumberOfCells()) / double(c->GetNumberOfCells());
    CHECK(ratio > 5 && ratio < 12);
    CHECK(relative(enclosedVolume(f), 6000) < 1e-12);
}

CASE(quadratic_tetrahedra_have_their_middle_nodes_in_vtks_order)
{
    const ResultsSeries s = mesh("box.step", 3, 2);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    CHECK(allCellsAre(g, VTK_QUADRATIC_TETRA));
    // The box is flat-faced: every middle node sits halfway along its edge.
    CHECK(misplacedCells(g) == 0);
    CHECK(relative(enclosedVolume(g), 6000) < 1e-12);
}

CASE(a_curved_part_is_filled_close_to_its_volume)
{
    const ResultsSeries s = mesh("dogbone.step", 2, 1);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    CHECK(misplacedCells(g) == 0);
    // Straight edges cut across the R30 transitions, which are concave:
    // a little volume is gained (3.2e-4 at this size), never a thousandth.
    const double v = enclosedVolume(g);
    CHECK(v > dogboneVolume);
    CHECK(relative(v, dogboneVolume) < 1e-3);
}

CASE(every_node_handed_on_belongs_to_an_element)
{
    // Netgen keeps points no element uses: in the dogbone, the centres of
    // the four R30 arcs, at y = -35 and 35, outside the part. They must not
    // reach the grid (they would stretch its bounds, and every node set).
    for (int order : {1, 2}) {
        const ResultsSeries s = mesh("dogbone.step", 2, order);
        vtkUnstructuredGrid *g = grid(s);
        CHECK(g);
        if (!g)
            continue;
        std::vector<bool> used(g->GetNumberOfPoints(), false);
        for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
            vtkIdType n;
            const vtkIdType *pts;
            g->GetCellPoints(c, n, pts);
            for (vtkIdType k = 0; k < n; ++k)
                used[pts[k]] = true;
        }
        CHECK(std::count(used.begin(), used.end(), false) == 0);
        double b[6];
        g->GetBounds(b);
        CHECK(b[2] >= -10 - 1e-9 && b[3] <= 10 + 1e-9);
    }
}

CASE(quadratic_middle_nodes_are_moved_onto_curved_faces)
{
    const ResultsSeries s = mesh("dogbone.step", 2, 2);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    CHECK(allCellsAre(g, VTK_QUADRATIC_TETRA));
    if (!allCellsAre(g, VTK_QUADRATIC_TETRA))
        return;
    // On the flat faces middle nodes stay halfway; on the transitions they
    // leave the straight edge for the arc, by up to the arc's sagitta.
    // VTK's edges: 01, 12, 20, 03, 13, 23 as nodes 4 to 9.
    const int ends[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
    int moved = 0;
    double furthest = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        vtkCell *cell = g->GetCell(c);
        for (int k = 0; k < 6; ++k) {
            const auto a = point(g, cell->GetPointId(ends[k][0])),
                       b = point(g, cell->GetPointId(ends[k][1])),
                       m = point(g, cell->GetPointId(4 + k));
            const double off = std::hypot(m[0] - (a[0] + b[0]) / 2, m[1] - (a[1] + b[1]) / 2,
                                          m[2] - (a[2] + b[2]) / 2);
            moved += off > 1e-9 ? 1 : 0;
            furthest = std::max(furthest, off);
        }
    }
    CHECK(moved > 0);
    // An edge of about 2 mm on R30 bows by about 2^2 / (8 * 30) = 0.017 mm.
    CHECK(furthest < 0.1);
}

CASE(each_solid_is_an_element_set_and_a_node_set)
{
    const ResultsSeries s = mesh("twoboxes.step", 2, 1);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    const NamedSet *one = setOf(s, "Solid1", false), *two = setOf(s, "Solid2", false);
    CHECK(one && two);
    CHECK(setOf(s, "Solid1", true) && setOf(s, "Solid2", true));
    CHECK(!setOf(s, "Solid3", false));
    if (!one || !two)
        return;
    CHECK(one->members.size() + two->members.size() == static_cast<size_t>(g->GetNumberOfCells()));
    CHECK(relative(volumeOf(g, one->members), 1000) < 1e-12);
    CHECK(relative(volumeOf(g, two->members), 1000) < 1e-12);
    // In the file's order: the first solid is the one at x 0 to 10.
    double b[6];
    g->GetCell(one->members.front())->GetBounds(b);
    CHECK(b[1] <= 10 + 1e-12);
}

CASE(every_root_of_a_step_file_is_meshed)
{
    // Two cubes apart, each its own root in the file (tworoots.step):
    // Netgen's reader keeps only the first root, and the second cube was
    // lost without a word (found 2026-11-18). Both are meshed, in the
    // file's order.
    QString error;
    const ResultsSeries s = mesh("tworoots.step", 5, 1, &error);
    CHECK(error.isEmpty());
    vtkUnstructuredGrid *g = grid(s);
    const NamedSet *one = setOf(s, "Solid1", false), *two = setOf(s, "Solid2", false);
    CHECK(g && one && two);
    if (!g || !one || !two)
        return;
    CHECK(relative(volumeOf(g, one->members), 1000) < 1e-12);
    CHECK(relative(volumeOf(g, two->members), 1000) < 1e-12);
    double b[6];
    g->GetCell(two->members.front())->GetBounds(b);
    CHECK(b[0] >= 20 - 1e-12);
}

// How many tetrahedra have every corner on the outer surface: in a thin
// wall, those span the whole thickness alone.
std::pair<vtkIdType, vtkIdType> allOnSurface(vtkUnstructuredGrid *g)
{
    std::map<std::array<vtkIdType, 3>, int> faces;
    const int corner[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
    vtkNew<vtkIdList> ids;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        g->GetCellPoints(c, ids);
        for (const auto &f : corner) {
            std::array<vtkIdType, 3> k{ids->GetId(f[0]), ids->GetId(f[1]), ids->GetId(f[2])};
            std::sort(k.begin(), k.end());
            ++faces[k];
        }
    }
    std::set<vtkIdType> outer;
    for (const auto &[k, n] : faces)
        if (n == 1)
            outer.insert(k.begin(), k.end());
    vtkIdType all = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        g->GetCellPoints(c, ids);
        all += outer.count(ids->GetId(0)) && outer.count(ids->GetId(1)) && outer.count(ids->GetId(2))
               && outer.count(ids->GetId(3));
    }
    return {all, g->GetNumberOfCells()};
}

CASE(a_thin_wall_gets_elements_through_its_thickness)
{
    // A plate 1.5 mm thick (plate.step). At a largest element of 5 mm,
    // Netgen alone puts one tetrahedron through the thickness almost
    // everywhere (on the benchmark's open box, 4854 of 4862 with every
    // corner on the surface, measured 2026-11-18): a wall that bends badly.
    QString error;
    const QString plate = QString(GF_FIXTURES) + "/step/plate.step";
    const ResultsSeries alone = meshStep(plate, Recipe{5, 1, 0}, &error);
    CHECK(error.isEmpty() && grid(alone));
    if (!grid(alone))
        return;
    const auto [was, wasOf] = allOnSurface(grid(alone));
    CHECK(was > 0.9 * wasOf);
    CHECK(!alone.provenance.join('\n').contains("thin walls:"));
    // With two elements through a wall (the recipe's default): the top and
    // bottom measured 1.5 mm apart, the sides 20 and 10 (along x, along
    // y), and the top and bottom made finer.
    const ResultsSeries s = meshStep(plate, Recipe{5, 1}, &error);
    CHECK(error.isEmpty() && grid(s));
    if (!grid(s))
        return;
    vtkDataArray *t = grid(s)->GetFieldData()->GetArray("face thickness");
    CHECK(t && t->GetNumberOfTuples() == 6);
    std::multiset<long> measured;
    for (vtkIdType f = 0; t && f < t->GetNumberOfTuples(); ++f) {
        const double d = t->GetComponent(f, 0);
        measured.insert(std::lround(d * 10));
        CHECK(std::abs(d * 10 - std::lround(d * 10)) < 1e-5);
    }
    CHECK((measured == std::multiset<long>{15, 15, 100, 100, 200, 200}));
    CHECK(s.provenance.contains("thin walls: 2 faces 1.5 thick, sized 0.5 for 2 elements through"));
    const auto [now, nowOf] = allOnSurface(grid(s));
    std::printf("  (plate: %lld of %lld through alone, %lld of %lld now)\n", static_cast<long long>(was),
                static_cast<long long>(wasOf), static_cast<long long>(now), static_cast<long long>(nowOf));
    CHECK(now < 0.25 * nowOf);
    // A box thick for its size (10 mm at 5): nothing made finer.
    const ResultsSeries box = mesh("box.step", 5, 2);
    CHECK(!box.provenance.join('\n').contains("thin walls:"));
}

CASE(a_mesh_steps_parameters_are_its_recipe)
{
    // As study.json keeps them (study spec 2.1): what is not given, the
    // default.
    Recipe r = recipeFrom(QJsonObject{{"maxSize", 3}, {"order", 2}});
    CHECK(r.maxSize == 3 && r.order == 2 && r.throughWall == 2);
    r = recipeFrom(QJsonObject{{"maxSize", 3}, {"throughWall", 0}});
    CHECK(r.maxSize == 3 && r.order == 1 && r.throughWall == 0);
}

CASE(a_brep_from_freecad_is_meshed_as_its_step)
{
    // The box written by FreeCAD's exportBrep (fixtures/freecad/box.brep):
    // the same six faces as box.step, the same numbers, and its solid's
    // volume and centre, known: 6000 mm3 at (5, 10, 15).
    QString error;
    const ResultsSeries brep =
        meshStep(QString(GF_FIXTURES) + "/freecad/box.brep", Recipe{5, 1}, &error);
    CHECK(error.isEmpty() && grid(brep));
    const ResultsSeries step = mesh("box.step", 5, 1);
    for (int f = 1; f <= 6; ++f) {
        const auto a = faceSignature(brep, f), b = faceSignature(step, f);
        CHECK(a && b && gf::core::sameFace(*b, *a));
    }
    CHECK(brep.provenance.contains("source: box.brep"));
    const auto solid = solidSignature(brep, 1);
    CHECK(solid && std::abs(solid->volume - 6000) < 1e-9);
    CHECK(solid && std::abs(solid->centre[0] - 5) < 1e-9 && std::abs(solid->centre[1] - 10) < 1e-9
          && std::abs(solid->centre[2] - 15) < 1e-9);
    CHECK(!solidSignature(brep, 2));
    // Two cubes side by side: each its own, in the file's order.
    const ResultsSeries two = mesh("twoboxes.step", 5, 1);
    const auto one = solidSignature(two, 1), other = solidSignature(two, 2);
    CHECK(one && other && std::abs(one->volume - 1000) < 1e-9 && std::abs(other->volume - 1000) < 1e-9);
    CHECK(one && other && std::abs(one->centre[0] - 5) < 1e-9 && std::abs(other->centre[0] - 15) < 1e-9);
}

CASE(a_mesh_size_is_suggested_from_the_parts_size)
{
    // A twenty-fifth of the bounding box's diagonal, rounded down to 1, 2
    // or 5 times a power of ten: the box's diagonal is sqrt(10^2 + 20^2 +
    // 30^2) = 37.4, a 25th 1.5, so 1; the dogbone's sqrt(150^2 + 20^2 +
    // 4^2) = 151.4, a 25th 6.06, so 5.
    QString error;
    CHECK(suggestedSize(QString(GF_FIXTURES) + "/freecad/box.brep", &error) == 1 && error.isEmpty());
    CHECK(suggestedSize(QString(GF_FIXTURES) + "/step/dogbone.step", &error) == 5 && error.isEmpty());
    CHECK(suggestedSize(QString(GF_FIXTURES) + "/step/none.step", &error) == 0 && !error.isEmpty());
}

CASE(touching_solids_share_their_nodes_where_they_touch)
{
    // The two boxes touch at x = 10: one face, meshed once, its nodes
    // in both solids. No two nodes anywhere at the same place.
    for (int order : {1, 2}) {
        const ResultsSeries s = mesh("twoboxes.step", 2, order);
        vtkUnstructuredGrid *g = grid(s);
        CHECK(g);
        if (!g)
            continue;
        std::set<std::array<double, 3>> places;
        for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p)
            places.insert(point(g, p));
        CHECK(static_cast<vtkIdType>(places.size()) == g->GetNumberOfPoints());
        CHECK(relative(enclosedVolume(g), 2000) < 1e-12);
        CHECK(misplacedCells(g) == 0);
        const NamedSet *one = setOf(s, "Solid1", true), *two = setOf(s, "Solid2", true);
        CHECK(one && two);
        if (!one || !two)
            continue;
        std::vector<vtkIdType> both;
        std::set_intersection(one->members.begin(), one->members.end(), two->members.begin(),
                              two->members.end(), std::back_inserter(both));
        CHECK(both.size() > 10);
        bool onContact = true;
        for (vtkIdType p : both)
            onContact = onContact && std::abs(point(g, p)[0] - 10) < 1e-12;
        CHECK(onContact);
    }
}

CASE(the_face_two_solids_share_is_one_face_set)
{
    const ResultsSeries s = mesh("twoboxes.step", 2, 1);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    // Twelve faces in the file, eleven once joined.
    CHECK(setOf(s, "Face11", true) && !setOf(s, "Face12", true));
    int contact = 0;
    for (int f = 1; f <= 11; ++f) {
        const NamedSet *face = setOf(s, QString("Face%1").arg(f), true);
        if (!face || face->members.empty())
            continue;
        bool on = true;
        for (vtkIdType p : face->members)
            on = on && std::abs(point(g, p)[0] - 10) < 1e-12;
        contact += on ? 1 : 0;
    }
    CHECK(contact == 1);
    CHECK(s.provenance.contains("joined: 2 solids, sharing 1 face"));
    CHECK(s.provenance.contains("sets: 2 solids, as element sets and node sets; 11 faces, as node sets"));
}

CASE(overlapping_solids_are_refused_and_named)
{
    QString error;
    const ResultsSeries s = mesh("overlapping.step", 2, 1, &error);
    CHECK(s.points.empty());
    CHECK(error == "solids 1 and 2 of overlapping.step overlap; touching solids are joined, "
                   "overlapping ones are a fault in the geometry");
}

// A solid's surface must close: Netgen given an open one freed memory
// twice and ended the program (2026-11-18), the window with it. The open
// edges are found first, and the faces they border named.
CASE(a_solid_whose_surface_does_not_close_is_refused_naming_the_open_faces)
{
    QString error;
    const ResultsSeries s = mesh("openbox.brep", 3, 1, &error);
    CHECK(s.points.empty());
    CHECK(error == "the surface of openbox.brep does not close: 4 edges border one face only, "
                   "on faces 1, 2, 3 and 4; a solid needs a closed surface (a face missing, or "
                   "two faces that do not meet, in the geometry)");
}

// A seam (a face meeting itself) and a pole (a degenerate edge) border one
// face, and are no opening: a ball's surface closes.
CASE(a_ball_s_seam_and_poles_are_no_opening)
{
    QString error;
    const ResultsSeries s = mesh("ball.step", 3, 1, &error);
    CHECK(error.isEmpty());
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    CHECK(misplacedCells(g) == 0);
    // Measured by the corners: flat faces inside a convex ball, so a
    // little short of it, never more.
    const double v = enclosedVolume(g), ball = 4.0 / 3.0 * M_PI * 1000;
    std::printf("  (ball: %.6g of %.6g)\n", v, ball);
    CHECK(v < ball && relative(v, ball) < 0.05);
}

// A convergence check meshes the same recipe finer by one ratio: every
// size the recipe makes divided by it, the largest element and the
// thin walls' sizes alike (core/Convergence).
CASE(a_recipe_made_finer_divides_every_size_it_makes)
{
    QString e1, e2;
    Recipe halved{6, 1};
    halved.finer = 2;
    const ResultsSeries a = meshStep(QString(GF_FIXTURES) + "/step/box.step", halved, &e1);
    const ResultsSeries b = meshStep(QString(GF_FIXTURES) + "/step/box.step", Recipe{3, 1}, &e2);
    CHECK(e1.isEmpty() && e2.isEmpty());
    CHECK(grid(a) && grid(b));
    if (!grid(a) || !grid(b))
        return;
    CHECK(grid(a)->GetNumberOfPoints() == grid(b)->GetNumberOfPoints());
    CHECK(grid(a)->GetNumberOfCells() == grid(b)->GetNumberOfCells());
    CHECK(a.provenance.contains("recipe: largest element 6, order 1, 2 elements through a wall, every size "
                                "over 2 (finer, for a convergence check)"));
    // The plate's 1.5 mm walls: 0.5 for 2 elements through, over 1.5.
    Recipe plate{5, 1};
    plate.finer = 1.5;
    QString e3;
    const ResultsSeries p = meshStep(QString(GF_FIXTURES) + "/step/plate.step", plate, &e3);
    CHECK(e3.isEmpty());
    CHECK(p.provenance.contains("thin walls: 2 faces 1.5 thick, sized 0.333333 for 2 elements through"));
    // From study.json's parameters too.
    CHECK(recipeFrom(QJsonObject{{"maxSize", 3}, {"finer", 1.5}}).finer == 1.5);
    CHECK(recipeFrom(QJsonObject{{"maxSize", 3}}).finer == 1);
}

CASE(each_face_of_the_geometry_is_a_node_set_on_that_face)
{
    const ResultsSeries s = mesh("box.step", 3, 1);
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g);
    if (!g)
        return;
    // OpenCASCADE's order of the box's faces, as Gmsh 4.14 lists them from
    // the same file: x = 0, x = 10, y = 0, y = 20, z = 0, z = 30.
    const std::pair<int, double> planes[6] = {{0, 0}, {0, 10}, {1, 0}, {1, 20}, {2, 0}, {2, 30}};
    std::set<vtkIdType> all;
    for (int f = 0; f < 6; ++f) {
        const NamedSet *face = setOf(s, QString("Face%1").arg(f + 1), true);
        CHECK(face && face->members.size() > 3);
        if (!face)
            continue;
        bool on = true;
        for (vtkIdType p : face->members)
            on = on && std::abs(point(g, p)[planes[f].first] - planes[f].second) < 1e-12;
        CHECK(on);
        all.insert(face->members.begin(), face->members.end());
    }
    CHECK(!setOf(s, "Face7", true));
    // Together they hold every node on the outside.
    int outside = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        const auto x = point(g, p);
        outside += (x[0] == 0 || x[0] == 10 || x[1] == 0 || x[1] == 20 || x[2] == 0 || x[2] == 30) ? 1 : 0;
    }
    CHECK(static_cast<int>(all.size()) == outside);
}

CASE(the_same_geometry_and_recipe_give_the_same_mesh)
{
    const ResultsSeries a = mesh("dogbone.step", 2, 2), b = mesh("dogbone.step", 2, 2);
    vtkUnstructuredGrid *ga = grid(a), *gb = grid(b);
    CHECK(ga && gb);
    if (!ga || !gb)
        return;
    CHECK(allCellsAre(ga, VTK_QUADRATIC_TETRA) && allCellsAre(gb, VTK_QUADRATIC_TETRA));
    if (!allCellsAre(ga, VTK_QUADRATIC_TETRA) || !allCellsAre(gb, VTK_QUADRATIC_TETRA))
        return;
    CHECK(ga->GetNumberOfPoints() == gb->GetNumberOfPoints());
    CHECK(ga->GetNumberOfCells() == gb->GetNumberOfCells());
    bool same = ga->GetNumberOfPoints() == gb->GetNumberOfPoints()
                && ga->GetNumberOfCells() == gb->GetNumberOfCells();
    for (vtkIdType p = 0; same && p < ga->GetNumberOfPoints(); ++p)
        same = point(ga, p) == point(gb, p);  // bit for bit
    for (vtkIdType c = 0; same && c < ga->GetNumberOfCells(); ++c)
        for (int k = 0; k < 10 && same; ++k)
            same = ga->GetCell(c)->GetPointId(k) == gb->GetCell(c)->GetPointId(k);
    CHECK(same);
}

CASE(meshing_on_two_threads_at_once_is_safe)
{
    // Netgen keeps global state: two meshings at once (a window meshing
    // while another opens) must neither crash nor differ from one alone.
    const ResultsSeries alone = mesh("dogbone.step", 2, 2);
    ResultsSeries a, b;
    std::thread one([&] { a = mesh("dogbone.step", 2, 2); });
    std::thread two([&] { b = mesh("dogbone.step", 2, 2); });
    one.join();
    two.join();
    vtkUnstructuredGrid *g = grid(alone), *ga = grid(a), *gb = grid(b);
    CHECK(g && ga && gb);
    if (!g || !ga || !gb)
        return;
    CHECK(ga->GetNumberOfCells() == g->GetNumberOfCells() && gb->GetNumberOfCells() == g->GetNumberOfCells());
    CHECK(ga->GetNumberOfPoints() == g->GetNumberOfPoints() && gb->GetNumberOfPoints() == g->GetNumberOfPoints());
}

CASE(the_provenance_names_the_engine_the_recipe_and_the_source)
{
    const ResultsSeries s = mesh("box.step", 3, 2);
    const QString all = s.provenance.join('\n');
    CHECK(engine().startsWith("Netgen "));
    CHECK(engine().contains("github.com/katalystnord/netgen at "));
    CHECK(s.provenance.contains("mesher: " + engine()));
    CHECK(s.provenance.contains("source: box.step"));
    CHECK(s.provenance.contains("recipe: largest element 3, order 2, 2 elements through a wall"));
    // Every Netgen setting the mesh depends on, as it was used: set by
    // GridForge, not left to Netgen's defaults, which may move between
    // versions (docs/survey/netgen-review.md, item 4).
    CHECK(s.provenance.contains("settings: grading 0.3, curvature safety 2, segments per edge 1, smallest element 0, "
                                "close edges off, surface optimisation smcmSmcmSmcm x3, volume optimisation "
                                "cmdDmustm x3, 4 threads"));
    // The geometry kernel, whose own triangulation sizes curved faces.
    CHECK(all.contains(QRegularExpression("geometry kernel: OpenCASCADE \\d+\\.\\d+\\.\\d+")));
    CHECK(all.contains(QRegularExpression("model: \\d+ quadratic tetrahedra, \\d+ nodes")));
    CHECK(s.provenance.contains("node order: Netgen's, put into VTK's by GridForge"));
    CHECK(s.provenance.contains("sets: 1 solid, as an element set and a node set; 6 faces, as node sets"));
    // On the grid too, for ParaView.
    vtkUnstructuredGrid *g = grid(s);
    CHECK(g && g->GetFieldData()->GetAbstractArray("provenance"));
}

CASE(a_missing_file_is_said_and_nothing_is_made)
{
    QString error;
    const ResultsSeries s = mesh("no-such.step", 3, 1, &error);
    CHECK(s.points.empty());
    CHECK(error.contains("no-such.step"));
}

CASE(a_recipe_without_a_size_is_refused)
{
    QString error;
    const ResultsSeries s = mesh("box.step", 0, 1, &error);
    CHECK(s.points.empty());
    CHECK(error == "the recipe has no largest element size");
}

CASE(an_order_other_than_1_or_2_is_refused)
{
    QString error;
    const ResultsSeries s = mesh("box.step", 3, 3, &error);
    CHECK(s.points.empty());
    CHECK(error == "the recipe's order is 3; only 1 and 2 are made");
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
