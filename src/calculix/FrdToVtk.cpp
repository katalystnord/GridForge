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

namespace gf::calculix {

using gf::core::ResultsSeries;
using gf::core::TimePoint;

namespace {

// The VTK cell for each .frd element type a fixture has proven, with the
// node order unchanged: ccx's C3D8 and C3D10 orders are VTK's hexahedron
// and quadratic tetrahedron orders (fixtures bar-tension, cantilever-te10;
// test every_cell_has_its_nodes_where_vtk_expects_them).
int vtkCellFor(FrdElementType t)
{
    switch (t) {
    case FrdElementType::He8: return VTK_HEXAHEDRON;
    case FrdElementType::Te10: return VTK_QUADRATIC_TETRA;
    default: return -1;
    }
}

const char *typeName(FrdElementType t)
{
    switch (t) {
    case FrdElementType::He8: return "he8";
    case FrdElementType::Pe6: return "pe6";
    case FrdElementType::Te4: return "te4";
    case FrdElementType::Pe15: return "pe15";
    case FrdElementType::Te10: return "te10";
    case FrdElementType::Tr3: return "tr3";
    case FrdElementType::Qu4: return "qu4";
    case FrdElementType::Be2: return "be2";
    }
    return "?";
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
        const int cell = vtkCellFor(e.type);
        if (cell < 0) {
            *error = QString("element %1 is a %2, which is not mapped to a ParaView cell yet "
                             "(no fixture has proven its node order)")
                         .arg(e.id).arg(typeName(e.type));
            return {};
        }
        std::vector<vtkIdType> ids;
        for (int id : e.nodes) {
            const auto it = index.find(id);
            if (it == index.end()) {
                *error = QString("element %1 uses node %2, which the file does not hold")
                             .arg(e.id).arg(id);
                return {};
            }
            ids.push_back(it->second);
        }
        mesh->InsertNextCell(cell, vtkIdType(ids.size()), ids.data());
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

} // namespace gf::calculix
