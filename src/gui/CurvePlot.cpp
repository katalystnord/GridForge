#include "gui/CurvePlot.h"

#include "gui/Style.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace gf::gui {

std::vector<double> niceTicks(double lo, double hi, int about)
{
    if (!(hi > lo))
        hi = lo + 1;
    const double rough = (hi - lo) / std::max(1, about);
    const int exponent = static_cast<int>(std::floor(std::log10(rough)));
    const double power = std::pow(10.0, exponent);
    int m = 10;
    for (int c : {1, 2, 5})
        if (c * power >= rough) {
            m = c;
            break;
        }
    // Each tick a whole number of steps, made as k m over (or times) a
    // power of ten, so 3 x 0.2 is 0.6 and not 0.6000000000000001.
    auto tick = [&](long long k) {
        return exponent < 0 ? double(k * m) / std::pow(10.0, -exponent) : double(k * m) * power;
    };
    const double step = m * power;
    std::vector<double> ticks;
    for (long long k = static_cast<long long>(std::floor(lo / step + 1e-9)); ; ++k) {
        ticks.push_back(tick(k));
        if (ticks.back() >= hi - 1e-9 * step)
            break;
    }
    return ticks;
}

CurvePlot::CurvePlot(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("curvePlot"));
    setMinimumSize(320, 180);
}

void CurvePlot::setCurves(const gf::core::TestCurve &measured, const gf::core::TestCurve &predicted)
{
    m_measured = measured;
    m_predicted = predicted;
    update();
}

void CurvePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    if (m_measured.strain.empty() && m_predicted.strain.empty()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter, tr("Study, Compare with a Test: a test's curve beside the run's"));
        return;
    }
    // The axes' reach: both curves, from 0.
    double maxStrain = 0, maxForce = 0;
    for (const auto *c : {&m_measured, &m_predicted})
        for (size_t i = 0; i < c->strain.size(); ++i) {
            maxStrain = std::max(maxStrain, 100 * c->strain[i]);
            maxForce = std::max(maxForce, c->force[i] / 1000);
        }
    const std::vector<double> xs = niceTicks(0, maxStrain), ys = niceTicks(0, maxForce);
    const QFontMetrics fm(font());
    const int left = fm.horizontalAdvance("00000") + 14, bottom = 2 * fm.height() + 8, top = 2 * fm.height(),
              right = 16;
    const QRectF area(left, top, width() - left - right, height() - top - bottom);
    auto at = [&](double strainPct, double kN) {
        return QPointF(area.left() + area.width() * strainPct / xs.back(),
                       area.bottom() - area.height() * kN / ys.back());
    };
    const QColor text = palette().color(QPalette::Text);
    QColor grid = text;
    grid.setAlpha(45);
    p.setPen(QPen(grid, 0.5));
    for (double x : xs) {
        p.drawLine(at(x, 0), at(x, ys.back()));
        p.setPen(text);
        p.drawText(QRectF(at(x, 0).x() - 30, area.bottom() + 2, 60, fm.height()), Qt::AlignCenter, QString::number(x));
        p.setPen(QPen(grid, 0.5));
    }
    for (double y : ys) {
        p.drawLine(at(0, y), at(xs.back(), y));
        p.setPen(text);
        p.drawText(QRectF(0, at(0, y).y() - fm.height() / 2.0, left - 6, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(y));
        p.setPen(QPen(grid, 0.5));
    }
    p.setPen(text);
    p.drawText(QRectF(area.left(), area.bottom() + fm.height() + 4, area.width(), fm.height()), Qt::AlignCenter,
               tr("strain (%)"));
    p.drawText(QRectF(0, 0, left + 80, fm.height() + 2), Qt::AlignLeft | Qt::AlignVCenter, tr("force (kN)"));
    // The legend, top right: a swatch and a name each.
    const QColor measuredColour = palette().color(QPalette::Mid).darker(130);
    int x = static_cast<int>(area.right());
    for (const auto &[name, colour] : {std::pair{tr("predicted"), ember}, std::pair{tr("measured"), measuredColour}}) {
        const int w = fm.horizontalAdvance(name);
        x -= w;
        p.setPen(text);
        p.drawText(QRectF(x, 0, w, fm.height() + 2), Qt::AlignLeft | Qt::AlignVCenter, name);
        x -= 22;
        p.setPen(QPen(colour, 2.5));
        p.drawLine(QPointF(x, fm.height() / 2.0 + 1), QPointF(x + 16, fm.height() / 2.0 + 1));
        x -= 14;
    }
    auto draw = [&](const gf::core::TestCurve &c, const QColor &colour, bool marks) {
        if (c.strain.empty())
            return;
        QPainterPath path(at(100 * c.strain[0], c.force[0] / 1000));
        for (size_t i = 1; i < c.strain.size(); ++i)
            path.lineTo(at(100 * c.strain[i], c.force[i] / 1000));
        p.setPen(QPen(colour, marks ? 2 : 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        if (marks) {
            p.setBrush(colour);
            for (size_t i = 0; i < c.strain.size(); ++i)
                p.drawEllipse(at(100 * c.strain[i], c.force[i] / 1000), 2.5, 2.5);
        }
    };
    draw(m_measured, measuredColour, false);
    draw(m_predicted, ember, true);
}

} // namespace gf::gui
