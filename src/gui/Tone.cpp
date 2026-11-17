#include "gui/Tone.h"

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>

namespace gf::gui {

QColor toneColour(Tone tone)
{
    switch (tone) {
    case Tone::Problem:
        return {190, 35, 35};
    case Tone::Warning:
        return {170, 110, 0};
    case Tone::Done:
        return {30, 125, 65};
    case Tone::Detail:
        return {118, 118, 118};
    case Tone::Normal:
        break;
    }
    return {};  // invalid: the text's own colour
}

Tone toneOf(const gf::core::ProtocolLine &l)
{
    using Type = gf::core::ProtocolLine::Type;
    switch (l.type) {
    case Type::Progress:
        return l.fraction >= 1.0 ? Tone::Done : Tone::Normal;
    case Type::Metric:
        return Tone::Detail;
    case Type::Log:
        return l.level == "error" ? Tone::Problem : l.level == "warning" ? Tone::Warning : Tone::Detail;
    case Type::File:
        return Tone::Done;
    case Type::Other:
        return l.text.contains("*ERROR") ? Tone::Problem
               : l.text.contains("*WARNING") ? Tone::Warning
                                              : Tone::Detail;
    }
    return Tone::Normal;
}

void appendLine(QPlainTextEdit *log, const QString &text, Tone tone)
{
    QTextCharFormat format;
    const QColor colour = toneColour(tone);
    if (colour.isValid())
        format.setForeground(colour);
    else
        format.setForeground(log->palette().color(QPalette::Text));
    if (tone == Tone::Problem)
        format.setFontWeight(QFont::DemiBold);
    QTextCursor end(log->document());
    end.movePosition(QTextCursor::End);
    if (!log->document()->isEmpty())
        end.insertBlock();
    end.insertText(text, format);
    log->verticalScrollBar()->setValue(log->verticalScrollBar()->maximum());
}

} // namespace gf::gui
