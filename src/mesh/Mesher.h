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
//                         OpenCASCADE's order of the faces once joined.
//
// Solids that touch are joined first: the face they share is one face,
// meshed once, its nodes in both solids. Solids that overlap are refused,
// naming the two.

#include "core/ResultsSeries.h"

#include <QJsonObject>
#include <QString>

#include <map>

namespace gf::mesh {

struct Recipe {
    double maxSize = 0;  // the largest element edge, in the geometry's units; must be > 0
    int order = 1;       // 1: straight edges; 2: a middle node on every edge, on the geometry
    // Elements through a thin wall: a face whose wall, measured to the
    // face opposite it, would get fewer is made finer, to its thickness
    // over this + 1 (at t / 2 a tetrahedron still spans a wall t thick).
    // 0: as Netgen alone.
    int throughWall = 2;
    // Every size the recipe makes (the largest element, the thin walls')
    // divided by this: the same recipe, finer by one ratio throughout, for
    // a mesh convergence check (core/Convergence). 1: as given.
    double finer = 1;
    // A largest element of their own for some faces (numbered as the
    // mesh's Face<n> sets, from 1): the face and the solid under it meshed
    // at that size, grading out to the rest. From study.json's "refine",
    // its groups resolved to their faces (core::meshParameters).
    std::map<int, double> faceSizes;
};

// A mesh step's parameters as study.json keeps them ("maxSize", "order",
// "throughWall", "finer", and "faceSizes": [[face, size], ...] as
// core::meshParameters resolves "refine"), what is not given left at its
// default.
Recipe recipeFrom(const QJsonObject &parameters);

// A largest element to start from for the geometry at `path` (STEP or
// .brep): a twenty-fifth of its bounding box's diagonal, rounded down to
// 1, 2 or 5 times a power of ten. 0, with *error, if it cannot be read.
double suggestedSize(const QString &path, QString *error);

// The engine as it will be named in the provenance: "Netgen v6.2.2608-...
// (github.com/katalystnord/netgen at 7420b2f9...)".
QString engine();

// Meshes a STEP file's solids with tetrahedra. Safe from any thread; one
// meshing runs at a time (Netgen keeps global state). An empty series with
// *error set if it cannot: the file unreadable, no solid in it, a recipe
// without a size, or the engine failing.
gf::core::ResultsSeries meshStep(const QString &stepPath, const Recipe &recipe, QString *error);

} // namespace gf::mesh
