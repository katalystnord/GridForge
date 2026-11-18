#pragma once

// Measured against predicted, drawn: a test's curve (grey, a line through
// its samples) and a solve's (ember, its points joined), force in kN
// against strain in %, as a Line Chart View would show them. Painted by
// hand: Qt Charts is not LGPL (CLAUDE.md, the stack).

#include "core/TestCurve.h"

#include <QWidget>

#include <vector>

namespace gf::gui {

// Tick marks for an axis from lo to hi: 1, 2 or 5 times a power of ten
// apart, about `about` of them, from a multiple at or below lo to one at
// or above hi.
std::vector<double> niceTicks(double lo, double hi, int about = 5);

class CurvePlot : public QWidget
{
    Q_OBJECT
public:
    explicit CurvePlot(QWidget *parent = nullptr);
    void setCurves(const gf::core::TestCurve &measured, const gf::core::TestCurve &predicted);
    const gf::core::TestCurve &measured() const { return m_measured; }
    const gf::core::TestCurve &predicted() const { return m_predicted; }
    QSize sizeHint() const override { return {480, 240}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    gf::core::TestCurve m_measured, m_predicted;
};

} // namespace gf::gui
