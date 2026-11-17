// The hand-off of a Gmsh mesh to ParaView: the .msh as one VTK grid with
// its physical groups as sets. The box is 10 x 20 x 30 mm whatever its
// elements, so every fixture must enclose 6000 mm3, with every node where
// VTK's cell definition expects it.

#include "CellChecks.h"
#include "gmsh/MshToVtk.h"

#include <QFileInfo>

#include <vtkCellData.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::gmsh;
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

ResultsSeries convert(const char *rel, QString *error = nullptr)
{
    const QString path = QString(GF_FIXTURES) + "/gmsh/" + rel;
    QString e;
    const MshFile f = readMsh(path);
    if (!f.ok())
        std::printf("  (%s: %s)\n", rel, qPrintable(f.error));
    ResultsSeries s = toSeries(f, QFileInfo(path).fileName(), &e);
    if (!e.isEmpty())
        std::printf("  (%s: %s)\n", rel, qPrintable(e));
    if (error)
        *error = e;
    return s;
}

vtkUnstructuredGrid *grid(const ResultsSeries &s)
{
    return s.points.empty() ? nullptr : s.points.front().grid.Get();
}

const NamedSet *setOf(const ResultsSeries &s, const char *name, bool nodes)
{
    for (const NamedSet &n : s.sets)
        if (n.name == name && n.nodes == nodes)
            return &n;
    return nullptr;
}

// Whether every point of a node set satisfies `where`.
bool allPoints(const ResultsSeries &s, const NamedSet *set,
               const std::function<bool(const std::array<double, 3> &)> &where)
{
    if (!set || !grid(s))
        return false;
    for (vtkIdType p : set->members)
        if (!where(point(grid(s), p)))
            return false;
    return true;
}

QStringList provenance(vtkUnstructuredGrid *g)
{
    QStringList out;
    auto *a = g ? vtkStringArray::SafeDownCast(g->GetFieldData()->GetAbstractArray("provenance"))
                : nullptr;
    for (vtkIdType i = 0; a && i < a->GetNumberOfValues(); ++i)
        out << QString::fromStdString(a->GetValue(i));
    return out;
}

} // namespace

CASE(each_element_type_lands_on_its_vtk_cell_in_place)
{
    struct Expect {
        const char *fixture;
        vtkIdType cells;
        int type;
    };
    const Expect all[] = {
        {"box-hex8/box.msh", 24, VTK_HEXAHEDRON},
        {"box-hex20/box.msh", 24, VTK_QUADRATIC_HEXAHEDRON},
        {"box-prism6/box.msh", 48, VTK_WEDGE},
        {"box-prism15/box.msh", 48, VTK_QUADRATIC_WEDGE},
        {"box-tet4/box.msh", 897, VTK_TETRA},
        {"box-tet10/box.msh", 897, VTK_QUADRATIC_TETRA},
    };
    for (const Expect &x : all) {
        const ResultsSeries s = convert(x.fixture);
        vtkUnstructuredGrid *g = grid(s);
        CHECK(s.points.size() == 1);
        if (!g)
            continue;
        bool oneType = g->GetNumberOfCells() == x.cells;
        for (vtkIdType c = 0; oneType && c < g->GetNumberOfCells(); ++c)
            oneType = g->GetCellType(c) == x.type;
        const int misplaced = misplacedCells(g);
        const double volume = enclosedVolume(g);
        if (!oneType || misplaced || std::abs(volume - 6000) > 1e-9 * 6000)
            std::printf("  (%s: %lld cells, %d misplaced, volume %.12g)\n", x.fixture,
                        (long long)g->GetNumberOfCells(), misplaced, volume);
        CHECK(oneType);
        CHECK(misplaced == 0);
        CHECK(std::abs(volume - 6000) <= 1e-9 * 6000);
    }
}

