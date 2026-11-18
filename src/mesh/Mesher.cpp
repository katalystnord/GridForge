#include "mesh/Mesher.h"

#include "core/FaceSignature.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>

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
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <gp_Lin.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <Standard_Version.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <algorithm>
#include <cstdio>
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

// A face's kind of surface, in words for study.json: OpenCASCADE's own
// kinds (GeomAbs_SurfaceType), as "plane", "cylinder", ...
static const char *surfaceKind(GeomAbs_SurfaceType t)
{
    switch (t) {
    case GeomAbs_Plane: return "plane";
    case GeomAbs_Cylinder: return "cylinder";
    case GeomAbs_Cone: return "cone";
    case GeomAbs_Sphere: return "sphere";
    case GeomAbs_Torus: return "torus";
    case GeomAbs_BezierSurface: return "Bezier surface";
    case GeomAbs_BSplineSurface: return "B-spline surface";
    case GeomAbs_SurfaceOfRevolution: return "surface of revolution";
    case GeomAbs_SurfaceOfExtrusion: return "surface of extrusion";
    case GeomAbs_OffsetSurface: return "offset surface";
    default: return "other";
    }
}

// The walls of a shape: how thick the wall behind each face is, and, for
// the thin ones, points across the face where the mesh is to be finer.
struct Walls {
    std::vector<double> thickness;  // per face of the map, 0 where no line met another face
    std::vector<std::pair<gp_Pnt, double>> finer;  // a point and its largest element
    std::vector<double> thin;  // the thin faces' thicknesses, for the provenance
    std::vector<int> thinFaces;  // and their numbers in the map
    std::vector<std::pair<int, double>> faceSize;  // each face made finer, and its size
};

