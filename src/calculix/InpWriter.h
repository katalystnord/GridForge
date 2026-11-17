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
//   *NSET, NSET=<name>        each node set, ten ids to a line; a name with
//                             a space or a comma written with underscores,
//                             and a comment line saying so
//   *ELSET, ELSET=<name>      each element set
//
// Version 1 writes tetrahedra; a grid with other cells is refused, naming
// the cell type. Two sets of one kind whose names would be written alike
// are refused.

#include "core/ResultsSeries.h"

#include <QString>

namespace gf::calculix {

// A name as a CalculiX deck can take it: spaces and commas become
// underscores (study spec, section 4.2).
QString inpName(const QString &name);

// A number as the deck writes it: the shortest text that reads back as the
// same double, never "-0", and never longer than 20 characters (ccx 2.23
// refuses a number with an exponent longer than 21): a longer one loses
// its last digits (14 significant digits left with a sign and a two-digit
// exponent, 5e-14 of it at worst; 13 with a three-digit one, 5e-13).
QString inpNumber(double x);

// The deck's text; empty with *error set if the mesh cannot be written
// (no grid, a cell type not written yet, two set names written alike).
QString meshInp(const gf::core::ResultsSeries &mesh, QString *error);

// meshInp written to a file; false with *error set.
bool writeMeshInp(const gf::core::ResultsSeries &mesh, const QString &path, QString *error);

} // namespace gf::calculix
