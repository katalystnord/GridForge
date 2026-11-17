#include "gui/Style.h"

#include <QApplication>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOptionTab>

namespace gf::gui {

Style::Style()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
}

void Style::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                        const QWidget *widget) const
{
    QProxyStyle::drawControl(element, option, painter, widget);
    if (element != CE_TabBarTabShape)
        return;
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
    if (!tab || !(tab->state & State_Selected))
        return;
    // The line along the tab's edge facing its page: the bottom for tabs on
    // top (North), as the layout and dock tabs are; the top for tabs below.
    const int thick = 3;
    QRect line = tab->rect.adjusted(1, 0, -1, 0);
    switch (tab->shape) {
    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        line.setHeight(thick);
        break;
    default:
        line.setTop(line.bottom() - thick + 1);
        break;
    }
    painter->fillRect(line, ember);
}

void applyStyle(QApplication &app)
{
    app.setStyle(new Style);
}

} // namespace gf::gui