CASE(nodes_and_elements_keep_the_files_tags)
{
    const ResultsSeries s = convert("box-hex8/box.msh");
    vtkUnstructuredGrid *g = grid(s);
    auto *nodeIds = g ? vtkIntArray::SafeDownCast(g->GetPointData()->GetArray("node id")) : nullptr;
    auto *elementIds =
        g ? vtkIntArray::SafeDownCast(g->GetCellData()->GetArray("element id")) : nullptr;
    CHECK(nodeIds && nodeIds->GetNumberOfTuples() == 60);
    CHECK(elementIds && elementIds->GetNumberOfTuples() == 24);
    if (!nodeIds || !elementIds)
        return;
    // Node 1 is the corner at the origin (the first point extruded).
    bool origin = false;
    for (vtkIdType p = 0; p < 60; ++p)
        if (nodeIds->GetValue(p) == 1)
            origin = point(g, p) == std::array<double, 3>{0, 0, 0};
    CHECK(origin);
    // The file holds the groups' 57 elements; the hexahedra are the last
    // 24, tags 34 to 57.
    CHECK(elementIds->GetValue(0) == 34 && elementIds->GetValue(23) == 57);
}

CASE(the_physical_groups_become_sets)
{
    const ResultsSeries s = convert("box-hex8/box.msh");
    // Node sets by hand: the faces x = 0 and x = 10 have 4 x 5 nodes, the
    // top 3 x 4, the edge 3, the corner 1.
    struct Expect {
        const char *name;
        size_t members;
    };
    for (const Expect &x : {Expect{"Solid", 60}, Expect{"Fixed", 20}, Expect{"Pulled", 20},
                            Expect{"PhysicalSurface77", 12}, Expect{"Edge", 3},
                            Expect{"Origin", 1}}) {
        const NamedSet *n = setOf(s, x.name, true);
        CHECK(n && n->members.size() == x.members);
    }
    const NamedSet *solid = setOf(s, "Solid", false);
    CHECK(solid && solid->members.size() == 24);
    CHECK(s.sets.size() == 7);  // six node sets, one element set
    // Node sets first; highest dimension first, then by tag.
    QStringList order;
    for (const NamedSet &n : s.sets)
        order << (n.nodes ? "" : "elements ") + n.name;
    CHECK(order == (QStringList{"Solid", "Fixed", "Pulled", "PhysicalSurface77", "Edge", "Origin",
                                "elements Solid"}));
    // The surfaces, curve and point are node sets only: their elements
    // are not cells of the volume grid.
    CHECK(!setOf(s, "Fixed", false) && !setOf(s, "Edge", false));
}

CASE(each_sets_nodes_are_where_the_group_is)
{
    using P = std::array<double, 3>;
    for (const char *rel : {"box-hex8/box.msh", "box-hex20/box.msh", "box-prism15/box.msh",
                            "box-tet4/box.msh", "box-tet10/box.msh"}) {
        const ResultsSeries s = convert(rel);
        const bool ok =
            allPoints(s, setOf(s, "Fixed", true), [](const P &p) { return p[0] == 0; })
            && allPoints(s, setOf(s, "Pulled", true), [](const P &p) { return p[0] == 10; })
            && allPoints(s, setOf(s, "PhysicalSurface77", true),
                         [](const P &p) { return p[2] == 30; })
            && allPoints(s, setOf(s, "Edge", true),
                         [](const P &p) { return p[1] == 0 && p[2] == 0; })
            && allPoints(s, setOf(s, "Origin", true),
                         [](const P &p) { return p == P{0, 0, 0}; });
        if (!ok)
            std::printf("  (%s)\n", rel);
        CHECK(ok);
    }
    // The quadratic face x = 0 holds its middle nodes too: 20 corners and
    // 15 + 16 edge middles.
    const ResultsSeries hex20 = convert("box-hex20/box.msh");
    const NamedSet *fixed = setOf(hex20, "Fixed", true);
    CHECK(fixed && fixed->members.size() == 51);
}

CASE(the_provenance_names_the_source_and_what_was_done)
{
    const ResultsSeries s = convert("box-hex8/box.msh");
    const QStringList p = provenance(grid(s));
    CHECK(p == s.provenance);
    CHECK(p.contains("source: box.msh"));
    CHECK(p.contains("format: MSH 4.1, ASCII"));
    CHECK(p.contains("model: the 24 elements of dimension 3; 33 of lower dimension left out"));
    CHECK(p.contains("sets: 6 physical groups, as 6 node sets and 1 element set"));
    const QStringList b = provenance(grid(convert("box-hex8-binary/box.msh")));
    CHECK(b.contains("format: MSH 4.1, binary"));
}

CASE(a_file_that_was_not_read_is_not_converted)
{
    QString error;
    const ResultsSeries s = convert("refused/hex27/box.msh", &error);
    CHECK(s.points.empty());
    CHECK(error.contains("type 10"));
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
