#pragma once

// gf_mesh: GridForge's mesher. Geometry in, a mesh out, the same mesh for
// the same geometry and recipe every time (tenet 10).
//
// The volume is filled by Netgen (LGPL-2.1), built from GridForge's fork
// (github.com/katalystnord/netgen, third_party/netgen) at the commit the
// submodule pins. What GridForge adds around it is written here.
//
// The mesh is handed on as the core's series: one time point holding the
// tetrahedra, each node and element numbered from 1 ("node id", "element
// id"), with sets for the geometry's parts:
//   Solid1, Solid2, ...   each solid's tetrahedra (element set) and their
//                         nodes (node set), in the file's order;
//   Face1, Face2, ...     the nodes on each face of the geometry, in
//                         OpenCASCADE's order of the faces.

#include "core/ResultsSeries.h"

#include <QString>

namespace gf::mesh {

struct Recipe {
    double maxSize = 0;  // the largest element edge, in the geometry's units; must be > 0
    int order = 1;       // 1: straight edges; 2: a middle node on every edge, on the geometry
};

// The engine as it will be named in the provenance: "Netgen v6.2.2608-...
// (github.com/katalystnord/netgen at 7420b2f9...)".
QString engine();

// Meshes a STEP file's solids with tetrahedra. An empty series with
// *error set if it cannot: the file unreadable, no solid in it, a recipe
// without a size, or the engine failing.
gf::core::ResultsSeries meshStep(const QString &stepPath, const Recipe &recipe, QString *error);

} // namespace gf::mesh
