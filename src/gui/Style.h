#pragma once

// GridForge's look on top of Qt's Fusion style (David, 2026-11-17): Fusion
// draws everything, as ParaView's; this proxy adds GridForge's ember line
// under the selected tab of every tab bar (layout tabs, dock tabs). No
// style sheets.

#include <QColor>
#include <QProxyStyle>

class QApplication;

namespace gf::gui {

// GridForge's accent.
inline const QColor ember{226, 92, 29};

class Style : public QProxyStyle
{
public:
    Style();
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
};

// Puts the style on the application (call once, after QApplication).
void applyStyle(QApplication &app);

} // namespace gf::gui
