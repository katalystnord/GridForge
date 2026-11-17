#pragma once

// A Gmsh mesh as a grid for ParaView: one time point holding the elements
// of the mesh's highest dimension (its volume, for a solid), each node and
// element keeping its tag ("node id", "element id"). Each physical group
// becomes a node set of the nodes of its elements; a group of the highest
// dimension is an element set too. Node order is put into VTK's here, each
// type only once a fixture has shown it right; the provenance says so.

#include "core/ResultsSeries.h"
#include "gmsh/MshReader.h"

namespace gf::gmsh {

// `source` is the file's name, for the provenance.
gf::core::ResultsSeries toSeries(const MshFile &msh, const QString &source, QString *error);

} // namespace gf::gmsh