// How thick the wall behind each face is: from points across the face, a
// line into the solid along the face's inward normal, to the first other
// face it meets; the shortest such. For a thin wall, the distance to the
// face opposite; for the rim of a wall, its height. The points are the
// centres of a coarse triangulation of the face, so a narrow face (a rim
// around a hole) is sampled as well as a wide one; the triangulation is
// taken off again after, so Netgen's own sizing of curved faces starts
// from the shape as it was.
//
// A face whose wall is thinner than throughWall + 1 largest elements gets
// points across it, no further apart than its thickness over throughWall
// + 1, each asking for that size there. Not over throughWall: a
// tetrahedron of edge t / 2 still spans a wall t thick, and Netgen makes
// it (a plate 1.5 mm thick meshed at 0.75 throughout: 5 interior nodes; at
// 0.5, 1028; measured 2026-11-18). Netgen takes a face's own size only
// along its edges (occgenmesh.cpp, "elements per edge"): in the middle of
// a plate 1.5 mm thick, 5 mm from an edge, the size grew back past the
// thickness, and one tetrahedron spanned it (seen 2026-11-18).
static Walls measureWalls(const TopoDS_Shape &shape, const TopTools_IndexedMapOfShape &faces, double maxSize,
                          int throughWall, double finer, const std::map<int, double> &faceSizes)
{
    Walls out;
    out.thickness.assign(faces.Extent(), 0);
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    const double size = std::sqrt(box.SquareExtent());
    // A power of ten near a millionth of the size, so the grid itself is
    // the same number on every machine.
    const double grid = std::pow(10.0, std::floor(std::log10(1e-6 * size)));
    BRepMesh_IncrementalMesh coarse(shape, 0.01 * size, false, 0.5, false);
    IntCurvesFace_ShapeIntersector cast;
    cast.Load(shape, 1e-7);
    for (int f = 1; f <= faces.Extent(); ++f) {
        const TopoDS_Face face = TopoDS::Face(faces(f));
        TopLoc_Location where;
        const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, where);
        if (tri.IsNull() || !tri->HasUVNodes())
            continue;
        BRepAdaptor_Surface surface(face);
        double nearest = 0;
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            const gp_XY uv = (tri->UVNode(a).XY() + tri->UVNode(b).XY() + tri->UVNode(c).XY()) / 3;
            BRepLProp_SLProps props(surface, uv.X(), uv.Y(), 1, 1e-9);
            if (!props.IsNormalDefined())
                continue;
            gp_Dir inward = props.Normal();
            if (face.Orientation() != TopAbs_REVERSED)
                inward.Reverse();
            // From a hair inside the face, so the face itself is not met.
            const gp_Pnt at = props.Value().Translated(gp_Vec(inward) * 1e-6);
            cast.Perform(gp_Lin(at, inward), 0, 1e30);
            if (!cast.IsDone())
                continue;
            for (int k = 1; k <= cast.NbPnt(); ++k) {
                const double d = cast.WParameter(k) + 1e-6;
                if (cast.WParameter(k) > 1e-9 && (nearest == 0 || d < nearest))
                    nearest = d;
            }
        }
        // Rounded to nine figures: the triangulation and the lines cast
        // through it differ in their last bits between processors (GitHub's
        // Xeon against a Ryzen, 2026-11-18, the first stage of the
        // trace to part), and so did the meshes they sized.
        nearest = nearest > 0 ? QString::number(nearest, 'g', 9).toDouble() : 0;
        out.thickness[f - 1] = nearest;
        const double h = nearest / (throughWall + 1) / finer;
        // A wall: at least four times as wide (the root of the face's
        // area) as it is thick. Not a part merely small for the element
        // size (a 10 mm box at 5: its sides are 24 mm wide), nor the
        // narrow side of a plate (20 x 1.5 mm, 10 from the side opposite).
        // Four is a judgement, not a measurement: where plate theory
        // starts to hold, a span of a few thicknesses.
        GProp_GProps area;
        BRepGProp::SurfaceProperties(face, area);
        const bool thin =
            !(throughWall <= 0 || nearest <= 0 || h >= maxSize / finer || 4 * nearest > std::sqrt(area.Mass()));
        // A size asked for the face (a group's "refine"), over finer as
        // every other size.
        const auto asked = faceSizes.find(f);
        const double wanted = std::min(thin ? h : INFINITY, asked != faceSizes.end() ? asked->second / finer : INFINITY);
        if (!(wanted < maxSize / finer))
            continue;
        if (thin) {
            out.thin.push_back(nearest);
            out.thinFaces.push_back(f);
        }
        out.faceSize.push_back({f, wanted});
        // Each triangle cut into k x k, k from its longest side, the
        // points put on the face's own surface through their parameters.
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            const gp_Pnt pa = tri->Node(a), pb = tri->Node(b), pc = tri->Node(c);
            const double longest = std::max({pa.Distance(pb), pb.Distance(pc), pc.Distance(pa)});
            const int k = std::max(1, static_cast<int>(std::ceil(longest / wanted)));
            const gp_XY ua = tri->UVNode(a).XY(), ub = tri->UVNode(b).XY(), uc = tri->UVNode(c).XY();
            for (int i = 0; i <= k; ++i)
                for (int j = 0; i + j <= k; ++j) {
                    const gp_XY uv = ua + (ub - ua) * (double(i) / k) + (uc - ua) * (double(j) / k);
                    // On a grid of a millionth of the shape's size, for
                    // the same reason as the thickness.
                    const gp_Pnt p = surface.Value(uv.X(), uv.Y());
                    auto snap = [&](double x) { return std::round(x / grid) * grid; };
                    out.finer.push_back({gp_Pnt(snap(p.X()), snap(p.Y()), snap(p.Z())), wanted});
                }
        }
    }
    BRepTools::Clean(shape);
    return out;
}

// Where a shape's surface is open: its edges that border one face only,
// and the faces (numbered in `faces`, from 1) they belong to. A seam (a
// cylinder's, where the face meets itself) and a degenerate edge (a
// cone's tip) border one face and are no gap.
struct Openings {
    int edges = 0;
    std::set<int> faces;
};

