#pragma once

// A set-up's named groups (study spec, section 4.2) as sets of a mesh
// from gf_mesh: a group of faces becomes a node set, the nodes of its
// faces (gf_mesh's Face<n> sets); a group of solids an element set and a
// node set (Solid<n>). Each named as the group.

#include "core/ResultsSeries.h"
#include "core/Setup.h"

namespace gf::mesh {

// Adds the groups' sets to the mesh. False with *error set if a group
// names a face or solid the mesh does not have.
bool addGroupSets(gf::core::ResultsSeries &mesh, const gf::core::Setup &setup, QString *error);

} // namespace gf::mesh
