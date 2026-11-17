#pragma once

// What the window's Study tree shows (docs/survey/gui-layouts.md, proposal
// A): one tree from geometry to results, each step holding its supports
// and loads. Pure: a study in, titled items out, so the tree is tested
// without a window.
//
//   Geometry            dogbone.step
//   Mesh                GridForge (Netgen), largest element 3 mm, quadratic
//   Materials
//     Measured steel    card tensense-ro.FCMat, plastic
//   Groups
//     Fixed grip        face 12
//   Supports            in every step
//     Fixed: Fixed grip
//   Steps
//     Pull              static, large displacements
//       Displacement: Pulled grip     x 1.5, y 0, z 0 mm
//   Jobs
//     solve             CalculiX, not run
//   Results             none yet

#include "core/Study.h"

#include <QString>

#include <vector>

namespace gf::core {

struct OutlineItem {
    QString title;
    QString detail;  // shown beside the title, in the units of the study
    QString kind;    // geometry, mesh, materials, material, groups, group, supports,
                     // support, steps, step, load, jobs, job, results
    QString key;     // what it stands for, for the Properties dock:
                     // "material:Measured steel", "step:0", "load:0:1", ...
    std::vector<OutlineItem> children;
};

// The tree's top-level items, in order.
std::vector<OutlineItem> studyOutline(const Study &study);

} // namespace gf::core
