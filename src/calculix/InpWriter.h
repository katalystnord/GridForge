#pragma once

// The mesh part of a CalculiX deck (.inp): the nodes, the elements and
// the named sets, for a study's own deck to *INCLUDE. Materials, sections,
// steps, loads and supports are the set-up's (docs/specs/study-v1.md,
// section 4, to write), not this file's.
//
// Written from the ccx 2.23 manual's *NODE, *ELEMENT, *NSET and *ELSET
// pages, and checked by running ccx 2.23 on it (fixture box-gfmesh).
//
//   ** comment lines: GridForge's version and the mesh's provenance
//   *NODE, NSET=NALL          every node, by its "node id"
//   *ELEMENT, TYPE=C3D10, ELSET=EALL
//                             every element, by its "element id"; C3D4 and
//                             C3D10, whose node order is VTK's
//   *NSET, NSET=<name>        each node set, ten ids to a line
//   *ELSET, ELSET=<name>      each element set
//
// Version 1 writes tetrahedra; a grid with other cells is refused, naming
// the cell type.

#include "core/ResultsSeries.h"

#include <QString>

namespace gf::calculix {

// The deck's text; empty with *error set if the mesh cannot be written
// (no grid, a cell type not written yet, a set name ccx cannot take).
QString meshInp(const gf::core::ResultsSeries &mesh, QString *error);

// meshInp written to a file; false with *error set.
bool writeMeshInp(const gf::core::ResultsSeries &mesh, const QString &path, QString *error);

} // namespace gf::calculix
