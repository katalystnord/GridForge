// gf_mesh's outer surface: the triangles a picked face is found by. The
// answers are known without a mesher (fixtures/step/README.md): the box is
// 10 x 20 x 30 mm at the origin, so its faces are 200, 200, 300, 300, 600
// and 600 mm2, each flat on a plane x, y or z = 0 or its length; the two
// boxes are 10 mm cubes touching at x = 10, which is inside, not outer.

#include "mesh/Mesher.h"
#include "mesh/Surface.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkIdList.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkTriangle.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <vector>

using namespace gf::mesh;
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
#define CASE(name)                              \
    void name();                                \
    static Register reg_##name(#name, name);    \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

ResultsSeries mesh(const char *file, int order)
{
    QString e;
    ResultsSeries s = meshStep(QString(GF_FIXTURES) + "/step/" + file, Recipe{5, order}, &e);
    if (!e.isEmpty())
        std::printf("  (%s: %s)\n", file, qPrintable(e));
    return s;
}

double area(vtkPolyData *p, vtkIdType c)
{
    double a[3], b[3], d[3];
    vtkIdList *ids = p->GetCell(c)->GetPointIds();
    p->GetPoint(ids->GetId(0), a);
    p->GetPoint(ids->GetId(1), b);
    p->GetPoint(ids->GetId(2), d);
    return vtkTriangle::TriangleArea(a, b, d);
}

// Area per geometry face; -1 at a face's entry if its triangles are not
// all on one plane x, y or z = 0 or the box's length.
std::map<int, double> boxFaces(vtkPolyData *p)
{
    std::map<int, double> out;
    std::map<int, std::pair<int, double>> plane;  // face -> axis, value
    vtkDataArray *face = p->GetCellData()->GetArray("face");
    if (!face)
        return out;
    for (vtkIdType c = 0; c < p->GetNumberOfCells(); ++c) {
        const int f = int(face->GetComponent(c, 0));
        vtkIdList *ids = p->GetCell(c)->GetPointIds();
        double x0[3], x1[3], x2[3];
        p->GetPoint(ids->GetId(0), x0);
        p->GetPoint(ids->GetId(1), x1);
        p->GetPoint(ids->GetId(2), x2);
        int axis = -1;
        for (int k = 0; k < 3; ++k)
            if (std::abs(x0[k] - x1[k]) < 1e-9 && std::abs(x1[k] - x2[k]) < 1e-9)
                axis = k;
        if (!plane.count(f))
            plane[f] = {axis, axis < 0 ? 0 : x0[axis]};
        if (axis < 0 || plane[f].first != axis || std::abs(plane[f].second - x0[axis]) > 1e-9) {
            out[f] = -1;
            continue;
        }
        if (out[f] >= 0)
            out[f] += area(p, c);
    }
    return out;
}

} // namespace

CASE(each_box_face_is_its_own_flat_face_with_its_area)
{
    for (int order : {1, 2}) {
        const ResultsSeries m = mesh("box.step", order);
        vtkSmartPointer<vtkPolyData> s = outerSurface(m);
        CHECK(s->GetNumberOfPoints() == m.points.front().grid->GetNumberOfPoints());
        const std::map<int, double> faces = boxFaces(s);
        CHECK(faces.size() == 6);
        std::vector<double> areas;
        for (const auto &[f, a] : faces) {
            CHECK(f >= 1 && f <= 6);
            areas.push_back(a);
        }
        std::sort(areas.begin(), areas.end());
        const std::vector<double> known{200, 200, 300, 300, 600, 600};
        CHECK(areas.size() == known.size());
        for (size_t i = 0; i < std::min(areas.size(), known.size()); ++i)
            CHECK(std::abs(areas[i] - known[i]) < 1e-9 * known[i]);
    }
}

CASE(the_face_two_solids_share_is_not_on_the_outer_surface)
{
    const ResultsSeries m = mesh("twoboxes.step", 1);
    vtkSmartPointer<vtkPolyData> s = outerSurface(m);
    double total = 0;
    for (vtkIdType c = 0; c < s->GetNumberOfCells(); ++c)
        total += area(s, c);
    // Two cubes' 12 faces of 100 mm2, less the two halves of x = 10.
    CHECK(std::abs(total - 1000) < 1e-9 * 1000);
    vtkDataArray *face = s->GetCellData()->GetArray("face");
    CHECK(face);
    for (vtkIdType c = 0; face && c < s->GetNumberOfCells(); ++c)
        CHECK(face->GetComponent(c, 0) >= 1);
}

