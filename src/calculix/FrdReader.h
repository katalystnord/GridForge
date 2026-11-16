#pragma once

// CalculiX results (.frd), read as the file says them. Written from the
// format's specification, the cgx manual's "Result Format" chapter
// (records 1C, 1U, 1P, 2C, 3C, 100C with -4, -5, -1, -2, -3; 9999 ends).
// Nothing here converts, derives or renames: that is the writer's job, and
// it says so (tenet 9).
//
// Read so far, each proven by a fixture written by ccx 2.23: the long ASCII
// format (FORMAT 1, *NODE FILE), the binary one (*NODE OUTPUT: nodes in
// float64, elements as int32, results in float32), and several increments
// in one file. Refused by name until a fixture proves them: the short ASCII
// format, other binary formats, more than six values per node in ASCII,
// material-dependent data, element types not listed below.

#include <QString>
#include <QStringList>

#include <vector>

namespace gf::calculix {

struct FrdNode {
    int id = 0;
    double x = 0, y = 0, z = 0;
};

// Element types by the number the file uses (cgx manual, "Element Types";
// te10 from ccx's own file). Only the numbers confirmed are accepted.
enum class FrdElementType {
    He8 = 1, Pe6 = 2, Te4 = 3, Pe15 = 5, Te10 = 6, Tr3 = 7, Qu4 = 9, Be2 = 11
};

struct FrdElement {
    int id = 0;
    FrdElementType type = FrdElementType::He8;
    int group = 0, material = 0;
    std::vector<int> nodes;
};

// One component of a dataset (the -5 record).
struct FrdComponent {
    QString name;          // "D1", "SZZ", ...
    int kind = 0;          // ICTYPE: 1 scalar, 2 vector, 4 matrix, 12 amplitude and phase
    int index1 = 0, index2 = 0;
    bool supplied = true;  // false: the file asks the reader to compute it (IEXIST 1), as "ALL"
};

// One block of nodal results (100C with its -4 record).
struct FrdDataset {
    QString name;             // "DISP", "STRESS", "TOSTRAIN", "FORC", ...
    double value = 0;         // the step's time, frequency or load factor
    int analysisType = 0;     // ICTYPE in 100C: 0 static, 1 time step, 2 frequency, ...
    int step = 0;             // NUMSTP; ccx counts increments here (1, 2, ...)
    QStringList parameters;   // the 1P records before it, as written; ccx's
                              // "STEP 6 2 1" is (dataset count, increment, step)
    std::vector<FrdComponent> components;
    std::vector<int> nodeIds;
    std::vector<double> values;  // per node, one value per supplied component

    int suppliedCount() const;
    // The value of a supplied component at a node; false if either is absent.
    bool valueAt(int nodeId, const QString &component, double *out) const;
};

struct FrdFile {
    QStringList userHeader;  // the 1U records, as written ("VERSION           Version 2.23")
    std::vector<FrdNode> nodes;
    std::vector<FrdElement> elements;
    std::vector<FrdDataset> datasets;

    QString error;  // empty when read; otherwise what and on which line
    bool ok() const { return error.isEmpty(); }

    const FrdNode *node(int id) const;
    const FrdDataset *dataset(const QString &name, int occurrence = 0) const;
};

FrdFile readFrd(const QString &path);

} // namespace gf::calculix
