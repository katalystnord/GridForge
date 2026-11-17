#pragma once

// The banner's one next step (as TenSense's): what the window is doing, or
// what the user does next. Pure, so each state is tested without a window.

#include <QString>

namespace gf::gui {

struct WindowState {
    bool studyOpen = false;
    QString geometry;         // the geometry file's name
    bool meshing = false;
    QString meshProblem;      // why the mesh could not be made
    bool meshed = false;
    bool canSolve = false;    // the study has a solve step
    QString toolkit;          // the solve toolkit's name
    bool running = false;
    int run = 0;              // the run running, or the last
    QString progress;         // "step 1, increment 7"
    int percent = 0;
    enum class Outcome { None, Solved, Cancelled, Failed } outcome = Outcome::None;
};

QString nextStep(const WindowState &s);

} // namespace gf::gui