// The volume a closed, outward-facing surface holds (the divergence
// theorem: the sum of x . n dA / 3 over its triangles).
double enclosed(vtkPolyData *p)
{
    double v = 0;
    for (vtkIdType c = 0; c < p->GetNumberOfCells(); ++c) {
        vtkIdList *ids = p->GetCell(c)->GetPointIds();
        double a[3], b[3], d[3];
        p->GetPoint(ids->GetId(0), a);
        p->GetPoint(ids->GetId(1), b);
        p->GetPoint(ids->GetId(2), d);
        // a . (b x d) / 6 is the signed volume of the tetrahedron to the origin.
        v += (a[0] * (b[1] * d[2] - b[2] * d[1]) - a[1] * (b[0] * d[2] - b[2] * d[0]) + a[2] * (b[0] * d[1] - b[1] * d[0]))
             / 6;
    }
    return v;
}

const double dogboneVolume = 8699.688877463912;  // fixtures/step/README.md

CASE(curved_faces_of_a_quadratic_mesh_are_drawn_through_their_middle_nodes)
{
    // Corner triangles cut the dogbone's concave R30 arcs short; drawn
    // through the middle nodes (which lie on the geometry), the surface
    // holds the solid's own volume more closely. Half the spacing quarters
    // a chord's sag, so the error should fall to about a quarter (seen:
    // 6.69 to 1.67 mm3 at size 3); asked here: below half.
    QString e;
    const ResultsSeries linear = meshStep(QString(GF_FIXTURES) + "/step/dogbone.step", Recipe{3, 1}, &e);
    const ResultsSeries quadratic = meshStep(QString(GF_FIXTURES) + "/step/dogbone.step", Recipe{3, 2}, &e);
    const double vLinear = enclosed(outerSurface(linear)), vQuadratic = enclosed(outerSurface(quadratic));
    std::printf("  (dogbone held: linear %.6f, quadratic %.6f, known %.6f)\n", vLinear, vQuadratic, dogboneVolume);
    CHECK(std::abs(vQuadratic - dogboneVolume) < 0.5 * std::abs(vLinear - dogboneVolume));
}

CASE(the_element_edges_on_the_surface_are_drawn_once_each)
{
    // A closed surface of T triangles has 3T/2 edges. Drawn through its
    // middle node, a quadratic edge is two segments; its four triangles
    // per face stay one element face's worth of edges.
    for (int order : {1, 2}) {
        const ResultsSeries m = mesh("box.step", order);
        vtkSmartPointer<vtkPolyData> s = outerSurface(m), lines = outerEdges(m);
        const vtkIdType faces = order == 1 ? s->GetNumberOfCells() : s->GetNumberOfCells() / 4;
        CHECK(lines->GetNumberOfLines() == 3 * faces / 2 * order);
        CHECK(lines->GetNumberOfPolys() == 0);
    }
}

CASE(a_flat_faces_outward_normal_and_centre_are_measured)
{
    // The box (10 x 20 x 30 at the origin): every face's outward normal is
    // an axis, out of the box, and its centre the middle of that side.
    const ResultsSeries m = mesh("box.step", 2);
    vtkSmartPointer<vtkPolyData> s = outerSurface(m);
    const double size[3] = {10, 20, 30};
    std::map<std::pair<int, int>, int> seen;  // (axis, sign) -> face
    for (int f = 1; f <= 6; ++f) {
        QString problem;
        const auto n = outwardNormal(s, {f}, &problem);
        CHECK(n && problem.isEmpty());
        if (!n)
            continue;
        int axis = -1;
        for (int k = 0; k < 3; ++k)
            if (std::abs(std::abs((*n)[k]) - 1) < 1e-12)
                axis = k;
        CHECK(axis >= 0);
        if (axis < 0)
            continue;
        const int sign = (*n)[axis] > 0 ? 1 : -1;
        seen[{axis, sign}] = f;
        const auto c = centreOf(s, {f});
        for (int k = 0; k < 3; ++k) {
            const double known = k == axis ? (sign > 0 ? size[k] : 0) : size[k] / 2;
            CHECK(std::abs(c[k] - known) < 1e-9);
        }
    }
    CHECK(seen.size() == 6);
    // Two faces: one normal only if they face the same way.
    QString problem;
    const int plusX = seen[{0, 1}], minusX = seen[{0, -1}], plusY = seen[{1, 1}];
    CHECK(!outwardNormal(s, {plusX, minusX}, &problem));
    CHECK(problem == QString("faces %1 and %2 do not face the same way").arg(std::min(plusX, minusX)).arg(std::max(plusX, minusX)));
    CHECK(!outwardNormal(s, {plusX, plusY}, &problem));
    CHECK(!outwardNormal(s, {99}, &problem) && problem == "face 99 is not on the surface");
}

