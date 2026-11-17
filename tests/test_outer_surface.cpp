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
