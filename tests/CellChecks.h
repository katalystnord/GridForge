#pragma once

// Checks on VTK cells, whichever file they came from: every node
// where VTK's cell definition expects it, and the volume they enclose.
// Shared by the hand-off tests (ccx's results, Gmsh's meshes).

#include <vtkCell.h>
#include <vtkCellType.h>
#include <vtkUnstructuredGrid.h>

#include <array>
#include <cmath>

namespace gf::test {

inline std::array<double, 3> point(vtkUnstructuredGrid *g, vtkIdType p)
{
    std::array<double, 3> x{};
    g->GetPoint(p, x.data());
    return x;
}

// Every face of every cell, as VTK defines its faces, has its normal
// pointing away from the cell's centre: the node order is VTK's, not
// mirrored or twisted. For quadratic cells, every edge's middle node sits
// halfway between its ends, as VTK's edge definition expects.
inline int misplacedCells(vtkUnstructuredGrid *g)
{
    int bad = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        vtkCell *cell = g->GetCell(c);
        std::array<double, 3> centre{};
        int corners = cell->GetNumberOfPoints();
        switch (cell->GetCellType()) {
        case VTK_QUADRATIC_TETRA: corners = 4; break;
        case VTK_QUADRATIC_WEDGE: corners = 6; break;
        case VTK_QUADRATIC_HEXAHEDRON: corners = 8; break;
        }
        for (int k = 0; k < corners; ++k)
            for (int d = 0; d < 3; ++d)
                centre[d] += point(g, cell->GetPointId(k))[d] / corners;
        bool ok = true;
        for (int f = 0; f < cell->GetNumberOfFaces(); ++f) {
            vtkCell *face = cell->GetFace(f);
            const auto a = point(g, face->GetPointId(0)), b = point(g, face->GetPointId(1)),
                       e = point(g, face->GetPointId(2));
            const std::array<double, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                v{e[0] - a[0], e[1] - a[1], e[2] - a[2]};
            const std::array<double, 3> n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                          u[0] * v[1] - u[1] * v[0]};
            const double out = n[0] * (a[0] - centre[0]) + n[1] * (a[1] - centre[1])
                               + n[2] * (a[2] - centre[2]);
            ok = ok && out > 0;
        }
        if (cell->GetCellType() == VTK_QUADRATIC_EDGE) {
            // A quadratic edge is its own edge: its third node sits halfway.
            const auto a = point(g, cell->GetPointId(0)), b = point(g, cell->GetPointId(1)),
                       m = point(g, cell->GetPointId(2));
            for (int d = 0; d < 3; ++d)
                ok = ok && std::abs(m[d] - (a[d] + b[d]) / 2) < 1e-9;
        }
        if (cell->GetCellDimension() == 2) {
            // A surface cell's corners turn one way round: not twisted.
            const int n = cell->GetCellType() == VTK_QUADRATIC_TRIANGLE || cell->GetCellType() == VTK_TRIANGLE ? 3 : 4;
            std::array<double, 3> first{};
            for (int k = 0; k < n; ++k) {
                const auto a = point(g, cell->GetPointId(k)), b = point(g, cell->GetPointId((k + 1) % n)),
                           e = point(g, cell->GetPointId((k + 2) % n));
                const std::array<double, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                    v{e[0] - b[0], e[1] - b[1], e[2] - b[2]};
                const std::array<double, 3> t{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                              u[0] * v[1] - u[1] * v[0]};
                if (k == 0)
                    first = t;
                ok = ok && t[0] * first[0] + t[1] * first[1] + t[2] * first[2] > 0;
            }
        }
        for (int k = 0; k < cell->GetNumberOfEdges(); ++k) {
            vtkCell *edge = cell->GetEdge(k);
            if (edge->GetNumberOfPoints() != 3)
                continue;
            const auto a = point(g, edge->GetPointId(0)), b = point(g, edge->GetPointId(1)),
                       m = point(g, edge->GetPointId(2));
            for (int d = 0; d < 3; ++d)
                ok = ok && std::abs(m[d] - (a[d] + b[d]) / 2) < 1e-9;
        }
        bad += ok ? 0 : 1;
    }
    return bad;
}

// The volume the 3D cells enclose, from their faces as VTK defines them
// (the divergence theorem over each face, its corners fanned into
// triangles). A cell whose faces turn inward counts negative, so a mesh
// with its cells inside out does not add up to its volume.
inline double enclosedVolume(vtkUnstructuredGrid *g)
{
    double total = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        vtkCell *cell = g->GetCell(c);
        if (cell->GetCellDimension() != 3)
            continue;
        for (int f = 0; f < cell->GetNumberOfFaces(); ++f) {
            vtkCell *face = cell->GetFace(f);
            const int corners = face->GetCellType() == VTK_QUADRATIC_TRIANGLE
                                        || face->GetCellType() == VTK_TRIANGLE
                                    ? 3
                                    : 4;
            const auto a = point(g, face->GetPointId(0));
            for (int k = 1; k + 1 < corners; ++k) {
                const auto b = point(g, face->GetPointId(k)), e = point(g, face->GetPointId(k + 1));
                total += (a[0] * (b[1] * e[2] - b[2] * e[1]) - a[1] * (b[0] * e[2] - b[2] * e[0])
                          + a[2] * (b[0] * e[1] - b[1] * e[0]))
                         / 6;
            }
        }
    }
    return total;
}

} // namespace gf::test