static Openings openings(const TopoDS_Shape &shape, const TopTools_IndexedMapOfShape &faces)
{
    Openings open;
    TopTools_IndexedDataMapOfShapeListOfShape owners;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, owners);
    for (int e = 1; e <= owners.Extent(); ++e) {
        const TopoDS_Edge edge = TopoDS::Edge(owners.FindKey(e));
        if (BRep_Tool::Degenerated(edge))
            continue;
        TopTools_IndexedMapOfShape distinct;
        for (const TopoDS_Shape &f : owners(e))
            distinct.Add(f);
        if (distinct.Extent() != 1 || BRep_Tool::IsClosed(edge, TopoDS::Face(distinct(1))))
            continue;
        ++open.edges;
        open.faces.insert(faces.FindIndex(distinct(1)));
    }
    return open;
}

// The geometry at `path`, a STEP (every root of it as one shape) or
// OpenCASCADE's own .brep, as FreeCAD's exportBrep writes it
// (docs/specs/groups-v1.md; no units in it: millimetres, FreeCAD's).
// Netgen's LoadOCC_STEP is not used: it keeps only the first root of a
// STEP, and a file with a root per body lost the rest without a word
// (2026-11-18); it also read names, colours and Netgen's own stored mesh
// sizes, which would have changed the mesh unseen. Null, with *why, if
// the file cannot be read.
static TopoDS_Shape readGeometry(const QString &path, QString *why)
{
    const QString file = QFileInfo(path).fileName();
    TopoDS_Shape shape;
    if (QFileInfo(path).suffix().compare("brep", Qt::CaseInsensitive) == 0
        || QFileInfo(path).suffix().compare("brp", Qt::CaseInsensitive) == 0) {
        BRep_Builder builder;
        if (!BRepTools::Read(shape, path.toUtf8().constData(), builder) || shape.IsNull())
            *why = QString("%1 could not be read as BREP").arg(file);
        return shape;
    }
    STEPControl_Reader reader;
    if (reader.ReadFile(path.toUtf8().constData()) != IFSelect_RetDone) {
        *why = QString("%1 could not be read as STEP").arg(file);
        return shape;
    }
    reader.TransferRoots();
    return reader.OneShape();
}

double suggestedSize(const QString &path, QString *error)
{
    QString why;
    const TopoDS_Shape shape = QFileInfo(path).isFile() ? readGeometry(path, &why) : TopoDS_Shape();
    if (why.isEmpty() && shape.IsNull())
        why = QString("no geometry at %1").arg(path);
    if (!why.isEmpty()) {
        if (error)
            *error = why;
        return 0;
    }
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    const double raw = std::sqrt(box.SquareExtent()) / 25 * (1 + 1e-9);
    const double power = std::pow(10.0, std::floor(std::log10(raw)));
    const double m = raw / power;
    if (error)
        error->clear();
    return (m >= 5 ? 5 : m >= 2 ? 2 : 1) * power;
}

