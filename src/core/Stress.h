#pragma once

// Quantities derived from a symmetric stress (or strain) tensor. Anything
// computed here is GridForge's, not the solver's, and is labelled
// "(derived)" wherever it is written (tenet 9).

#include <array>

namespace gf::core {

// Components in the solver's order: xx, yy, zz, xy, yz, zx.
struct SymmetricTensor {
    double xx = 0, yy = 0, zz = 0, xy = 0, yz = 0, zx = 0;
};

// sqrt(((s1 - s2)^2 + (s2 - s3)^2 + (s3 - s1)^2) / 2), from the components.
double vonMises(const SymmetricTensor &s);

// The three principal values, largest first.
std::array<double, 3> principalValues(const SymmetricTensor &s);

} // namespace gf::core
