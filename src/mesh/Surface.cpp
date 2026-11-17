#include "mesh/Surface.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <array>
#include <map>
#include <vector>

namespace gf::mesh {

vtkSmartPointer<vtkPolyData> outerSurface(const gf::core::ResultsSeries &mesh)
{
    auto out = vtkSmartPointer<vtkPolyData>::New();
    if (mesh.points.empty() || !mesh.points.front().grid)
        return out;
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    out->SetPoints(grid->GetPoints());

    // Each tetrahedron's four faces by their corner nodes (VTK's order: the
    // first four points are the corners, linear or quadratic); a face seen
    // once is on the outside. Kept in the order first met, so the same mesh
    // gives the same surface.
    using Key = std::array<vtkIdType, 3>;
    std::map<Key, std::pair<int, Key>> seen;  // sorted corners -> count, corners as met
    std::vector<Key> order;
    static const int sides[4][3] = {{0, 1, 3}, {1, 2, 3}, {2, 0, 3}, {0, 2, 1}};  // normals outward
    for (vtkIdType c = 0; c < grid->GetNumberOfCells(); ++c) {
        const int type = grid->GetCellType(c);
        if (type != VTK_TETRA && type != VTK_QUADRATIC_TETRA)
            continue;
        vtkIdType n;
        const vtkIdType *p;
        grid->GetCellPoints(c, n, p);
        for (const auto &side : sides) {
            const Key corners{p[side[0]], p[side[1]], p[side[2]]};
            Key key = corners;
            std::sort(key.begin(), key.end());
            auto [at, fresh] = seen.try_emplace(key, 0, corners);
            if (fresh)
                order.push_back(key);
            ++at->second.first;
        }
    }

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
    for (const Key &key : order) {
        const auto &[count, corners] = seen.at(key);
        if (count != 1)
            continue;
        triangles->InsertNextCell({corners[0], corners[1], corners[2]});
        // The face all three corners lie on; the lowest if several do.
        int on = 0;
        for (int f : facesOf[corners[0]]) {
            const auto has = [f](const std::vector<int> &v) { return std::find(v.begin(), v.end(), f) != v.end(); };
            if (has(facesOf[corners[1]]) && has(facesOf[corners[2]]) && (on == 0 || f < on))
                on = f;
        }
        face->InsertNextValue(on);
    }
    out->SetPolys(triangles);
    out->GetCellData()->AddArray(face);
    return out;
}

} // namespace gf::mesh
