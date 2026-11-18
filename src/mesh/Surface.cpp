#include "mesh/Surface.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkIdList.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace gf::mesh {

namespace {

// A tetrahedron face seen once is on the outside: its corners as met (so
// the normal points out), and for a quadratic tetrahedron the middle node
// of each of its edges, corner 0-1, 1-2, 2-0.
struct OuterFace {
    std::array<vtkIdType, 3> corners;
    std::array<vtkIdType, 3> middles;
    bool quadratic = false;
};

// The tetrahedra's faces nothing else shares, in the order first met, so
// the same mesh gives the same surface.
std::vector<OuterFace> outerFaces(vtkUnstructuredGrid *grid)
{
    using Key = std::array<vtkIdType, 3>;
    std::map<Key, std::pair<int, OuterFace>> seen;  // sorted corners -> count, face
    std::vector<Key> order;
    // VTK's faces of a tetrahedron, normals outward, and its middle nodes:
    // point 4 on edge 0-1, 5 on 1-2, 6 on 0-2, 7 on 0-3, 8 on 1-3, 9 on 2-3.
    static const int sides[4][3] = {{0, 1, 3}, {1, 2, 3}, {2, 0, 3}, {0, 2, 1}};
    auto middle = [](int a, int b) {
        static const int at[4][4] = {{-1, 4, 6, 7}, {4, -1, 5, 8}, {6, 5, -1, 9}, {7, 8, 9, -1}};
        return at[a][b];
    };
    for (vtkIdType c = 0; c < grid->GetNumberOfCells(); ++c) {
        const int type = grid->GetCellType(c);
        if (type != VTK_TETRA && type != VTK_QUADRATIC_TETRA)
            continue;
        vtkIdType n;
        const vtkIdType *p;
        grid->GetCellPoints(c, n, p);
        for (const auto &side : sides) {
            OuterFace f;
            f.corners = {p[side[0]], p[side[1]], p[side[2]]};
            f.quadratic = type == VTK_QUADRATIC_TETRA;
            if (f.quadratic)
                f.middles = {p[middle(side[0], side[1])], p[middle(side[1], side[2])], p[middle(side[2], side[0])]};
            Key key = f.corners;
            std::sort(key.begin(), key.end());
            auto [at, fresh] = seen.try_emplace(key, 0, f);
            if (fresh)
                order.push_back(key);
            ++at->second.first;
        }
    }
    std::vector<OuterFace> out;
    for (const Key &key : order)
        if (const auto &[count, f] = seen.at(key); count == 1)
            out.push_back(f);
    return out;
}

} // namespace

vtkSmartPointer<vtkPolyData> outerSurface(const gf::core::ResultsSeries &mesh)
{
    auto out = vtkSmartPointer<vtkPolyData>::New();
    if (mesh.points.empty() || !mesh.points.front().grid)
        return out;
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    out->SetPoints(grid->GetPoints());

    // Which geometry faces each node lies on, from the Face<n> sets.
    std::vector<std::vector<int>> facesOf(grid->GetNumberOfPoints());
    for (const gf::core::NamedSet &s : mesh.sets) {
        if (!s.nodes || !s.name.startsWith("Face"))
            continue;
        bool ok = false;
        const int f = s.name.mid(4).toInt(&ok);
        if (!ok || f < 1)
            continue;
        for (vtkIdType m : s.members)
            if (m >= 0 && m < vtkIdType(facesOf.size()))
                facesOf[m].push_back(f);
    }

    vtkNew<vtkCellArray> triangles;
    vtkNew<vtkIntArray> face;
    face->SetName("face");
    for (const OuterFace &f : outerFaces(grid)) {
        const auto &c = f.corners;
        // The face all three corners lie on; the lowest if several do.
        int on = 0;
        for (int g : facesOf[c[0]]) {
            const auto has = [g](const std::vector<int> &v) { return std::find(v.begin(), v.end(), g) != v.end(); };
            if (has(facesOf[c[1]]) && has(facesOf[c[2]]) && (on == 0 || g < on))
                on = g;
        }
        if (f.quadratic) {
            // Four triangles through the middle nodes, each the way round
            // the face is.
            const auto &m = f.middles;  // 0-1, 1-2, 2-0
            for (const std::array<vtkIdType, 3> &t : {std::array<vtkIdType, 3>{c[0], m[0], m[2]},
                                                     std::array<vtkIdType, 3>{m[0], c[1], m[1]},
                                                     std::array<vtkIdType, 3>{m[2], m[1], c[2]},
                                                     std::array<vtkIdType, 3>{m[0], m[1], m[2]}}) {
                triangles->InsertNextCell({t[0], t[1], t[2]});
                face->InsertNextValue(on);
            }
        } else {
            triangles->InsertNextCell({c[0], c[1], c[2]});
            face->InsertNextValue(on);
        }
    }
    out->SetPolys(triangles);
    out->GetCellData()->AddArray(face);
    return out;
}

