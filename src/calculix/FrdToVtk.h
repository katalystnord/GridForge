#pragma once

// CalculiX results (.frd) as grids for ParaView. Each solver dataset keeps
// its own name and component names (DISP with D1 D2 D3, STRESS with SXX ...
// SZX); what GridForge computes from them is added beside them, named
// "(derived)". Element types are mapped to VTK cells only once a fixture
// has shown the node order is right; others are refused by name.

#include "calculix/FrdReader.h"
#include "calculix/InpSets.h"
#include "core/ResultsSeries.h"

namespace gf::calculix {

// One time point per output increment (the 100C step number), in file
// order. `source` is the file's name, for the provenance.
gf::core::ResultsSeries toSeries(const FrdFile &frd, const QString &source, QString *error);

// The deck's named sets onto the series, by node and element id. Ids the
// results lack are left out, and the provenance says how many and of which
// set; `deck` is the deck's name, for the provenance.
void addSets(gf::core::ResultsSeries &series, const InpSets &sets, const QString &deck);

// What the results hold that cannot be physical, said, never changed
// (tenets 9 and 10). ccx computes element results at the integration
// points and extrapolates them to the nodes for the .frd; where a field
// changes sharply, the nodal values overshoot. Version 1: an equivalent
// plastic strain (PE) below 0 at a node, which cannot be (checked on
// examples/dogbone-plastic: none below 0 at its 6496 integration points,
// 65 nodes below 0 in the .frd, at the yield fronts). One line per field,
// over every time point; none when nothing is out of bounds.
QStringList extrapolationNotes(const gf::core::ResultsSeries &series);

} // namespace gf::calculix
