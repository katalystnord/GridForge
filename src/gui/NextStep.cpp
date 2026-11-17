#include "gui/NextStep.h"

namespace gf::gui {

QString nextStep(const WindowState &s)
{
    using O = WindowState::Outcome;
    if (!s.studyOpen)
        return QStringLiteral("Open a study: File, Open Study (a folder with a study.json).");
    if (s.meshing)
        return QStringLiteral("Meshing %1 ... the window stays yours meanwhile.").arg(s.geometry);
    if (!s.meshProblem.isEmpty())
        return QStringLiteral("The mesh could not be made: %1. Fix the geometry or the recipe, then File, Reload.")
            .arg(s.meshProblem);
    if (s.running)
        return QStringLiteral("Solving with %1, run %2: %3 (%4%). Stop cancels and keeps its files.")
            .arg(s.toolkit)
            .arg(s.run)
            .arg(s.progress.isEmpty() ? QStringLiteral("starting") : s.progress)
            .arg(s.percent);
    if (s.outcome == O::Solved)
        return QStringLiteral("Run %1 solved. Step through the increments in Results, or Open in ParaView.").arg(s.run);
    if (s.outcome == O::Cancelled)
        return QStringLiteral("Run %1 cancelled; its files are kept. Press Run (F5) to solve again.").arg(s.run);
    if (s.outcome == O::Failed)
        return QStringLiteral("Run %1 failed: see the Job tab for %2's own words.").arg(s.run).arg(s.toolkit);
    if (s.meshed && s.canSolve)
        return QStringLiteral("Mesh ready. Press Run (F5) to solve with %1.").arg(s.toolkit);
    if (s.meshed)
        return QStringLiteral("Mesh ready. The study has no solve step to run.");
    return QStringLiteral("Open a study: File, Open Study (a folder with a study.json).");
}

} // namespace gf::gui
