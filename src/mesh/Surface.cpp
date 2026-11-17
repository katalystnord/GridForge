#include "mesh/Surface.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <array>
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

} // namespace gf::mesh
