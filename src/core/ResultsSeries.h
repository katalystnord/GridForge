#pragma once

// Results on their way to ParaView: one unstructured grid per time point,
// and the provenance the files carry (tenet 9: which solver and version,
// what was converted, what was derived). Solver-neutral; a back end fills
// it, the writer writes it.

#include <QString>
#include <QStringList>

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <vector>

namespace gf::core {

struct TimePoint {
    double time = 0;
    vtkSmartPointer<vtkUnstructuredGrid> grid;
};

struct ResultsSeries {
    std::vector<TimePoint> points;
    QStringList provenance;  // "solver: CalculiX Version 2.23", ...; written into every file
};

// Writes <dir>/<base>.pvd and one <dir>/<base>_NNNN.vtu per time point.
// Returns the paths written, or an empty list with *error set.
QStringList writeVtuSeries(const ResultsSeries &series, const QString &dir, const QString &base,
                           QString *error);

} // namespace gf::core
