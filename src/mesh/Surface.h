#pragma once

// The mesh's outer surface, for showing it and for picking its faces:
// a triangle for every tetrahedron face no other tetrahedron shares,
// through its three corner nodes, on the grid's own points (point i of
// the surface is point i of the grid). Each triangle carries, as the cell
// array "face", the face of the geometry it lies on, from gf_mesh's
// Face<n> sets (from 1); 0 if it lies on none.

#include "core/ResultsSeries.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

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

} // namespace gf::mesh
