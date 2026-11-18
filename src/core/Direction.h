#pragma once

// Where a load's direction came from (study spec 4.6, "direction"): an
// axis, a face's normal or an edge, as the user chose it in the window.
// For people and for the window to show the same choice again; the
// numbers beside it stay the record, and the solver never reads it. A
// direction taken from the geometry does not follow it when the geometry
// changes (ANSYS's help says the same of its own,
// docs/survey/loads-and-supports.md).
//
//   "+x" ... "-z"                  along an axis
//   "outward normal of face 6"     the face's normal, out of the solid
//   "inward normal of face 6"      into it
//   "along edge 14"                the edge from its first vertex to its last
//   "against edge 14"              the other way
//
// Given by components, there is none.

#include <QString>

#include <optional>

namespace gf::core {

struct Direction {
    enum class From { Components, Axis, Normal, Edge };
    From from = From::Components;
    int axis = 0;           // Axis: 0 x, 1 y, 2 z
    int number = 0;         // Normal: the face; Edge: the edge; from 1
    bool reversed = false;  // "-x"; the inward normal; against the edge
};

// The direction a text names, or none if it names none.
std::optional<Direction> parseDirection(const QString &text);

// Its text; empty for one given by components.
QString directionText(const Direction &direction);

} // namespace gf::core
