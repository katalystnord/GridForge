// What the window draws for a step's supports and loads
// (docs/survey/loads-and-supports.md): an arrow in the true direction for
// a movement, a force or a pressure, a cone per held direction for a
// support (as PrePoMax's, and Abaqus's stemless arrows), at the centre of
// the group's faces. The answers are known from the box itself
// (fixtures/step/README.md): 10 x 20 x 30 mm at the origin, so each face
// is the middle of a side and faces out along an axis.

#include "core/Study.h"
#include "core/StudyEdit.h"
#include "gui/Markers.h"
#include "mesh/Mesher.h"
#include "mesh/Surface.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf;

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

const double size[3] = {10, 20, 30};

QString boxStudy(QTemporaryDir &dir)
{
    const QString from = QString(GF_FIXTURES) + "/studies/box-tension";
    for (const char *f : {"study.json", "box.step"})
        QFile::copy(from + "/" + f, QDir(dir.path()).filePath(f));
    QFile(QDir(dir.path()).filePath("study.json")).setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return dir.path();
}

vtkSmartPointer<vtkPolyData> boxSurface()
{
    QString error;
    static const core::ResultsSeries m = mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", mesh::Recipe{5, 2}, &error);
    return mesh::outerSurface(m);
}

// The middle of the box's side that faces along `axis`, `sign` (+1 or -1).
std::array<double, 3> side(int axis, int sign)
{
    std::array<double, 3> c{size[0] / 2, size[1] / 2, size[2] / 2};
    c[axis] = sign > 0 ? size[axis] : 0;
    return c;
}

bool near(const std::array<double, 3> &a, const std::array<double, 3> &b)
{
    return std::abs(a[0] - b[0]) < 1e-9 && std::abs(a[1] - b[1]) < 1e-9 && std::abs(a[2] - b[2]) < 1e-9;
}

std::array<double, 3> axisVector(int axis, int sign)
{
    std::array<double, 3> d{0, 0, 0};
    d[axis] = sign;
    return d;
}

// The axis a side faces along, from where its marker sits.
std::pair<int, int> sideOf(const std::array<double, 3> &at)
{
    for (int k = 0; k < 3; ++k) {
        if (std::abs(at[k]) < 1e-9)
            return {k, -1};
        if (std::abs(at[k] - size[k]) < 1e-9)
            return {k, 1};
    }
    return {-1, 0};
}

} // namespace

CASE(the_box_studys_symmetry_and_pull_are_drawn_where_they_act)
{
    QTemporaryDir dir;
    QString error;
    const core::Study study = core::readStudy(boxStudy(dir), &error);
    CHECK(error.isEmpty());
    const std::vector<gui::Marker> m = gui::markersFor(study, 0, boxSurface());
    // Three symmetry supports, a cone each, square to its face and into the
    // box; the pull an arrow along +x out of face x = 10.
    int cones = 0, arrows = 0;
    for (const gui::Marker &k : m) {
        const auto [axis, sign] = sideOf(k.at);
        CHECK(axis >= 0);
        if (axis < 0)
            continue;
        CHECK(near(k.at, side(axis, sign)));
        if (k.shape == gui::Marker::Shape::Cone) {
            ++cones;
            CHECK(k.role == "support" && k.key.startsWith("support:"));
            CHECK(near(k.direction, axisVector(axis, -sign)));  // into the box
        } else {
            ++arrows;
            CHECK(k.role == "move" && k.key == "load:0:0");
            CHECK(axis == 0 && sign == 1 && near(k.direction, axisVector(0, 1)) && k.outward);
        }
    }
    CHECK(cones == 3 && arrows == 1);
}

CASE(each_kind_is_drawn_in_its_true_direction)
{
    QTemporaryDir dir;
    const QString folder = boxStudy(dir);
    QString error;
    // The top (z = 30) fixed; a force into face x = 10; a pressure on it.
    CHECK(core::addFaceGroup(folder, "Top", {6}, &error));
    core::Study study = core::readStudy(folder, &error);
    const auto top = gf::mesh::outwardNormal(boxSurface(), {6}, &error);
    CHECK(top && std::abs((*top)[2] - 1) < 1e-12);  // face 6 is the top (z = 30), measured
    CHECK(core::addItem(folder, "step:0", QJsonObject{{"group", "Top"}, {"fixed", true}}, &error) == "support:0:0");
    CHECK(core::addItem(folder, "step:0",
                        QJsonObject{{"group", "Pulled"}, {"force", QJsonObject{{"x", -50}}}, {"direction", "-x"}}, &error)
          == "load:0:1");
    CHECK(core::addItem(folder, "step:0", QJsonObject{{"group", "Pulled"}, {"pressure", 5}}, &error) == "load:0:2");
    study = core::readStudy(folder, &error);
    CHECK(error.isEmpty());
    const std::vector<gui::Marker> m = gui::markersFor(study, 0, boxSurface());
    int fixedCones = 0;
    bool force = false, pressure = false;
    for (const gui::Marker &k : m) {
        if (k.key == "support:0:0") {
            ++fixedCones;
            CHECK(k.shape == gui::Marker::Shape::Cone && near(k.at, side(2, 1)));
            // Each held direction: z into the box, x and y along the face.
            CHECK(near(k.direction, axisVector(2, -1)) || near(k.direction, axisVector(0, 1))
                  || near(k.direction, axisVector(1, 1)));
        } else if (k.key == "load:0:1") {
            force = true;
            // Pushing into the face: the arrow's tip on it.
            CHECK(k.role == "force" && k.shape == gui::Marker::Shape::Arrow);
            CHECK(near(k.direction, axisVector(0, -1)) && !k.outward && near(k.at, side(0, 1)));
        } else if (k.key == "load:0:2") {
            pressure = true;
            CHECK(k.role == "pressure" && near(k.direction, axisVector(0, -1)) && !k.outward);
        }
    }
    CHECK(fixedCones == 3 && force && pressure);
    // A step that keeps them draws them too; none before they are set.
    CHECK(gui::markersFor(study, 5, boxSurface()).empty());
    CHECK(gui::markersFor(study, 0, nullptr).empty());
}

CASE(a_click_picks_the_nearest_edge_within_reach)
{
    // Two edges drawn: a horizontal one at y = 100 (edge 7, two segments)
    // and a vertical one at x = 300 (edge 3).
    const std::vector<std::array<double, 4>> segments{{100, 100, 200, 100}, {200, 100, 280, 100}, {300, 0, 300, 200}};
    const std::vector<int> edgeOf{7, 7, 3};
    CHECK(gui::nearestEdge(segments, edgeOf, 150, 104, 8) == 7);
    CHECK(gui::nearestEdge(segments, edgeOf, 296, 50, 8) == 3);
    // Nearer the second segment's end than the vertical edge: 7.
    CHECK(gui::nearestEdge(segments, edgeOf, 284, 100, 8) == 7);
    CHECK(gui::nearestEdge(segments, edgeOf, 293, 100, 8) == 3);
    // Beyond a segment's end, measured to the end, not the line.
    CHECK(gui::nearestEdge(segments, edgeOf, 50, 100, 8) == 0);
    CHECK(gui::nearestEdge(segments, edgeOf, 150, 120, 8) == 0);
    CHECK(gui::nearestEdge({}, {}, 0, 0, 8) == 0);
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
