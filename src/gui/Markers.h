#pragma once

// What the window draws for a step's supports and loads
// (docs/survey/loads-and-supports.md): an arrow in the true direction for
// a movement, a force or a pressure; a cone per held direction for a
// support, as PrePoMax's and Abaqus's (a stemless arrow); each at the
// centre of its group's faces, at one size, not scaled by the load (as
// every precedent surveyed), coloured by kind. Pure: a study, a step and
// the mesh's outer surface in, markers out, so it is tested without a
// window.

#include "core/Study.h"

#include <QString>

#include <vtkPolyData.h>

#include <array>
#include <vector>

namespace gf::gui {

struct Marker {
    enum class Shape { Arrow, Cone };
    Shape shape = Shape::Arrow;
    QString role;                    // "support", "move", "force", "pressure"
    QString key;                     // the item it shows ("load:0:1")
    std::array<double, 3> at{};      // on the faces: their centre
    std::array<double, 3> direction{};  // unit; a cone points along it, its tip at `at`
    bool outward = true;             // an arrow drawn out of the faces from `at`; else its tip at `at`
};

// The markers for what holds in step `step` (set there or kept from
// before, as the Steps dock shows it). None without a surface.
std::vector<Marker> markersFor(const gf::core::Study &study, int step, vtkPolyData *surface);

} // namespace gf::gui
