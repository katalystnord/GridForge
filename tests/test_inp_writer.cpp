// The mesh part of a CalculiX deck: written from a grid, then read back by
// the deck reader that ccx's own decks are tested with (InpSets), and by
// eye against text written by hand. That ccx 2.23 takes it and solves it
// right is the fixture box-gfmesh (test_results_handoff and below).

#include "calculix/InpSets.h"
#include "calculix/InpWriter.h"

#include <QDir>
#include <QTemporaryDir>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>

#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::calculix;
using gf::core::NamedSet;
using gf::core::ResultsSeries;

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

// Twelve nodes (ids 101 to 112) at (k / 10, 0.5, -k), three linear
// tetrahedra (ids 7, 8, 9) of four nodes each, and sets.
ResultsSeries threeTets(int type = VTK_TETRA)
{
    auto g = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    vtkNew<vtkIntArray> nodeIds, elementIds;
    nodeIds->SetName("node id");
    elementIds->SetName("element id");
    for (int k = 0; k < 12; ++k) {
        points->InsertNextPoint(k / 10.0, 0.5, -static_cast<double>(k));  // -0.0 at k = 0
        nodeIds->InsertNextValue(101 + k);
    }
    g->SetPoints(points);
    for (vtkIdType c = 0; c < 3; ++c) {
        const vtkIdType pts[4] = {4 * c, 4 * c + 1, 4 * c + 2, 4 * c + 3};
        g->InsertNextCell(type, 4, pts);
        elementIds->InsertNextValue(7 + static_cast<int>(c));
    }
    g->GetPointData()->AddArray(nodeIds);
    g->GetCellData()->AddArray(elementIds);
    ResultsSeries s;
    s.points.push_back({0.0, g});
    NamedSet twelve{"Twelve", true, {}};
    for (vtkIdType k = 0; k < 12; ++k)
        twelve.members.push_back(k);
    s.sets = {twelve, NamedSet{"Fixed", true, {0, 5}}, NamedSet{"Second", false, {1}},
              NamedSet{"Fixed", false, {0, 2}}};
    s.provenance << "mesher: by hand" << "source: three tetrahedra";
    return s;
}

QStringList lines(const QString &text) { return text.split('\n'); }

} // namespace

CASE(the_deck_starts_with_who_wrote_it_and_the_mesh_provenance)
{
    QString error;
    const QStringList l = lines(meshInp(threeTets(), &error));
    CHECK(error.isEmpty());
    CHECK(l.value(0).startsWith("** written by GridForge "));
    CHECK(l.value(1) == "** mesher: by hand");
    CHECK(l.value(2) == "** source: three tetrahedra");
}

CASE(nodes_are_written_by_their_ids_with_every_digit_needed)
{
    QString error;
    const QStringList l = lines(meshInp(threeTets(), &error));
    const int at = l.indexOf("*NODE, NSET=NALL");
    CHECK(at > 0);
    if (at < 0)
        return;
    CHECK(l.value(at + 1) == "101, 0, 0.5, 0");  // -0 written as 0
    CHECK(l.value(at + 2) == "102, 0.1, 0.5, -1");
    CHECK(l.value(at + 4) == "104, 0.3, 0.5, -3");  // the shortest that reads back exactly
    CHECK(l.value(at + 12) == "112, 1.1, 0.5, -11");
    CHECK(l.value(at + 13).startsWith("*"));
}

CASE(tetrahedra_are_written_by_their_ids_in_vtks_order)
{
    QString error;
    const QStringList l = lines(meshInp(threeTets(), &error));
    const int at = l.indexOf("*ELEMENT, TYPE=C3D4, ELSET=EALL");
    CHECK(at > 0);
    if (at < 0)
        return;
    CHECK(l.value(at + 1) == "7, 101, 102, 103, 104");
    CHECK(l.value(at + 3) == "9, 109, 110, 111, 112");
}

CASE(sets_are_written_ten_ids_to_a_line_by_their_ids)
{
    QString error;
    const QString text = meshInp(threeTets(), &error);
    const QStringList l = lines(text);
    const int at = l.indexOf("*NSET, NSET=Twelve");
    CHECK(at > 0);
    CHECK(l.value(at + 1) == "101, 102, 103, 104, 105, 106, 107, 108, 109, 110");
    CHECK(l.value(at + 2) == "111, 112");
    CHECK(l.contains("*ELSET, ELSET=Second"));
    // A node set and an element set may share a name; ccx keeps them apart.
    CHECK(l.contains("*NSET, NSET=Fixed") && l.contains("*ELSET, ELSET=Fixed"));
}