vtkSmartPointer<vtkPolyData> outerEdges(const gf::core::ResultsSeries &mesh)
{
    auto out = vtkSmartPointer<vtkPolyData>::New();
    if (mesh.points.empty() || !mesh.points.front().grid)
        return out;
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    out->SetPoints(grid->GetPoints());
    vtkNew<vtkCellArray> lines;
    std::set<std::pair<vtkIdType, vtkIdType>> drawn;
    for (const OuterFace &f : outerFaces(grid))
        for (int k = 0; k < 3; ++k) {
            const vtkIdType a = f.corners[k], b = f.corners[(k + 1) % 3];
            if (!drawn.insert({std::min(a, b), std::max(a, b)}).second)
                continue;  // the face beside it drew it
            if (f.quadratic) {
                lines->InsertNextCell({a, f.middles[k]});
                lines->InsertNextCell({f.middles[k], b});
            } else {
                lines->InsertNextCell({a, b});
            }
        }
    out->SetLines(lines);
    return out;
}

namespace {

// Each triangle of the faces asked for: its face, its area-weighted normal
// (half the cross product, out of the solid) and its centroid.
struct Piece {
    int face;
    std::array<double, 3> normal, centroid;
    double area;
};

std::vector<Piece> piecesOf(vtkPolyData *surface, const std::vector<int> &faces)
{
    std::vector<Piece> out;
    vtkDataArray *face = surface->GetCellData()->GetArray("face");
    if (!face)
        return out;
    vtkNew<vtkIdList> ids;
    for (vtkIdType c = 0; c < surface->GetNumberOfCells(); ++c) {
        const int f = int(face->GetComponent(c, 0));
        if (std::find(faces.begin(), faces.end(), f) == faces.end())
            continue;
        surface->GetCellPoints(c, ids);
        double a[3], b[3], d[3];
        surface->GetPoint(ids->GetId(0), a);
        surface->GetPoint(ids->GetId(1), b);
        surface->GetPoint(ids->GetId(2), d);
        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {d[0] - a[0], d[1] - a[1], d[2] - a[2]};
        const std::array<double, 3> n = {0.5 * (u[1] * v[2] - u[2] * v[1]), 0.5 * (u[2] * v[0] - u[0] * v[2]),
                                         0.5 * (u[0] * v[1] - u[1] * v[0])};
        out.push_back({f, n, {(a[0] + b[0] + d[0]) / 3, (a[1] + b[1] + d[1]) / 3, (a[2] + b[2] + d[2]) / 3},
                       std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])});
    }
    return out;
}

std::array<double, 3> unit(const std::array<double, 3> &v)
{
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return l > 0 ? std::array<double, 3>{v[0] / l, v[1] / l, v[2] / l} : v;
}

double dot(const std::array<double, 3> &a, const std::array<double, 3> &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

} // namespace

std::optional<std::array<double, 3>> outwardNormal(vtkPolyData *surface, const std::vector<int> &faces,
                                                   QString *problem)
{
    auto fail = [&](const QString &why) {
        if (problem)
            *problem = why;
        return std::nullopt;
    };
    // A face is flat when every triangle on it faces as the whole face does;
    // a millionth of a radian's leeway for rounding in the mesh's points.
    constexpr double flat = 1 - 1e-6;
    const std::vector<Piece> pieces = piecesOf(surface, faces);
    std::array<double, 3> total{}, first{};
    int firstFace = 0;
    for (int f : faces) {
        std::array<double, 3> sum{};
        bool any = false;
        for (const Piece &p : pieces)
            if (p.face == f) {
                any = true;
                for (int k = 0; k < 3; ++k)
                    sum[k] += p.normal[k];
            }
        if (!any)
            return fail(QString("face %1 is not on the surface").arg(f));
        const std::array<double, 3> n = unit(sum);
        for (const Piece &p : pieces)
            if (p.face == f && p.area > 0 && dot(unit(p.normal), n) < flat)
                return fail(QString("face %1 is curved, so it has no one normal").arg(f));
        if (firstFace == 0) {
            first = n;
            firstFace = f;
        } else if (dot(first, n) < flat) {
            return fail(QString("faces %1 and %2 do not face the same way").arg(std::min(firstFace, f)).arg(std::max(firstFace, f)));
        }
        for (int k = 0; k < 3; ++k)
            total[k] += sum[k];
    }
    if (firstFace == 0)
        return fail("no faces");
    if (problem)
        problem->clear();
    return unit(total);
}

