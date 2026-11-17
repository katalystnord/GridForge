#pragma once

// The quality of a mesh, for people and for agents: what a solver will
// refuse (inverted or folded elements) and how good the shapes are.
// Solver-neutral and mesher-neutral: it measures a VTK grid, so a mesh from
// Gmsh is measured the same way as GridForge's own.
//
// Version 1 measures tetrahedra (linear and quadratic); other cells are
// counted and said to be not measured.
//
// Measures, for each tetrahedron (its corners):
//   mean ratio      12 (3V)^(2/3) / (sum of the squared edge lengths):
//                   1 for the regular tetrahedron, 0 for a flat one;
//   dihedral angle  the smallest angle between two of its faces, in
//                   degrees: 70.53 for the regular tetrahedron;
//   inverted        its corners turn the wrong way (VTK's order: corners
//                   0 1 2 counter-clockwise seen from corner 3), or it
//                   has no volume;
//   folded          (quadratic) its Jacobian is not positive at a corner,
//                   a middle node or its centre: a middle node pushed so
//                   far that the element turns inside out somewhere.

#include <QJsonObject>
#include <QStringList>

#include <vtkUnstructuredGrid.h>

#include <vector>

namespace gf::mesh {

struct Quality {
    long long tetrahedra = 0;           // linear and quadratic
    long long notMeasured = 0;          // cells of other types
    long long inverted = 0;
    long long folded = 0;               // quadratic only
    double minMeanRatio = 0, averageMeanRatio = 0;
    long long meanRatioBelow01 = 0, meanRatioBelow03 = 0;
    double minDihedral = 0;             // degrees
    std::vector<double> meanRatio;      // per cell; -1 for cells not measured
    std::vector<vtkIdType> worst;       // up to 5 cells, worst mean ratio first

    // No element a solver would refuse, among those measured.
    bool solverReady() const { return inverted == 0 && folded == 0; }
};

Quality measure(vtkUnstructuredGrid *grid);

// For agents: the report as JSON ("format": "gridforge-mesh-quality").
// Cells are named by their "element id" where the grid has one.
QJsonObject toJson(const Quality &quality, vtkUnstructuredGrid *grid);

// For people: a few lines, the verdict first.
QStringList summary(const Quality &quality);

} // namespace gf::mesh
