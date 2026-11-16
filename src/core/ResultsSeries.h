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

// A named part of the model, from the solver's deck: a node set by its
// points, an element set by its cells (indices into every time point's
// grid, which all share one mesh).
struct NamedSet {
    QString name;
    bool nodes = true;
    std::vector<vtkIdType> members;
};

struct ResultsSeries {
    std::vector<TimePoint> points;
    std::vector<NamedSet> sets;
    QStringList provenance;  // "solver: CalculiX Version 2.23", ...; written into every file
};

// Writes <dir>/<base>.pvd and one <dir>/<base>_NNNN.vtu per time point.
// Returns the paths written, or an empty list with *error set.
QStringList writeVtuSeries(const ResultsSeries &series, const QString &dir, const QString &base,
                           QString *error);

// Writes one VTKHDF file holding every time point: the whole model as the
// block Model, each set as a block under NodeSets (vertices at its points)
// or ElementSets (its cells), each with the point data and provenance.
// False with *error set if it cannot.
bool writeVtkHdf(const ResultsSeries &series, const QString &path, QString *error);

} // namespace gf::core
