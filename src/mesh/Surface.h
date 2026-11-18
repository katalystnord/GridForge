#pragma once

// The mesh's outer surface, for showing it and for picking its faces:
// a triangle for every tetrahedron face no other tetrahedron shares,
// through its three corner nodes, on the grid's own points (point i of
// the surface is point i of the grid). Each triangle carries, as the cell
// array "face", the face of the geometry it lies on, from gf_mesh's
// Face<n> sets (from 1); 0 if it lies on none.

#include "core/ResultsSeries.h"

#include <QString>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <optional>
#include <vector>

namespace gf::mesh {

//
// A quadratic tetrahedron's face is drawn as four triangles through its
// three middle nodes (which lie on the geometry), so curved faces are drawn
// curved; each of the four carries the face's number.
vtkSmartPointer<vtkPolyData> outerSurface(const gf::core::ResultsSeries &mesh);

// The element edges on the outer surface, each once, as lines on the
// grid's own points: corner to corner, or through the middle node (two
// segments) for a quadratic mesh. Drawn over outerSurface, they show the
// elements as they are, not the four triangles a quadratic face is drawn
// with.
vtkSmartPointer<vtkPolyData> outerEdges(const gf::core::ResultsSeries &mesh);

// The outward normal of geometry faces, measured on outerSurface's
// triangles (area weighted): for a load along a face's normal. None, with
// *problem saying why, if a face is not on the surface, is curved, or the
// faces do not all face the same way.
std::optional<std::array<double, 3>> outwardNormal(vtkPolyData *surface, const std::vector<int> &faces,
                                                   QString *problem);

// The faces' normals averaged by area, curved or not: which way they face
// as a whole, for drawing. Zero if they cancel or are not on the surface.
std::array<double, 3> meanNormal(vtkPolyData *surface, const std::vector<int> &faces);

// The centre of geometry faces (area weighted), where their loads are drawn.
std::array<double, 3> centreOf(vtkPolyData *surface, const std::vector<int> &faces);

// The geometry's edges, as gf_mesh hands them on with the mesh (the grid's
// field data: "edge segments", "edge ends", "edge straight"), numbered as
// Netgen numbers OpenCASCADE's edges (from 1). For drawing and picking an
// edge, and for a load along one.
int edgeCount(const gf::core::ResultsSeries &mesh);

// Each edge's mesh segments as lines on the grid's own points, each with
// the cell array "edge": corner to corner, or through the middle node.
vtkSmartPointer<vtkPolyData> geometryEdges(const gf::core::ResultsSeries &mesh);

// A straight edge's direction, from its first vertex to its last (its
// curve's own way), as a unit vector. None, with *problem, if the edge is
// curved or there is no such edge.
std::optional<std::array<double, 3>> edgeDirection(const gf::core::ResultsSeries &mesh, int edge, QString *problem);

} // namespace gf::mesh
