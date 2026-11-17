#pragma once

// How a line in the Messages and Job docks is coloured, by what it means:
//   Problem   red     something failed or cannot go on
//   Warning   amber   a warning or note to read (not ember: never a selection)
//   Done      green   finished well: meshed, solved, 100%
//   Normal    text    progress
//   Detail    grey    residuals, where things are, what started

#include "core/JobRunner.h"

#include <QColor>
#include <QString>

class QPlainTextEdit;

namespace gf::gui {

enum class Tone { Normal, Detail, Done, Warning, Problem };

QColor toneColour(Tone tone);

// The tone of a line a toolkit wrote (toolkits spec, section 5).
Tone toneOf(const gf::core::ProtocolLine &line);

// Appends one line in its tone's colour, and keeps the end in view.
void appendLine(QPlainTextEdit *log, const QString &text, Tone tone);

} // namespace gf::gui
