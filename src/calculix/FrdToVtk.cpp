#include "calculix/FrdToVtk.h"

#include "core/ApplicationInfo.h"
#include "core/Stress.h"

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStringArray.h>

#include <cmath>
#include <unordered_map>
#include <vector>

namespace gf::calculix {

using gf::core::ResultsSeries;
using gf::core::TimePoint;

namespace {

// The VTK cell for each .frd element type a fixture has proven, and where
// each of VTK's nodes sits in the file's list. ccx writes C3D8, C3D10 and
// C3D4 in VTK's order. C3D20 and C3D15 come with the vertical edges' middle
// nodes before the top edges', where VTK has them after. C3D6 is mirrored:
// VTK's linear wedge wants its face (0, 1, 2) to face out, while its
// quadratic wedge, like ccx, wants (0, 2, 1) to (vtkWedge::GetFaceArray
// against vtkQuadraticWedge's, VTK 9.5). Shells, plane elements and beams
// written with OUTPUT=2D (tr3, tr6, qu4, qu8, be2, be3) are in VTK's order
// as written, be3 included (end, end, middle). (Fixtures shell-*, beam-*,
// bar-tension, cantilever-te10, cube-*; test
// each_solid_element_type_lands_on_its_vtk_cell_in_place).
struct CellMap {
    int cell = -1;
    std::vector<int> order;  // VTK node k is the file's node order[k]
};

std::vector<int> identity(int n)
{
    std::vector<int> v(size_t(n), 0);
    for (int k = 0; k < n; ++k)
        v[size_t(k)] = k;
    return v;
}

const CellMap &cellFor(FrdElementType t)
{
    static const CellMap none;
    static const CellMap he8{VTK_HEXAHEDRON, identity(8)};
    static const CellMap te4{VTK_TETRA, identity(4)};
    static const CellMap pe6{VTK_WEDGE, {0, 2, 1, 3, 5, 4}};
    static const CellMap te10{VTK_QUADRATIC_TETRA, identity(10)};
    static const CellMap he20{VTK_QUADRATIC_HEXAHEDRON,
                              {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 12, 13, 14, 15}};
    static const CellMap pe15{VTK_QUADRATIC_WEDGE,
                              {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 13, 14, 9, 10, 11}};
    static const CellMap tr3{VTK_TRIANGLE, identity(3)};
    static const CellMap tr6{VTK_QUADRATIC_TRIANGLE, identity(6)};
    static const CellMap qu4{VTK_QUAD, identity(4)};
    static const CellMap qu8{VTK_QUADRATIC_QUAD, identity(8)};
    static const CellMap be2{VTK_LINE, identity(2)};
    static const CellMap be3{VTK_QUADRATIC_EDGE, identity(3)};
    switch (t) {
    case FrdElementType::He8: return he8;
    case FrdElementType::Tr3: return tr3;
    case FrdElementType::Tr6: return tr6;
    case FrdElementType::Qu4: return qu4;
    case FrdElementType::Qu8: return qu8;
    case FrdElementType::Be2: return be2;
    case FrdElementType::Be3: return be3;
    case FrdElementType::Te4: return te4;
    case FrdElementType::Pe6: return pe6;
    case FrdElementType::Te10: return te10;
    case FrdElementType::He20: return he20;
    case FrdElementType::Pe15: return pe15;
    default: return none;
    }
}

QString typeName(FrdElementType t)
{
    switch (t) {
    case FrdElementType::He8: return "he8";
    case FrdElementType::Pe6: return "pe6";
    case FrdElementType::Te4: return "te4";
    case FrdElementType::He20: return "he20";
    case FrdElementType::Pe15: return "pe15";
    case FrdElementType::Te10: return "te10";
    case FrdElementType::Tr3: return "tr3";
    case FrdElementType::Tr6: return "tr6";
    case FrdElementType::Qu4: return "qu4";
    case FrdElementType::Qu8: return "qu8";
    case FrdElementType::Be2: return "be2";
    case FrdElementType::Be3: return "be3";
    }
    return QString("type %1").arg(int(t));
}

const char *kVonMises = "von Mises stress (derived)";
const char *kPrincipal = "principal stresses (derived)";

// One solver dataset as a point array: its name, its supplied components
// with their names; NaN at any point the dataset leaves out.
vtkSmartPointer<vtkDoubleArray> pointArray(const FrdDataset &d, vtkIdType points,
                                           const std::unordered_map<int, vtkIdType> &index)
{
    auto a = vtkSmartPointer<vtkDoubleArray>::New();
    a->SetName(d.name.toUtf8().constData());
    const int per = d.suppliedCount();
    a->SetNumberOfComponents(per);
    a->SetNumberOfTuples(points);
    a->Fill(NAN);
    int k = 0;
    for (const FrdComponent &c : d.components)
        if (c.supplied)
            a->SetComponentName(k++, c.name.toUtf8().constData());
    for (size_t i = 0; i < d.nodeIds.size(); ++i) {
        const auto it = index.find(d.nodeIds[i]);
        if (it == index.end())
            continue;
        for (int c = 0; c < per; ++c)
            a->SetComponent(it->second, c, d.values[i * size_t(per) + size_t(c)]);
    }
    return a;
}

void addDerivedStresses(vtkUnstructuredGrid *g, vtkDataArray *stress)
{
    const vtkIdType n = stress->GetNumberOfTuples();
    vtkNew<vtkDoubleArray> mises, principal;
    mises->SetName(kVonMises);
    mises->SetNumberOfTuples(n);
    principal->SetName(kPrincipal);
    principal->SetNumberOfComponents(3);
    principal->SetComponentName(0, "S1");
    principal->SetComponentName(1, "S2");
    principal->SetComponentName(2, "S3");
    principal->SetNumberOfTuples(n);
    for (vtkIdType p = 0; p < n; ++p) {
        double s[6];
        stress->GetTuple(p, s);
        const gf::core::SymmetricTensor t{s[0], s[1], s[2], s[3], s[4], s[5]};
        mises->SetValue(p, gf::core::vonMises(t));
        const auto v = gf::core::principalValues(t);
        principal->SetTuple(p, v.data());
    }
    g->GetPointData()->AddArray(mises);
    g->GetPointData()->AddArray(principal);
}

} // namespace

ResultsSeries toSeries(const FrdFile &frd, const QString &source, QString *error)
{
    error->clear();
    if (!frd.ok()) {
        *error = frd.error;
        return {};
    }

    // The mesh, shared by every time point.
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    vtkNew<vtkIntArray> nodeIds;
    nodeIds->SetName("node id");
    std::unordered_map<int, vtkIdType> index;
    for (const FrdNode &n : frd.nodes) {
        index[n.id] = points->InsertNextPoint(n.x, n.y, n.z);
        nodeIds->InsertNextValue(n.id);
    }
    auto mesh = vtkSmartPointer<vtkUnstructuredGrid>::New();
    mesh->SetPoints(points);
    mesh->Allocate(vtkIdType(frd.elements.size()));
    vtkNew<vtkIntArray> elementIds, materials;
    elementIds->SetName("element id");
    materials->SetName("material");
    for (const FrdElement &e : frd.elements) {
        const CellMap &map = cellFor(e.type);
        if (map.cell < 0) {
            *error = QString("element %1 is a %2, which is not mapped to a ParaView cell yet "
                             "(no fixture has proven its node order)")
                         .arg(e.id).arg(typeName(e.type));
            return {};
        }
        std::vector<vtkIdType> ids;
        for (int k : map.order) {
            const int id = e.nodes[size_t(k)];
            const auto it = index.find(id);
            if (it == index.end()) {
                *error = QString("element %1 uses node %2, which the file does not hold")
                             .arg(e.id).arg(id);
                return {};
            }
            ids.push_back(it->second);
        }
        mesh->InsertNextCell(map.cell, vtkIdType(ids.size()), ids.data());
        elementIds->InsertNextValue(e.id);
        materials->InsertNextValue(e.material);
    }
    mesh->GetPointData()->AddArray(nodeIds);
    mesh->GetCellData()->AddArray(elementIds);
    mesh->GetCellData()->AddArray(materials);

    ResultsSeries series;
    QString solver = "unknown (the file does not say)";
    for (const QString &line : frd.userHeader)
        if (line.startsWith("VERSION"))
            solver = "CalculiX " + line.mid(7).trimmed();
    series.provenance << "solver: " + solver << "source: " + source
                      << QString("converted by: %1 %2 (build %3)")
                             .arg(gf::core::applicationName(), gf::core::applicationVersion(),
                                  gf::core::applicationBuild());

    // One time point per output increment, in the order the file holds them.
    bool derived = false;
    for (const FrdDataset &d : frd.datasets) {
        if (series.points.empty() || series.points.back().grid->GetFieldData()
                                             ->GetArray("frd step")->GetTuple1(0) != d.step) {
            TimePoint t;
            t.time = d.value;
            t.grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
            t.grid->ShallowCopy(mesh);
            // Each time point owns its point data; the mesh arrays are shared.
            vtkNew<vtkPointData> pd;
            pd->ShallowCopy(mesh->GetPointData());
            t.grid->GetPointData()->ShallowCopy(pd);
            vtkNew<vtkFieldData> fd;
            vtkNew<vtkIntArray> step;
            step->SetName("frd step");
            step->InsertNextValue(d.step);
            fd->AddArray(step);
            t.grid->SetFieldData(fd);
            series.points.push_back(t);
        }
        vtkUnstructuredGrid *g = series.points.back().grid;
        auto a = pointArray(d, mesh->GetNumberOfPoints(), index);
        g->GetPointData()->AddArray(a);
        if (d.name == "STRESS" && a->GetNumberOfComponents() == 6) {
            addDerivedStresses(g, a);
            derived = true;
        }
    }
    if (derived)
        series.provenance << QString("derived by GridForge: %1, %2, from STRESS")
                                 .arg(kVonMises, kPrincipal);

    vtkNew<vtkStringArray> provenance;
    provenance->SetName("provenance");
    for (const QString &line : series.provenance)
        provenance->InsertNextValue(line.toStdString());
    for (TimePoint &t : series.points)
        t.grid->GetFieldData()->AddArray(provenance);
    return series;
}

void addSets(ResultsSeries &series, const InpSets &sets, const QString &deck)
{
    if (series.points.empty())
        return;
    vtkUnstructuredGrid *g = series.points.front().grid;
    std::unordered_map<int, vtkIdType> point, cell;
    if (auto *ids = vtkIntArray::SafeDownCast(g->GetPointData()->GetArray("node id")))
        for (vtkIdType p = 0; p < ids->GetNumberOfTuples(); ++p)
            point[ids->GetValue(p)] = p;
    if (auto *ids = vtkIntArray::SafeDownCast(g->GetCellData()->GetArray("element id")))
        for (vtkIdType c = 0; c < ids->GetNumberOfTuples(); ++c)
            cell[ids->GetValue(c)] = c;

    QStringList notes;
    for (const InpSet &in : sets.sets) {
        const auto &index = in.nodes ? point : cell;
        gf::core::NamedSet out{in.name, in.nodes, {}};
        for (int id : in.ids) {
            const auto it = index.find(id);
            if (it != index.end())
                out.members.push_back(it->second);
        }
        const size_t missing = in.ids.size() - out.members.size();
        if (missing > 0)
            notes << QString("set %1: %2 of its %3 %4 are not in the results, left out")
                         .arg(in.name)
                         .arg(missing)
                         .arg(in.ids.size())
                         .arg(in.nodes ? "nodes" : "elements");
        series.sets.push_back(std::move(out));
    }
    series.provenance << QString("sets: %1, from %2").arg(sets.sets.size()).arg(deck) << notes;

    // The grids carry the provenance too; they share one array, so replace it.
    vtkNew<vtkStringArray> provenance;
    provenance->SetName("provenance");
    for (const QString &line : series.provenance)
        provenance->InsertNextValue(line.toStdString());
    for (gf::core::TimePoint &t : series.points)
        t.grid->GetFieldData()->AddArray(provenance);
}

} // namespace gf::calculix
