// The banner's next step for each state of the window.

#include "gui/NextStep.h"

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
    check(s, "Open a study: File, Open Study (a folder with a study.json).");
    s.studyOpen = true;
    s.geometry = "dogbone.step";
    s.meshing = true;
    check(s, "Meshing dogbone.step ... the window stays yours meanwhile.");
    s.meshing = false;
    s.meshProblem = "dogbone.step holds no solid to mesh";
    check(s, "The mesh could not be made: dogbone.step holds no solid to mesh. Fix the geometry or the recipe, then "
             "File, Reload.");
    s.meshProblem.clear();
    s.meshed = true;
    s.canSolve = true;
    s.toolkit = "CalculiX";
    check(s, "Mesh ready. Press Run (F5) to solve with CalculiX.");
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
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
