#pragma once

// The analysis deck of a CalculiX job (study spec, sections 4.3 to 4.7):
// materials, sections, supports and steps from a study's set-up, around
// the mesh part (InpWriter) it *INCLUDEs. The mesh must carry the set-up's
// groups as sets (gf::mesh::addGroupSets).
//
//   ** who wrote it, the units, and what was entered by hand
//   *INCLUDE, INPUT=<mesh file>
//   *MATERIAL, NAME=<name>   *ELASTIC (E, nu), *DENSITY, the plastic block
//                            by *INCLUDE
//   *SOLID SECTION           each group of solids with its material
//   *BOUNDARY                the supports from the start
//   *STEP[, NLGEOM]          each analysis step: *STATIC, its supports and
//     ...                    displacement loads as *BOUNDARY, forces as
//   *END STEP                *CLOAD (spread by the outer faces' areas),
//                            pressure as *DLOAD per element face (P1 to
//                            P4), gravity as *DLOAD GRAV, the output asked
//                            for, the raw lines given by hand
//
// Materials' values from their cards are filled first
// (gf::core::fillFromCards); the comments say where each value came from.
// Refused by name: a solid with no material; a material without Young's
// modulus or Poisson's ratio; symmetry on a face not normal to x, y or z;
// an output not known; gravity on a material without density; a force or
// pressure on a group with no face on the outside; a step that replaces
// what came before (not written yet).

#include "core/ResultsSeries.h"
#include "core/Setup.h"

#include <QString>

namespace gf::calculix {

QString analysisInp(const gf::core::Setup &setup, const gf::core::ResultsSeries &mesh,
                    const QString &meshFile, QString *error);

} // namespace gf::calculix
