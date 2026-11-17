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

vtkSmartPointer<vtkPolyData> outerSurface(const gf::core::ResultsSeries &mesh);

} // namespace gf::mesh
