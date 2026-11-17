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

// The Steps dock: a row per support or load (by kind and group), a
// column per analysis step. A cell holds what holds in that step: set
// there, or kept from an earlier step (what a step changes stays, study
// spec 4.5), or nothing. Its key is the item that set it, for Properties.
//
//                              Pull                Hold                 Push
//   Fixed: Fixed grip          fixed               fixed                fixed
//   Displacement: Pulled grip  x 1.5, y 0, z 0 mm  (kept)               x 0.5 mm
//   Force: Pulled grip                                                  y 50 N
struct StepCell {
    QString text;
    QString key;
    bool kept = false;
};

struct StepTable {
    struct Row {
        QString title;
        std::vector<StepCell> cells;  // one per step
    };
    QStringList steps;
    std::vector<Row> rows;
};

StepTable stepTable(const Study &study);

} // namespace gf::core
