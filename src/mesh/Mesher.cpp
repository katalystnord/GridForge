#include "mesh/Mesher.h"

#include <QFileInfo>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
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

#include <BOPAlgo_Builder.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <algorithm>
#include <array>
#include <map>
#include <mutex>
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

// Solids that touch are joined, so the face they share is one face, meshed
// once, and both solids' elements use its nodes. OpenCASCADE's general
// fuse does the joining; its history says which joined solid came from
// which solid of the file, and a joined solid from two of them means they
// overlap. Netgen has a GlueGeometry of its own; this one keeps the errors
// and the history.
struct Joined {
    TopoDS_Shape shape;
    TopTools_IndexedMapOfShape solids;  // the joined shape's solids
    std::vector<int> origin;            // for each, the file's solid it came from, from 1
    int sharedFaces = 0;
    QString error;
};

Joined join(const TopoDS_Shape &shape, const QString &file)
{
    Joined joined;
    std::vector<TopoDS_Shape> inputs;
    for (TopExp_Explorer e(shape, TopAbs_SOLID); e.More(); e.Next())
        inputs.push_back(e.Current());
    if (inputs.size() < 2) {
        joined.shape = shape;
        TopExp::MapShapes(shape, TopAbs_SOLID, joined.solids);
        joined.origin.assign(joined.solids.Extent(), 1);
        return joined;
    }
    BOPAlgo_Builder builder;
    for (const TopoDS_Shape &solid : inputs)
        builder.AddArgument(solid);
    builder.Perform();
    if (builder.HasErrors()) {
        joined.error = QString("OpenCASCADE could not join the solids of %1").arg(file);
        return joined;
    }
    joined.shape = builder.Shape();
    TopExp::MapShapes(joined.shape, TopAbs_SOLID, joined.solids);
    joined.origin.assign(joined.solids.Extent(), 0);
    for (size_t i = 0; i < inputs.size(); ++i) {
        std::vector<int> became;
        const TopTools_ListOfShape &modified = builder.Modified(inputs[i]);
        if (modified.IsEmpty())
            became.push_back(joined.solids.FindIndex(inputs[i]));
        for (const TopoDS_Shape &m : modified)
            became.push_back(joined.solids.FindIndex(m));
        for (int index : became) {
            if (index < 1) {
                joined.error = QString("OpenCASCADE lost solid %1 of %2 while joining").arg(i + 1).arg(file);
                return joined;
            }
            int &from = joined.origin[index - 1];
            if (from != 0) {
                joined.error = QString("solids %1 and %2 of %3 overlap; touching solids are "
                                       "joined, overlapping ones are a fault in the geometry")
                                   .arg(from).arg(i + 1).arg(file);
                return joined;
            }
            from = static_cast<int>(i + 1);
        }
    }
    TopTools_IndexedDataMapOfShapeListOfShape owners;
    TopExp::MapShapesAndAncestors(joined.shape, TopAbs_FACE, TopAbs_SOLID, owners);
    for (int f = 1; f <= owners.Extent(); ++f)
        joined.sharedFaces += owners(f).Extent() > 1 ? 1 : 0;
    return joined;
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

    // Netgen keeps global state (its messages, its current mesh and
    // geometry): one meshing at a time, whichever thread asks (a window
    // meshing in the background while another study opens). Found by the
    // window's walkthrough: two at once crashed inside Netgen.
    static std::mutex netgen;
    const std::lock_guard<std::mutex> one(netgen);
    ngcore::printmessage_importance = 0;  // Netgen's progress messages off

    const QString file = QFileInfo(stepPath).fileName();
    std::shared_ptr<netgen::OCCGeometry> geometry;
    auto mesh = std::make_shared<netgen::Mesh>();
    int fileSolids = 0;
    Joined joined;
    try {
        std::unique_ptr<netgen::OCCGeometry> read(netgen::LoadOCC_STEP(stepPath.toStdString()));
        fileSolids = read->somap.Extent();
        if (fileSolids == 0)
            return fail(QString("%1 holds no solid to mesh").arg(file));
        joined = join(read->shape, file);
        if (!joined.error.isEmpty())
            return fail(joined.error);
        geometry = std::make_shared<netgen::OCCGeometry>(joined.shape);
        netgen::MeshingParameters parameters;
        parameters.maxh = recipe.maxSize;
        mesh->SetGeometry(geometry);
        if (geometry->GenerateMesh(mesh, parameters) != 0)
            return fail("Netgen could not mesh " + file);
        if (recipe.order == 2)
            geometry->GetRefinement().MakeSecondOrder(*mesh);
    } catch (const std::exception &e) {
        return fail(QString("Netgen could not mesh %1: %2").arg(file, e.what()));
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

    // Netgen's domains are the joined shape's solids; each is named for
    // the file's solid it came from.
    const int solids = fileSolids;
    const int faces = geometry->fmap.Extent();
    std::vector<int> solidOfDomain(geometry->somap.Extent() + 1, 0);
    for (int d = 1; d <= geometry->somap.Extent(); ++d) {
        const int index = joined.solids.FindIndex(geometry->somap(d));
        if (index < 1)
            return fail(QString("lost track of a solid of %1 between joining and meshing").arg(file));
        solidOfDomain[d] = joined.origin[index - 1];
    }
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
        const int domain = el.GetIndex().Nr1();
        const int solid = domain >= 1 && domain < static_cast<int>(solidOfDomain.size())
                              ? solidOfDomain[domain]
                              : 0;  // from 1, in the file's order
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

    // The geometry's edges, for drawing and picking one and for a load
    // along it: each mesh segment (once: Netgen's own list holds each
    // once, seen 2026-11-18; the set keeps it so), split at its middle
    // node; each edge's ends, its curve's first
    // and last points; and whether it is straight. Numbered as Netgen
    // numbers OpenCASCADE's edges, from 1.
    const int edges = static_cast<int>(geometry->GetNEdges());
    std::set<std::array<vtkIdType, 3>> segments;  // edge, then the two points, smaller first
    auto addSegment = [&](int e, vtkIdType a, vtkIdType b) {
        segments.insert({e, std::min(a, b), std::max(a, b)});
    };
    for (const netgen::Segment &seg : mesh->LineSegments()) {
        const int e = mesh->GetEdgeDescriptor(seg).EdgeNr();
        const auto a = place.find(seg[0]), b = place.find(seg[1]);
        if (e < 1 || e > edges || a == place.end() || b == place.end())
            continue;
        const auto m = seg.GetNP() == 3 ? place.find(seg[2]) : place.end();
        if (m != place.end()) {
            addSegment(e, a->second, m->second);
            addSegment(e, m->second, b->second);
        } else {
            addSegment(e, a->second, b->second);
        }
    }
    vtkNew<vtkIntArray> edgeSegments;
    edgeSegments->SetName("edge segments");
    edgeSegments->SetNumberOfComponents(3);
    for (const auto &k : segments) {
        const int t[3] = {int(k[0]), int(k[1]), int(k[2])};
        edgeSegments->InsertNextTypedTuple(t);
    }
    vtkNew<vtkDoubleArray> edgeEnds;
    edgeEnds->SetName("edge ends");
    edgeEnds->SetNumberOfComponents(6);
    vtkNew<vtkIntArray> edgeStraight;
    edgeStraight->SetName("edge straight");
    for (int e = 0; e < edges; ++e) {
        // Netgen's edge e is the OCC edge map's e + 1 (OCCGeometry builds
        // its edges in that order, and its GetEdge(shape) counts on it).
        // Not through netgen::OCCEdge: Netgen does not export that class's
        // type, and the sanitized build's checks need it (CI, 2026-11-18).
        const netgen::GeometryEdge &edge = geometry->GetEdge(e);
        const netgen::Point<3> first = edge.GetPoint(0), last = edge.GetPoint(1);
        const double ends[6] = {first(0), first(1), first(2), last(0), last(1), last(2)};
        edgeEnds->InsertNextTypedTuple(ends);
        const TopoDS_Edge shape = TopoDS::Edge(geometry->emap(e + 1));
        edgeStraight->InsertNextValue(BRepAdaptor_Curve(shape).GetType() == GeomAbs_Line ? 1 : 0);
    }
    grid->GetFieldData()->AddArray(edgeSegments);
    grid->GetFieldData()->AddArray(edgeEnds);
    grid->GetFieldData()->AddArray(edgeStraight);

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
                      << "source: " + file
                      << QString("recipe: largest element %1, order %2").arg(recipe.maxSize).arg(recipe.order)
                      ;
    if (fileSolids > 1)
        series.provenance << QString("joined: %1, sharing %2")
                                 .arg(plural(fileSolids, "solid", "solids"),
                                      plural(joined.sharedFaces, "face", "faces"));
    series.provenance
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