Recipe recipeFrom(const QJsonObject &parameters)
{
    Recipe r;
    r.maxSize = parameters["maxSize"].toDouble();
    r.order = parameters["order"].toInt(r.order);
    r.throughWall = parameters["throughWall"].toInt(r.throughWall);
    r.finer = parameters["finer"].toDouble(r.finer);
    for (const QJsonValue &pair : parameters["faceSizes"].toArray())
        r.faceSizes[pair.toArray().at(0).toInt()] = pair.toArray().at(1).toDouble();
    return r;
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
    if (!(recipe.finer >= 1))
        return fail(QString("the recipe's finer is %1; it divides every size, and is 1 or more").arg(recipe.finer));
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
    QString settings;
    TopTools_IndexedMapOfShape joinedFaces;
    std::vector<double> thickness, thinFaces;
    Walls walls;
    std::vector<gf::core::SolidSignature> solidSignatures;
    try {
        QString unread;
        const TopoDS_Shape shape = readGeometry(stepPath, &unread);
        if (!unread.isEmpty())
            return fail(unread);
        // Each of the file's solids, its volume and centre, in the order
        // the solids are numbered (Solid<n>, as join() takes them).
        for (TopExp_Explorer e(shape, TopAbs_SOLID); e.More(); e.Next()) {
            GProp_GProps props;
            BRepGProp::VolumeProperties(e.Current(), props);
            const gp_Pnt c = props.CentreOfMass();
            solidSignatures.push_back({props.Mass(), {c.X(), c.Y(), c.Z()}});
        }
        TopTools_IndexedMapOfShape fileSolidMap;
        if (!shape.IsNull())
            TopExp::MapShapes(shape, TopAbs_SOLID, fileSolidMap);
        fileSolids = fileSolidMap.Extent();
        if (fileSolids == 0)
            return fail(QString("%1 holds no solid to mesh").arg(file));
        joined = join(shape, file);
        if (!joined.error.isEmpty())
            return fail(joined.error);
        // Thin walls: a face whose wall is thinner than throughWall
        // largest elements is given its thickness over throughWall as its
        // largest element, through Netgen's own shape properties (as its
        // Python face.maxh), read when the geometry is built and passed on
        // to the face's edges. Under the mutex: the properties are global.
        TopExp::MapShapes(joined.shape, TopAbs_FACE, joinedFaces);
        // A surface that does not close is no solid; Netgen given one ended
        // the program (2026-11-18). Said here, with where it is open.
        if (const Openings open = openings(joined.shape, joinedFaces); open.edges > 0) {
            QStringList named;
            for (int f : open.faces)
                named << QString::number(f);
            const QString last = named.takeLast();
            const QString list = named.isEmpty() ? last : named.join(", ") + " and " + last;
            return fail(QString("the surface of %1 does not close: %2 border%3 one face only, on %4 %5; a solid "
                                "needs a closed surface (a face missing, or two faces that do not meet, in the "
                                "geometry)")
                            .arg(file, plural(open.edges, "edge", "edges"), open.edges == 1 ? "s" : "",
                                 open.faces.size() == 1 ? "face" : "faces", list));
        }
        for (const auto &[f, size] : recipe.faceSizes)
            if (f < 1 || f > joinedFaces.Extent())
                return fail(QString("the recipe asks a size for face %1, and %2 has faces 1 to %3")
                                .arg(f).arg(file).arg(joinedFaces.Extent()));
            else if (!(size > 0))
                return fail(QString("the recipe asks a size of %1 for face %2; a size is above 0").arg(size).arg(f));
        walls = measureWalls(joined.shape, joinedFaces, recipe.maxSize, recipe.throughWall, recipe.finer,
                             recipe.faceSizes);
        thickness = walls.thickness;
        thinFaces = walls.thin;
        for (const auto &[f, size] : walls.faceSize)
            netgen::OCCGeometry::GetProperties(joinedFaces(f)).maxh = size;
        geometry = std::make_shared<netgen::OCCGeometry>(joined.shape);
        netgen::MeshingParameters parameters;
        parameters.maxh = recipe.maxSize / recipe.finer;
        // Every other setting the mesh depends on, given here rather than
        // left to Netgen's defaults, which may move between versions; the
        // values are those defaults at the pinned commit (meshtype.hpp).
        parameters.grading = 0.3;
        parameters.curvaturesafety = 2;
        parameters.segmentsperedge = 1;
        parameters.minh = 0;
        parameters.closeedgefac = std::nullopt;
        parameters.optimize2d = "smcmSmcmSmcm";
        parameters.optsteps2d = 3;
        parameters.optimize3d = "cmdDmustm";
        parameters.optsteps3d = 3;
        parameters.parallel_meshing = true;
        parameters.nthreads = 4;
        for (const auto &[p, h] : walls.finer)
            parameters.meshsize_points.Append(netgen::MeshingParameters::MeshSizePoint(
                netgen::Point<3>(p.X(), p.Y(), p.Z()), h));
        // Said as used, for the provenance.
        settings = QString("settings: grading %1, curvature safety %2, segments per edge %3, smallest element %4, "
                           "close edges %5, surface optimisation %6 x%7, volume optimisation %8 x%9, %10 threads")
                       .arg(parameters.grading)
                       .arg(parameters.curvaturesafety)
                       .arg(parameters.segmentsperedge)
                       .arg(parameters.minh)
                       .arg(parameters.closeedgefac ? QString::number(*parameters.closeedgefac) : QString("off"))
                       .arg(QString::fromStdString(parameters.optimize2d))
                       .arg(parameters.optsteps2d)
                       .arg(QString::fromStdString(parameters.optimize3d))
                       .arg(parameters.optsteps3d)
                       .arg(parameters.parallel_meshing ? parameters.nthreads : 1);
        mesh->SetGeometry(geometry);
        if (qEnvironmentVariableIsSet("GF_MESH_TRACE")) {
            // Stage by stage, a fingerprint after each, to find where two
            // machines' meshes of the same study part (2026-11-18): the
            // walls measured, then Netgen's edges, surface, volume and
            // its optimisation, each run on from the last.
            QCryptographicHash walled(QCryptographicHash::Sha256);
            for (double t : thickness)
                walled.addData(QByteArray::number(t, 'g', 17) + '\n');
            for (const auto &[p, h] : walls.finer)
                walled.addData(QString("%1 %2 %3 %4\n").arg(p.X(), 0, 'g', 17).arg(p.Y(), 0, 'g', 17)
                                   .arg(p.Z(), 0, 'g', 17).arg(h, 0, 'g', 17).toUtf8());
            std::fprintf(stderr, "trace: walls %zu faces, %zu points, %s\n", thickness.size(), walls.finer.size(),
                         walled.result().toHex().left(16).constData());
            const struct {
                int start, end;
                const char *name;
            } stages[] = {{netgen::MESHCONST_ANALYSE, netgen::MESHCONST_MESHEDGES, "edges"},
                          {netgen::MESHCONST_MESHSURFACE, netgen::MESHCONST_OPTSURFACE, "surface"},
                          {netgen::MESHCONST_MESHVOLUME, netgen::MESHCONST_MESHVOLUME, "volume"},
                          {netgen::MESHCONST_OPTVOLUME, netgen::MESHCONST_OPTVOLUME, "optimised"}};
            for (const auto &stage : stages) {
                netgen::MeshingParameters staged = parameters;
                staged.perfstepsstart = stage.start;
                staged.perfstepsend = stage.end;
                if (geometry->GenerateMesh(mesh, staged) != 0)
                    return fail("Netgen could not mesh " + file);
                QCryptographicHash h(QCryptographicHash::Sha256);
                for (netgen::PointIndex pi : mesh->Points().Range()) {
                    const netgen::Point<3> &p = (*mesh)[pi];
                    h.addData(QString("%1 %2 %3\n").arg(p(0), 0, 'g', 12).arg(p(1), 0, 'g', 12).arg(p(2), 0, 'g', 12)
                                  .toUtf8());
                }
                std::fprintf(stderr, "trace: %-9s %zu points, %zu segments, %zu surface, %zu volume elements, %s\n",
                             stage.name, size_t(mesh->GetNP()), size_t(mesh->GetNSeg()), size_t(mesh->GetNSE()),
                             size_t(mesh->GetNE()), h.result().toHex().left(16).constData());
            }
        } else if (geometry->GenerateMesh(mesh, parameters) != 0) {
            return fail("Netgen could not mesh " + file);
        }
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
        const TopoDS_Edge shape = TopoDS::Edge(geometry->emap(e + 1));
        // A degenerate edge (a ball's pole, a cone's tip) has no curve to
        // ask: it is its vertex, at both ends. Asked for points, it ended
        // the program (2026-11-18, a ball).
        if (BRep_Tool::Degenerated(shape)) {
            const gp_Pnt p = BRep_Tool::Pnt(TopExp::FirstVertex(shape));
            const double ends[6] = {p.X(), p.Y(), p.Z(), p.X(), p.Y(), p.Z()};
            edgeEnds->InsertNextTypedTuple(ends);
            edgeStraight->InsertNextValue(0);
            continue;
        }
        const netgen::GeometryEdge &edge = geometry->GetEdge(e);
        const netgen::Point<3> first = edge.GetPoint(0), last = edge.GetPoint(1);
        const double ends[6] = {first(0), first(1), first(2), last(0), last(1), last(2)};
        edgeEnds->InsertNextTypedTuple(ends);
        edgeStraight->InsertNextValue(BRepAdaptor_Curve(shape).GetType() == GeomAbs_Line ? 1 : 0);
    }
    grid->GetFieldData()->AddArray(edgeSegments);
    grid->GetFieldData()->AddArray(edgeEnds);
    grid->GetFieldData()->AddArray(edgeStraight);

    // Each face's signature (study spec 4.2): its kind, area and centre,
    // measured on the geometry, so a group can tell when its faces have
    // changed. Face n is the OCC face map's n, as Netgen's SurfNr.
    vtkNew<vtkStringArray> faceKind;
    faceKind->SetName("face kind");
    vtkNew<vtkDoubleArray> faceArea;
    faceArea->SetName("face area");
    vtkNew<vtkDoubleArray> faceCentre;
    faceCentre->SetName("face centre");
    faceCentre->SetNumberOfComponents(3);
    for (int f = 1; f <= faces; ++f) {
        const TopoDS_Face face = TopoDS::Face(geometry->fmap(f));
        faceKind->InsertNextValue(surfaceKind(BRepAdaptor_Surface(face).GetType()));
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        faceArea->InsertNextValue(props.Mass());
        const gp_Pnt c = props.CentreOfMass();
        const double centre[3] = {c.X(), c.Y(), c.Z()};
        faceCentre->InsertNextTypedTuple(centre);
    }
    // Each face's wall thickness, as measured for the thin walls.
    vtkNew<vtkDoubleArray> faceThickness;
    faceThickness->SetName("face thickness");
    for (int f = 1; f <= faces; ++f) {
        const int at = joinedFaces.FindIndex(geometry->fmap(f));
        faceThickness->InsertNextValue(at >= 1 ? thickness[at - 1] : 0);
    }
    grid->GetFieldData()->AddArray(faceThickness);
    vtkNew<vtkDoubleArray> solidVolume;
    solidVolume->SetName("solid volume");
    vtkNew<vtkDoubleArray> solidCentre;
    solidCentre->SetName("solid centre");
    solidCentre->SetNumberOfComponents(3);
    for (const gf::core::SolidSignature &v : solidSignatures) {
        solidVolume->InsertNextValue(v.volume);
        solidCentre->InsertNextTypedTuple(v.centre.data());
    }
    grid->GetFieldData()->AddArray(solidVolume);
    grid->GetFieldData()->AddArray(solidCentre);
    grid->GetFieldData()->AddArray(faceKind);
    grid->GetFieldData()->AddArray(faceArea);
    grid->GetFieldData()->AddArray(faceCentre);

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
                      << QString("recipe: largest element %1, order %2, %3")
                             .arg(recipe.maxSize)
                             .arg(recipe.order)
                             .arg((recipe.throughWall > 0 ? QString("%1 elements through a wall").arg(recipe.throughWall)
                                                          : QString("no elements added through thin walls"))
                                  + (recipe.finer != 1 ? QString(", every size over %1 (finer, for a convergence "
                                                                 "check)").arg(recipe.finer)
                                                       : QString()))
                      << settings
                      << QString("geometry kernel: OpenCASCADE %1").arg(OCC_VERSION_COMPLETE);
    if (!recipe.faceSizes.empty()) {
        QStringList asked;
        for (const auto &[f, size] : recipe.faceSizes)
            asked << QString("face %1 at %2").arg(f).arg(QString::number(size / recipe.finer, 'g', 6));
        series.provenance << "finer faces: " + asked.join(", ");
    }
    if (!thinFaces.empty()) {
        const auto [thinnest, thickest] = std::minmax_element(thinFaces.begin(), thinFaces.end());
        auto n = [](double x) { return QString::number(x, 'g', 6); };
        // A range as written: equal to six figures, one number.
        auto range = [&](double a, double b) { return n(a) == n(b) ? n(a) : n(a) + " to " + n(b); };
        series.provenance << QString("thin walls: %1 %2 thick, sized %3 for %4 elements through")
                                 .arg(plural(static_cast<int>(thinFaces.size()), "face", "faces"),
                                      range(*thinnest, *thickest),
                                      range(*thinnest / (recipe.throughWall + 1) / recipe.finer,
                                            *thickest / (recipe.throughWall + 1) / recipe.finer))
                                 .arg(recipe.throughWall);
    }
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