CASE(a_curved_face_has_no_one_normal)
{
    // The dogbone's R30 arcs are cylinders: refused by name; its flat
    // faces measured.
    const ResultsSeries m = mesh("dogbone.step", 2);
    vtkSmartPointer<vtkPolyData> s = outerSurface(m);
    int curved = 0, flat = 0;
    for (int f = 1; f <= 14; ++f) {
        QString problem;
        if (outwardNormal(s, {f}, &problem))
            ++flat;
        else if (problem == QString("face %1 is curved, so it has no one normal").arg(f))
            ++curved;
    }
    std::printf("  (dogbone: %d flat, %d curved)\n", flat, curved);
    CHECK(curved == 4 && flat == 10);  // dogbone.step: 14 ADVANCED_FACEs, 4 on CYLINDRICAL_SURFACEs, 10 on PLANEs
}

CASE(the_geometrys_edges_are_handed_on_with_their_ends)
{
    // The box's 12 edges (box.step: 12 EDGE_CURVEs): four each of 10, 20
    // and 30 mm, each straight along an axis; their segments drawn on the
    // grid's own points add up to the edge.
    for (int order : {1, 2}) {
        const ResultsSeries m = mesh("box.step", order);
        CHECK(edgeCount(m) == 12);
        vtkSmartPointer<vtkPolyData> lines = geometryEdges(m);
        CHECK(lines->GetNumberOfPoints() == m.points.front().grid->GetNumberOfPoints());
        vtkDataArray *edge = lines->GetCellData()->GetArray("edge");
        CHECK(edge);
        if (!edge)
            continue;
        std::map<int, double> drawn;
        vtkNew<vtkIdList> ids;
        for (vtkIdType c = 0; c < lines->GetNumberOfCells(); ++c) {
            lines->GetCellPoints(c, ids);
            double a[3], b[3];
            lines->GetPoint(ids->GetId(0), a);
            lines->GetPoint(ids->GetId(1), b);
            drawn[int(edge->GetComponent(c, 0))] += std::sqrt(vtkMath::Distance2BetweenPoints(a, b));
        }
        CHECK(drawn.size() == 12);
        std::map<long, int> lengths;
        for (int e = 1; e <= 12; ++e) {
            QString problem;
            const auto d = edgeDirection(m, e, &problem);
            CHECK(d && problem.isEmpty());
            int axes = 0;
            for (int k = 0; d && k < 3; ++k)
                axes += std::abs(std::abs((*d)[k]) - 1) < 1e-12 ? 1 : 0;
            CHECK(axes == 1);
            ++lengths[std::lround(drawn[e])];
            CHECK(std::abs(drawn[e] - std::lround(drawn[e])) < 1e-9);
        }
        CHECK((lengths == std::map<long, int>{{10, 4}, {20, 4}, {30, 4}}));
    }
    // A second-order mesh is the first-order one with middle nodes: each
    // segment drawn through its middle, so twice as many.
    const vtkIdType linear = geometryEdges(mesh("box.step", 1))->GetNumberOfCells();
    const vtkIdType quadratic = geometryEdges(mesh("box.step", 2))->GetNumberOfCells();
    CHECK(linear > 0 && quadratic == 2 * linear);
}

CASE(a_curved_edge_has_no_one_direction)
{
    // dogbone.step: 36 EDGE_CURVEs, 8 of them on its four R30 arcs'
    // CIRCLEs (top and bottom), so 28 straight.
    const ResultsSeries m = mesh("dogbone.step", 2);
    CHECK(edgeCount(m) == 36);
    int straight = 0, curved = 0;
    for (int e = 1; e <= 36; ++e) {
        QString problem;
        if (edgeDirection(m, e, &problem))
            ++straight;
        else if (problem == QString("edge %1 is curved, so it has no one direction").arg(e))
            ++curved;
    }
    CHECK(straight == 28 && curved == 8);
    QString problem;
    CHECK(!edgeDirection(m, 37, &problem) && problem == "there is no edge 37");
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
