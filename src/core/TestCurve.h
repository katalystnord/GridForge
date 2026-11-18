#pragma once

// Measured against predicted (ROADMAP Phase 5): a test's curve, as a
// testing program measured it, beside the same test as a solve predicts
// it. Both as force against the extensometer's strain: the measured one
// read from TenSense's analysed curve (its exports spec, section 3, a
// .vtp), the predicted one from a results file (core::writeVtkHdf's),
// each time point a point: the force on a group of faces (the sum of
// its nodes' reactions along the gauge), and the strain a virtual
// extensometer reads between two points of the part.

#include <QString>
#include <QStringList>

#include <array>
#include <vector>

namespace gf::core {

struct TestCurve {
    std::vector<double> strain;  // engineering strain, 1 (not %)
    std::vector<double> force;   // N
    QString source;              // where it came from, for the provenance
};

// TenSense's analysed curve (.vtp): its arrays "strain" and "force_N". On
// a fault, an empty curve and *error.
TestCurve readTenSenseCurve(const QString &path, QString *error);

// The curve a results file predicts: for each time point, the strain the
// extensometer from `a` to `b` reads (the change of their distance over
// it, the points moved with the part) and the force on the node set
// `group` along a to b (the sum of its reactions, RF). (0, 0) first. On a
// fault (no such set, no reactions, a point outside the part), an empty
// curve and *error.
TestCurve predictedCurve(const QString &resultsPath, const QString &group, const std::array<double, 3> &a,
                         const std::array<double, 3> &b, QString *error);

// The two side by side: at each predicted point inside the measured
// strains, the measured force there (linear between its samples, the
// first time the strain reaches it) and the difference.
struct CurveComparison {
    struct Point {
        double strain, predicted, measured;
        double difference() const { return (predicted - measured) / measured; }
    };
    std::vector<Point> points;
    int outside = 0;  // predicted points beyond the measured strains
};
CurveComparison compareCurves(const TestCurve &measured, const TestCurve &predicted);

// The comparison in words: the largest difference and where, the mean
// difference, over which strains.
QString comparisonWords(const CurveComparison &c);

} // namespace gf::core
