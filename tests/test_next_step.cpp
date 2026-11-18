// The banner's next step for each state of the window.

#include "gui/NextStep.h"
#include "gui/Tone.h"

#include <cstdio>

using namespace gf::gui;

int main()
{
    int failures = 0;
    auto check = [&](const WindowState &s, const char *expected) {
        const QString got = nextStep(s);
        if (got != expected) {
            std::printf("FAIL\n  got:      %s\n  expected: %s\n", qPrintable(got), expected);
            ++failures;
        }
    };
    WindowState s;
    check(s, "Open a study: File, Open Study, or make one from a part's geometry: File, New Study from Geometry.");
    s.studyOpen = true;
    s.geometry = "dogbone.step";
    s.meshing = true;
    check(s, "Meshing dogbone.step ... the window stays yours meanwhile; Stop ends it.");
    s.meshing = false;
    s.meshStopped = true;
    check(s, "Meshing stopped. Change the mesh's size in Properties, or File, Reload to mesh again.");
    s.meshStopped = false;
    s.meshProblem = "dogbone.step holds no solid to mesh";
    check(s, "The mesh could not be made: dogbone.step holds no solid to mesh. Fix the geometry or the recipe, then "
             "File, Reload.");
    s.meshProblem.clear();
    s.meshed = true;
    s.canSolve = true;
    s.toolkit = "CalculiX";
    // A study not set up yet: each next step said in turn, from the window
    // alone (the release walkthrough, ROADMAP Phase 3).
    s.hasMaterial = s.hasSupport = s.hasLoad = false;
    s.freecadGroups = 3;
    check(s, "Mesh ready. Add the 3 groups named in FreeCAD: Study, Add Groups From FreeCAD.");
    s.freecadGroups = 0;
    check(s, "Give the part a material: Study, Add Material.");
    s.hasMaterial = true;
    check(s, "Hold the part: click the faces it is held by, then choose a support in Properties.");
    s.hasSupport = true;
    check(s, "Load it: click the faces it is loaded on, then choose Move, Force or Pressure in Properties.");
    s.hasLoad = true;
    check(s, "Mesh ready. Press Run (F5) to solve with CalculiX.");
    // A group signed on faces the geometry no longer has (study spec 4.2):
    // said before a solve is tried, even after one.
    s.changedGroup = "Top";
    check(s, "Group Top's faces have changed with the geometry: select it in the Study tree to keep them as they are "
             "now, or pick them again.");
    s.outcome = WindowState::Outcome::Solved;
    s.run = 1;
    check(s, "Group Top's faces have changed with the geometry: select it in the Study tree to keep them as they are "
             "now, or pick them again.");
    s.outcome = WindowState::Outcome::None;
    s.changedGroup.clear();
    s.running = true;
    s.run = 2;
    check(s, "Solving with CalculiX, run 2: starting (0%). Stop cancels and keeps its files.");
    s.progress = "step 1, increment 7";
    s.percent = 42;
    check(s, "Solving with CalculiX, run 2: step 1, increment 7 (42%). Stop cancels and keeps its files.");
    s.running = false;
    s.outcome = WindowState::Outcome::Solved;
    check(s, "Run 2 solved. Step through the increments in Results, or Open in ParaView.");
    s.outcome = WindowState::Outcome::Cancelled;
    check(s, "Run 2 cancelled; its files are kept. Press Run (F5) to solve again.");
    s.outcome = WindowState::Outcome::Failed;
    check(s, "Run 2 failed: see the Job tab for CalculiX's own words.");
    s.outcome = WindowState::Outcome::None;
    s.canSolve = false;
    check(s, "Mesh ready. The study has no solve step to run.");
    // The tone of each kind of line a toolkit writes.
    using gf::core::ProtocolLine;
    using gf::core::readProtocolLine;
    auto tone = [&](const char *line, Tone expected) {
        if (toneOf(readProtocolLine(line)) != expected) {
            std::printf("FAIL tone of %s\n", line);
            ++failures;
        }
    };
    tone(R"({"type": "progress", "fraction": 0.42, "text": "step 1, increment 7"})", Tone::Normal);
    tone(R"({"type": "progress", "fraction": 1, "text": "step 1, increment 29"})", Tone::Done);
    tone(R"({"type": "metric", "name": "force residual", "value": 0.1})", Tone::Detail);
    tone(R"({"type": "log", "level": "info", "text": "ccx started"})", Tone::Detail);
    tone(R"({"type": "log", "level": "warning", "text": "PE below 0"})", Tone::Warning);
    tone(R"({"type": "log", "level": "error", "text": "*ERROR reading *NODE"})", Tone::Problem);
    tone(" *ERROR reading *MATERIAL. Card image:", Tone::Problem);
    tone(" *WARNING in contact: overclosure", Tone::Warning);
    tone(" Using up to 1 cpu(s) for spooles.", Tone::Detail);
    // Problems and warnings are told apart from the ember accent and each other.
    if (toneColour(Tone::Problem) == toneColour(Tone::Warning) || toneColour(Tone::Warning) == QColor(226, 92, 29)
        || toneColour(Tone::Normal).isValid()) {
        std::printf("FAIL tone colours\n");
        ++failures;
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