CASE(the_deck_reads_back_with_its_sets)
{
    QTemporaryDir dir;
    QString error;
    const QString path = QDir(dir.path()).filePath("mesh.inp");
    CHECK(writeMeshInp(threeTets(), path, &error));
    const InpSets read = readInpSets(path);
    CHECK(read.ok());
    const InpSet *nall = read.find("NALL", true), *eall = read.find("EALL", false),
                 *twelve = read.find("TWELVE", true), *fixed = read.find("Fixed", true),
                 *second = read.find("Second", false), *fixedElements = read.find("Fixed", false);
    CHECK(nall && nall->ids.size() == 12 && nall->ids.front() == 101);
    CHECK(eall && eall->ids == std::vector<int>({7, 8, 9}));
    CHECK(twelve && twelve->ids.size() == 12 && twelve->ids.back() == 112);
    CHECK(fixed && fixed->ids == std::vector<int>({101, 106}));
    CHECK(second && second->ids == std::vector<int>({8}));
    CHECK(fixedElements && fixedElements->ids == std::vector<int>({7, 9}));
}

CASE(quadratic_tetrahedra_are_c3d10)
{
    // Ten nodes per cell: 12 points are not enough, so give each cell its
    // own ten (VTK's order is ccx's for C3D10).
    auto g = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkNew<vtkPoints> points;
    vtkNew<vtkIntArray> nodeIds, elementIds;
    nodeIds->SetName("node id");
    elementIds->SetName("element id");
    for (int k = 0; k < 10; ++k) {
        points->InsertNextPoint(k, 0, 0);
        nodeIds->InsertNextValue(k + 1);
    }
    g->SetPoints(points);
    const vtkIdType pts[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    g->InsertNextCell(VTK_QUADRATIC_TETRA, 10, pts);
    elementIds->InsertNextValue(1);
    g->GetPointData()->AddArray(nodeIds);
    g->GetCellData()->AddArray(elementIds);
    ResultsSeries s;
    s.points.push_back({0.0, g});
    QString error;
    const QStringList l = lines(meshInp(s, &error));
    const int at = l.indexOf("*ELEMENT, TYPE=C3D10, ELSET=EALL");
    CHECK(at > 0);
    CHECK(l.value(at + 1) == "1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10");
}

CASE(cells_not_written_yet_are_refused_by_name)
{
    QString error;
    CHECK(meshInp(threeTets(VTK_QUAD), &error).isEmpty());
    CHECK(error == "a quadrilateral is not written to a CalculiX deck yet; tetrahedra are");
}

CASE(a_set_name_ccx_cannot_take_is_written_with_underscores_and_said)
{
    // Study spec 4.2: a group name with a space or a comma is written with
    // underscores, and the deck's comments say so.
    CHECK(inpName("Fixed end") == "Fixed_end");
    CHECK(inpName("a,b c") == "a_b_c");
    CHECK(inpName("Back") == "Back");
    ResultsSeries s = threeTets();
    s.sets.push_back({"Fixed end", true, {0}});
    QString error;
    const QStringList l = lines(meshInp(s, &error));
    CHECK(error.isEmpty());
    CHECK(l.contains("** the set \"Fixed end\" is written as Fixed_end: a CalculiX set name has no "
                     "spaces or commas"));
    CHECK(l.contains("*NSET, NSET=Fixed_end"));
}

CASE(two_sets_that_would_be_written_alike_are_refused)
{
    ResultsSeries s = threeTets();
    s.sets.push_back({"Fixed end", true, {0}});
    s.sets.push_back({"Fixed_end", true, {1}});
    QString error;
    CHECK(meshInp(s, &error).isEmpty());
    CHECK(error == "the sets \"Fixed end\" and \"Fixed_end\" would both be written as Fixed_end");
}

CASE(nothing_to_write_is_said)
{
    QString error;
    CHECK(meshInp(ResultsSeries{}, &error).isEmpty());
    CHECK(error == "no mesh to write");
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
