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

} // namespace gf::calculix
