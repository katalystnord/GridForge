#include "mesh/Mesher.h"

#include <QFileInfo>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStringArray.h>

// Netgen's own headers; OpenCASCADE's come in through occgeom.hpp.
#include <meshing.hpp>
#include <occgeom.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace gf::mesh {

namespace {

// Netgen's tetrahedra turn the other way from VTK's, and number their
// middle nodes 01, 02, 03, 12, 13, 23. In VTK's order (corners 0 1 3 2,
// then edges 01, 13, 30, 02, 12, 32 of Netgen's corners) they are these
// (the same permutation Netgen's own Abaqus writer uses for C3D4, C3D10).
constexpr int tet4[4] = {0, 1, 3, 2};
constexpr int tet10[10] = {0, 1, 3, 2, 4, 8, 6, 5, 7, 9};

QString plural(int n, const char *one, const char *many)
{
    return QString("%1 %2").arg(n).arg(n == 1 ? one : many);
}

} // namespace

QString engine()
{
    return QString("Netgen %1 (github.com/katalystnord/netgen at %2)")
        .arg(GF_NETGEN_VERSION, GF_NETGEN_COMMIT);
}

gf::core::ResultsSeries meshStep(const QString &stepPath, const Recipe &recipe, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return gf::core::ResultsSeries{};
    };
    if (!(recipe.maxSize > 0))
        return fail("the recipe has no largest element size");
    if (recipe.order != 1 && recipe.order != 2)
        return fail(QString("the recipe's order is %1; only 1 and 2 are made").arg(recipe.order));
    if (!QFileInfo(stepPath).isFile())
        return fail(QString("cannot read %1: no such file").arg(stepPath));

    ngcore::printmessage_importance = 0;  // Netgen's progress messages off

    std::shared_ptr<netgen::OCCGeometry> geometry;
    auto mesh = std::make_shared<netgen::Mesh>();
    try {
        geometry.reset(netgen::LoadOCC_STEP(stepPath.toStdString()));
        if (geometry->somap.Extent() == 0)
            return fail(QString("%1 holds no solid to mesh").arg(QFileInfo(stepPath).fileName()));
        netgen::MeshingParameters parameters;
        parameters.maxh = recipe.maxSize;
        mesh->SetGeometry(geometry);
        if (geometry->GenerateMesh(mesh, parameters) != 0)
            return fail("Netgen could not mesh " + QFileInfo(stepPath).fileName());
        if (recipe.order == 2)
            geometry->GetRefinement().MakeSecondOrder(*mesh);
    } catch (const std::exception &e) {
        return fail(QString("Netgen could not mesh %1: %2").arg(QFileInfo(stepPath).fileName(), e.what()));
    }

    // Points, numbered from 1 in Netgen's order: only those an element
    // uses. Netgen keeps others (a dogbone's arc centres, outside the
    // part), which would stretch the grid and every node set.
    std::set<netgen::PointIndex> usedPoints;
    for (netgen::ElementIndex ei : mesh->VolumeElements().Range()) {
        const auto el = (*mesh)[ei];
        for (int k = 0; k < el.GetNP(); ++k)
            usedPoints.insert(el[k]);
    }
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    vtkNew<vtkIntArray> nodeIds;
    nodeIds->SetName("node id");
    std::map<netgen::PointIndex, vtkIdType> place;
    for (netgen::PointIndex pi : mesh->Points().Range()) {
        if (!usedPoints.count(pi))
            continue;
        const netgen::Point<3> &p = (*mesh)[pi];
        place[pi] = points->InsertNextPoint(p(0), p(1), p(2));
        nodeIds->InsertNextValue(static_cast<int>(place[pi] + 1));
    }

    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->GetPointData()->AddArray(nodeIds);
    vtkNew<vtkIntArray> elementIds;
    elementIds->SetName("element id");

    const int solids = geometry->somap.Extent();
    const int faces = geometry->fmap.Extent();
    std::vector<std::vector<vtkIdType>> solidCells(solids);
    for (netgen::ElementIndex ei : mesh->VolumeElements().Range()) {
        const auto el = (*mesh)[ei];
        const int np = el.GetNP();
        if (np != 4 && np != 10)
            return fail(QString("Netgen made a %1-node volume element; only tetrahedra are taken").arg(np));
        vtkIdType ids[10];
        for (int k = 0; k < np; ++k)
            ids[k] = place[el[np == 4 ? tet4[k] : tet10[k]]];
        const vtkIdType cell = grid->InsertNextCell(np == 4 ? VTK_TETRA : VTK_QUADRATIC_TETRA, np, ids);
        elementIds->InsertNextValue(static_cast<int>(cell + 1));
        const int solid = el.GetIndex().Nr1();  // 1-based, in the file's order
        if (solid >= 1 && solid <= solids)
            solidCells[solid - 1].push_back(cell);
    }
    grid->GetCellData()->AddArray(elementIds);

    // The nodes of each geometry face, from the surface elements on it.
    std::vector<std::set<vtkIdType>> faceNodes(faces);
    for (netgen::SurfaceElementIndex si : mesh->SurfaceElements().Range()) {
        const auto el = (*mesh)[si];
        const int face = mesh->GetFaceDescriptor(el.GetIndex()).SurfNr();  // 1-based
        if (face < 1 || face > faces)
            continue;
        for (int k = 0; k < el.GetNP(); ++k)
            if (const auto at = place.find(el[k]); at != place.end())
                faceNodes[face - 1].insert(at->second);
    }

    gf::core::ResultsSeries series;
    series.points.push_back({0.0, grid});
    for (int s = 0; s < solids; ++s) {
        const QString name = QString("Solid%1").arg(s + 1);
        std::set<vtkIdType> nodes;
        for (vtkIdType c : solidCells[s]) {
            vtkIdType n;
            const vtkIdType *pts;
            grid->GetCellPoints(c, n, pts);
            nodes.insert(pts, pts + n);
        }
        series.sets.push_back({name, true, {nodes.begin(), nodes.end()}});
        series.sets.push_back({name, false, solidCells[s]});
    }
    for (int f = 0; f < faces; ++f)
        series.sets.push_back({QString("Face%1").arg(f + 1), true,
                               {faceNodes[f].begin(), faceNodes[f].end()}});
    // Node sets first, as the Gmsh hand-off orders them.
    std::stable_partition(series.sets.begin(), series.sets.end(),
                          [](const gf::core::NamedSet &s) { return s.nodes; });

    series.provenance << "mesher: " + engine()
                      << "source: " + QFileInfo(stepPath).fileName()
                      << QString("recipe: largest element %1, order %2").arg(recipe.maxSize).arg(recipe.order)
                      << QString("model: %1 %2tetrahedra, %3 nodes")
                             .arg(grid->GetNumberOfCells())
                             .arg(recipe.order == 2 ? "quadratic " : "")
                             .arg(grid->GetNumberOfPoints())
                      << "node order: Netgen's, put into VTK's by GridForge"
                      << QString("sets: %1, as %2; %3, as node sets")
                             .arg(plural(solids, "solid", "solids"),
                                  solids == 1 ? "an element set and a node set"
                                              : "element sets and node sets",
                                  plural(faces, "face", "faces"));
    vtkNew<vtkStringArray> provenance;
    provenance->SetName("provenance");
    for (const QString &line : series.provenance)
        provenance->InsertNextValue(line.toStdString());
    grid->GetFieldData()->AddArray(provenance);
    return series;
}

} // namespace gf::mesh
