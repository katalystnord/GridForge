#pragma once

// Named sets from a CalculiX deck (.inp). The .frd carries no set names,
// so the hand-off reads them from the deck that made the results. Only
// sets are read here: *NODE and *ELEMENT (their NSET= and ELSET=), *NSET
// and *ELSET (ids, other sets' names, GENERATE), *INCLUDE. Every other
// keyword is passed over. Written from the ccx 2.23 manual's keyword
// pages and checked against what ccx itself accepted (fixture bar-sets).

#include <QString>
#include <QStringList>

#include <vector>

namespace gf::calculix {

struct InpSet {
    QString name;           // as the deck first spells it; ccx ignores case
    bool nodes = true;      // a node set (*NSET) or an element set (*ELSET)
    std::vector<int> ids;   // in the deck's order, each once
};

struct InpSets {
    std::vector<InpSet> sets;  // in the order the deck first names them
    QStringList files;         // the deck, then each included file
    QString error;             // empty when read; otherwise what, where
    bool ok() const { return error.isEmpty(); }

    const InpSet *find(const QString &name, bool nodes) const;
};

InpSets readInpSets(const QString &path);

} // namespace gf::calculix
