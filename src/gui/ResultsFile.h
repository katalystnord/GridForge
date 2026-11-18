#pragma once

// A run's results as the window shows them: results.vtkhdf (written by the
// toolkit, core::writeVtkHdf), its time points (increments) and the fields
// it holds on the model. No widgets, so it is tested without a display.
//
// Fields offered: each point field of one component (PE, von Mises stress
// (derived)), and each of three as its magnitude ("DISP (magnitude)").
// Tensors (STRESS, TOSTRAIN) are left to ParaView, which shows them whole.

#include <QString>
#include <QStringList>

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <utility>
#include <array>
#include <vector>

#include <vtkHDFReader.h>

namespace gf::gui {

class ResultsFile
{
public:
    ResultsFile();
    ~ResultsFile();

    // False with *error set if the file cannot be read.
    bool open(const QString &path, QString *error);
    QString path() const { return m_path; }

    int timeCount() const { return static_cast<int>(m_times.size()); }
    double time(int k) const { return m_times.at(k); }
    QStringList fields() const { return m_fields; }
    // The field to show first: plastic strain, else von Mises stress, else
    // the first offered.
    QString preferredField() const;

    // The model (the file's Model block) at time point k, with the field
    // asked for as its point scalars (named "shown").
    // Moved by the displacements times `deform` (0: undeformed), as
    // ParaView's Warp By Vector; the colours are of the field all the same.
    vtkSmartPointer<vtkUnstructuredGrid> model(int k, const QString &field, double deform = 0);
    // Whether the file holds displacements to deform by.
    bool canDeform() const { return !m_displacement.isEmpty(); }
    // The largest displacement at time point k (0 if none).
    double largestDisplacement(int k);
    // The displacement of the point that moves most at time point k (0s if none).
    std::array<double, 3> largestDisplacementVector(int k);
    std::pair<double, double> range(int k, const QString &field);

private:
    QString m_path;
    vtkSmartPointer<vtkHDFReader> m_reader;
    std::vector<double> m_times;
    QStringList m_fields;
    QString m_displacement;  // the point field of displacements, if any
};

} // namespace gf::gui