std::array<double, 3> meanNormal(vtkPolyData *surface, const std::vector<int> &faces)
{
    std::array<double, 3> sum{};
    for (const Piece &p : piecesOf(surface, faces))
        for (int k = 0; k < 3; ++k)
            sum[k] += p.normal[k];
    return unit(sum);
}

std::array<double, 3> centreOf(vtkPolyData *surface, const std::vector<int> &faces)
{
    std::array<double, 3> c{};
    double area = 0;
    for (const Piece &p : piecesOf(surface, faces)) {
        for (int k = 0; k < 3; ++k)
            c[k] += p.area * p.centroid[k];
        area += p.area;
    }
    if (area > 0)
        for (double &x : c)
            x /= area;
    return c;
}

namespace {

vtkDataArray *fieldArray(const gf::core::ResultsSeries &mesh, const char *name)
{
    if (mesh.points.empty() || !mesh.points.front().grid)
        return nullptr;
    return mesh.points.front().grid->GetFieldData()->GetArray(name);
}

} // namespace

int edgeCount(const gf::core::ResultsSeries &mesh)
{
    vtkDataArray *ends = fieldArray(mesh, "edge ends");
    return ends ? int(ends->GetNumberOfTuples()) : 0;
}

vtkSmartPointer<vtkPolyData> geometryEdges(const gf::core::ResultsSeries &mesh)
{
    auto out = vtkSmartPointer<vtkPolyData>::New();
    vtkDataArray *segments = fieldArray(mesh, "edge segments");
    if (mesh.points.empty() || !segments)
        return out;
    out->SetPoints(mesh.points.front().grid->GetPoints());
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkIntArray> edge;
    edge->SetName("edge");
    for (vtkIdType i = 0; i < segments->GetNumberOfTuples(); ++i) {
        double t[3];
        segments->GetTuple(i, t);
        lines->InsertNextCell({vtkIdType(t[1]), vtkIdType(t[2])});
        edge->InsertNextValue(int(t[0]));
    }
    out->SetLines(lines);
    out->GetCellData()->AddArray(edge);
    return out;
}

std::optional<std::array<double, 3>> edgeDirection(const gf::core::ResultsSeries &mesh, int edge, QString *problem)
{
    auto fail = [&](const QString &why) {
        if (problem)
            *problem = why;
        return std::nullopt;
    };
    vtkDataArray *ends = fieldArray(mesh, "edge ends");
    vtkDataArray *straight = fieldArray(mesh, "edge straight");
    if (!ends || !straight || edge < 1 || edge > ends->GetNumberOfTuples())
        return fail(QString("there is no edge %1").arg(edge));
    if (straight->GetComponent(edge - 1, 0) == 0)
        return fail(QString("edge %1 is curved, so it has no one direction").arg(edge));
    double e[6];
    ends->GetTuple(edge - 1, e);
    const std::array<double, 3> d = unit({e[3] - e[0], e[4] - e[1], e[5] - e[2]});
    if (problem)
        problem->clear();
    return d;
}

std::optional<gf::core::FaceSignature> faceSignature(const gf::core::ResultsSeries &mesh, int face)
{
    if (mesh.points.empty() || !mesh.points.front().grid)
        return std::nullopt;
    vtkFieldData *data = mesh.points.front().grid->GetFieldData();
    auto *kind = vtkStringArray::SafeDownCast(data->GetAbstractArray("face kind"));
    vtkDataArray *area = data->GetArray("face area"), *centre = data->GetArray("face centre");
    if (!kind || !area || !centre || face < 1 || face > kind->GetNumberOfValues()
        || face > area->GetNumberOfTuples() || face > centre->GetNumberOfTuples())
        return std::nullopt;
    gf::core::FaceSignature s;
    s.kind = QString::fromStdString(kind->GetValue(face - 1));
    s.area = area->GetComponent(face - 1, 0);
    centre->GetTuple(face - 1, s.centre.data());
    return s;
}

std::optional<gf::core::SolidSignature> solidSignature(const gf::core::ResultsSeries &mesh, int solid)
{
    if (mesh.points.empty() || !mesh.points.front().grid)
        return std::nullopt;
    vtkFieldData *data = mesh.points.front().grid->GetFieldData();
    vtkDataArray *volume = data->GetArray("solid volume"), *centre = data->GetArray("solid centre");
    if (!volume || !centre || solid < 1 || solid > volume->GetNumberOfTuples() || solid > centre->GetNumberOfTuples())
        return std::nullopt;
    gf::core::SolidSignature s;
    s.volume = volume->GetComponent(solid - 1, 0);
    centre->GetTuple(solid - 1, s.centre.data());
    return s;
}

} // namespace gf::mesh
