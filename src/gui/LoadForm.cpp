#include "gui/LoadForm.h"

#include "gui/Style.h"
#include "gui/Tone.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

#include <cmath>

namespace gf::gui {

namespace {

using Kind = core::LoadChoice::Kind;
using From = core::Direction::From;

// The direction buttons, by id.
enum DirectionId { Normal = 0, X = 1, Y = 2, Z = 3, Edge = 4, Components = 5 };

bool isLoad(int kind) { return kind >= int(Kind::Move); }

// A measured number for people: what is nothing to rounding shown as 0.
QString shown(double v) { return std::abs(v) < 1e-9 ? QString("0") : QString::number(v, 'g', 6); }

QToolButton *choiceButton(const QString &text, const QString &name, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setObjectName(name);
    b->setCheckable(true);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    return b;
}

// As ParaView's Apply: ember when there is something to apply; else the
// window's own colours.
void light(QPushButton *b, bool on)
{
    if (!on) {
        b->setPalette(QPalette());
        return;
    }
    QPalette p = b->palette();
    p.setColor(QPalette::Button, ember);
    p.setColor(QPalette::ButtonText, Qt::white);
    b->setPalette(p);
}

} // namespace

LoadForm::LoadForm(const Context &context, const core::LoadChoice &start, const QString &place, QWidget *parent)
    : QWidget(parent), m_context(context), m_choice(start)
{
    setObjectName(QStringLiteral("loadForm"));
    // The choice made is told at a glance: ember, GridForge's colour for
    // what is selected.
    // A border of its own grey: palette(mid) matched the background under
    // some platforms' palettes, and the choices read as plain words.
    setStyleSheet(QString("QToolButton { padding: 3px 9px; border: 1px solid #a3a8b0; border-radius: 3px;"
                          " background: palette(button); }"
                          "QToolButton:checked { background: rgb(%1, %2, %3); color: white; border-color: rgb(%1, %2, %3); }"
                          "QToolButton:disabled { color: palette(mid); }")
                      .arg(ember.red())
                      .arg(ember.green())
                      .arg(ember.blue()));
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    // What: supports and loads as two rows of choices.
    m_kinds = new QButtonGroup(this);
    m_kinds->setExclusive(true);
    const char *const kindNames[6] = {"fixed", "axes", "symmetry", "move", "force", "pressure"};
    const QString kindTexts[6] = {tr("Fixed"), tr("Per direction"), tr("Symmetry"), tr("Move"), tr("Force"), tr("Pressure")};
    const QString kindTips[6] = {tr("Held in x, y and z"), tr("Each direction free, held or moved"),
                                 tr("No movement across the face; the face must be square to x, y or z"),
                                 tr("Moved by a given distance in a step"),
                                 tr("A total force, spread over the faces by area"),
                                 tr("Normal to the faces; positive pushes into them")};
    QWidget *rows[2] = {new QWidget(this), new QWidget(this)};
    for (QWidget *r : rows) {
        auto *h = new QHBoxLayout(r);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);
    }
    for (int k = 0; k < 6; ++k) {
        QToolButton *b = choiceButton(kindTexts[k], QString("kind:") + kindNames[k], rows[k / 3]);
        b->setToolTip(kindTips[k]);
        m_kinds->addButton(b, k);
        rows[k / 3]->layout()->addWidget(b);
    }
    static_cast<QHBoxLayout *>(rows[0]->layout())->addStretch();
    static_cast<QHBoxLayout *>(rows[1]->layout())->addStretch();
    form->addRow(tr("Support"), rows[0]);
    form->addRow(tr("Load"), rows[1]);

    // Where: from the start, or in a step.
    m_place = new QComboBox(this);
    m_place->setObjectName(QStringLiteral("place"));
    m_place->addItem(tr("From the start, in every step"), QStringLiteral("supports"));
    for (int i = 0; i < m_context.steps.size(); ++i)
        m_place->addItem(tr("In step %1").arg(m_context.steps[i]), QString("step:%1").arg(i));
    const int at = m_place->findData(place);
    // An item's own place; a new one's follows what it is (below).
    m_place->setCurrentIndex(m_context.editing && at >= 0 ? at : 0);
    m_place->setEnabled(!m_context.editing);  // moving an item between steps: not yet
    form->addRow(tr("Where"), m_place);

    // Direction: from the model first, components last.
    m_directionRow = new QWidget(this);
    auto *dirs = new QHBoxLayout(m_directionRow);
    dirs->setContentsMargins(0, 0, 0, 0);
    dirs->setSpacing(2);
    m_directions = new QButtonGroup(this);
    m_directions->setExclusive(true);
    const char *const dirNames[6] = {"normal", "x", "y", "z", "edge", "components"};
    const QString dirTexts[6] = {tr("Normal"), "X", "Y", "Z", tr("Edge"), tr("Components")};
    const QString dirTips[6] = {tr("Out of the faces, square to them"), tr("Along x; click again to turn it round"),
                                tr("Along y; click again to turn it round"), tr("Along z; click again to turn it round"),
                                tr("Along a straight edge, clicked in the view"), tr("Each direction given")};
    for (int d = 0; d < 6; ++d) {
        QToolButton *b = choiceButton(dirTexts[d], QString("dir:") + dirNames[d], m_directionRow);
        b->setToolTip(dirTips[d]);
        m_directions->addButton(b, d);
        dirs->addWidget(b);
    }
    m_directions->button(Edge)->setEnabled(bool(m_context.edge));
    auto *flip = new QToolButton(m_directionRow);
    flip->setObjectName(QStringLiteral("flip"));
    flip->setText(tr("Flip"));
    flip->setToolTip(tr("Turn the direction round"));
    dirs->addSpacing(8);
    dirs->addWidget(flip);
    dirs->addStretch();
    form->addRow(tr("Direction"), m_directionRow);
    m_directionText = new QLabel(this);
    m_directionText->setObjectName(QStringLiteral("direction"));
    m_directionText->setWordWrap(true);
    m_directionText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(QString(), m_directionText);

    // Size, with what it means.
    auto *sizeRow = new QWidget(this);
    auto *sizes = new QHBoxLayout(sizeRow);
    sizes->setContentsMargins(0, 0, 0, 0);
    m_size = new QLineEdit(sizeRow);
    m_size->setObjectName(QStringLiteral("magnitude"));
    m_size->setMaximumWidth(110);
    m_unit = new QLabel(sizeRow);
    m_unit->setWordWrap(true);
    sizes->addWidget(m_size);
    sizes->addWidget(m_unit, 1);
    m_sizeLabel = new QLabel(tr("Size"), this);
    form->addRow(m_sizeLabel, sizeRow);

    m_othersRow = m_others = new QComboBox(this);
    m_others->setObjectName(QStringLiteral("others"));
    m_others->addItems({tr("free"), tr("held where they are")});
    m_others->setToolTip(tr("The directions the move is not along"));
    form->addRow(tr("Others"), m_others);

    // Per direction: free, held or moved by a value.
    m_axesBox = new QWidget(this);
    auto *grid = new QGridLayout(m_axesBox);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        const QString axis(QChar('x' + i));
        grid->addWidget(new QLabel(axis, m_axesBox), i, 0);
        m_axisState[i] = new QComboBox(m_axesBox);
        m_axisState[i]->setObjectName("axis:" + axis);
        m_axisState[i]->addItems({tr("free"), tr("held"), tr("moved by")});
        m_axisValue[i] = new QLineEdit(m_axesBox);
        m_axisValue[i]->setObjectName("axisValue:" + axis);
        m_axisValue[i]->setMaximumWidth(90);
        grid->addWidget(m_axisState[i], i, 1);
        grid->addWidget(m_axisValue[i], i, 2);
        grid->addWidget(new QLabel(m_context.length, m_axesBox), i, 3);
    }
    grid->setColumnStretch(4, 1);
    form->addRow(tr("Directions"), m_axesBox);

    // A force by components.
    m_forceBox = new QWidget(this);
    auto *fgrid = new QGridLayout(m_forceBox);
    fgrid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        const QString axis(QChar('x' + i));
        fgrid->addWidget(new QLabel(axis, m_forceBox), i, 0);
        m_component[i] = new QLineEdit(m_forceBox);
        m_component[i]->setObjectName("component:" + axis);
        m_component[i]->setPlaceholderText("0");
        m_component[i]->setMaximumWidth(110);
        fgrid->addWidget(m_component[i], i, 1);
        fgrid->addWidget(new QLabel(m_context.force, m_forceBox), i, 2);
    }
    fgrid->setColumnStretch(3, 1);
    form->addRow(tr("Components"), m_forceBox);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    form->addRow(m_note);

    auto *buttons = new QWidget(this);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 6, 0, 6);
    m_apply = new QPushButton(tr("Apply"), buttons);
    m_apply->setObjectName(QStringLiteral("applyLoad"));
    row->addWidget(m_apply);
    if (m_context.editing) {
        auto *remove = new QPushButton(tr("Delete"), buttons);
        remove->setObjectName(QStringLiteral("removeLoad"));
        row->addWidget(remove);
        connect(remove, &QPushButton::clicked, this, &LoadForm::removeClicked);
    }
    row->addStretch();
    form->addRow(buttons);

    // The start shown.
    const bool fresh = !m_context.editing;
    if (!fresh)
        m_kinds->button(int(start.kind))->setChecked(true);
    m_reversed = start.direction.reversed;
    switch (start.direction.from) {
    case From::Axis:
        m_lastDirection = X + start.direction.axis;
        break;
    case From::Normal:
        m_lastDirection = Normal;
        break;
    case From::Edge:
        m_lastDirection = Edge;
        m_edge = start.direction.number;
        break;
    case From::Components:
        m_lastDirection = Components;
        break;
    }
    if (fresh) {
        // As Fusion's: along the faces' normal when they have one.
        QString problem;
        m_lastDirection = m_context.normal && m_context.normal(&problem) ? Normal : X;
        m_reversed = false;
    }
    m_directions->button(m_lastDirection)->setChecked(true);
    if (start.kind == Kind::Move || start.kind == Kind::Force || start.kind == Kind::Pressure)
        m_size->setText(QString::number(start.magnitude, 'g', 12));
    m_others->setCurrentIndex(start.othersHeld ? 1 : 0);
    for (int i = 0; i < 3; ++i) {
        m_axisState[i]->setCurrentIndex(int(start.axes[i].kind));
        if (start.axes[i].kind == core::AxisState::Kind::Value)
            m_axisValue[i]->setText(QString::number(start.axes[i].value, 'g', 12));
    }
    const double comps[3] = {start.components.x, start.components.y, start.components.z};
    for (int i = 0; i < 3; ++i)
        if (comps[i] != 0)
            m_component[i]->setText(QString::number(comps[i], 'g', 12));
    if (fresh)
        m_size->clear();

    connect(m_kinds, &QButtonGroup::idClicked, this, [this, place](int k) {
        // A load is given in a step (the one chosen in the Steps dock); a
        // support from the start, unless the place was chosen by hand.
        if (isLoad(k) && m_place->currentIndex() == 0)
            m_place->setCurrentIndex(std::max(1, m_place->findData(place)));
        else if (!isLoad(k) && !m_placeChosen && !m_context.editing)
            m_place->setCurrentIndex(0);
        showKind();
        changed();
    });
    connect(m_place, &QComboBox::activated, this, [this] { m_placeChosen = true; });
    connect(m_directions, &QButtonGroup::idClicked, this, [this](int d) {
        // An axis clicked again turns round (Altair Inspire's X, Y, Z).
        if (d == m_lastDirection && d >= X && d <= Z)
            m_reversed = !m_reversed;
        else if (d != m_lastDirection)
            m_reversed = false;
        m_lastDirection = d;
        showKind();
        changed();
        if (d == Edge)
            emit edgeWanted();  // each click: another edge may be wanted
    });
    connect(flip, &QToolButton::clicked, this, [this] {
        m_reversed = !m_reversed;
        if (m_lastDirection == Components) {
            for (QLineEdit *e : m_component)
                if (!e->text().isEmpty() && e->text() != "0")
                    e->setText(e->text().startsWith('-') ? e->text().mid(1) : "-" + e->text());
            for (QLineEdit *e : m_axisValue)
                if (!e->text().isEmpty() && e->text() != "0")
                    e->setText(e->text().startsWith('-') ? e->text().mid(1) : "-" + e->text());
        }
        showDirection();
        changed();
    });
    for (QLineEdit *e : findChildren<QLineEdit *>())
        connect(e, &QLineEdit::textEdited, this, &LoadForm::changed);
    for (QLineEdit *e : findChildren<QLineEdit *>())
        connect(e, &QLineEdit::returnPressed, this, [this] {
            if (m_apply->isEnabled())
                emit applyClicked();
        });
    for (QComboBox *c : findChildren<QComboBox *>())
        connect(c, &QComboBox::activated, this, [this] {
            showKind();
            changed();
        });
    connect(m_apply, &QPushButton::clicked, this, &LoadForm::applyClicked);

    showKind();
    m_apply->setEnabled(fresh && m_kinds->checkedId() >= 0);
    light(m_apply, m_apply->isEnabled());
}

Kind LoadForm::kind() const { return Kind(std::max(0, m_kinds->checkedId())); }

void LoadForm::showKind()
{
    auto *form = static_cast<QFormLayout *>(layout());
    const int k = m_kinds->checkedId();
    const bool chosen = k >= 0;
    const bool directed = k == int(Kind::Move) || k == int(Kind::Force);
    const bool byComponents = directed && m_lastDirection == Components;
    form->setRowVisible(m_place, chosen);
    form->setRowVisible(m_directionRow, directed);
    form->setRowVisible(m_directionText, directed);
    form->setRowVisible(m_size->parentWidget(), (directed && !byComponents) || k == int(Kind::Pressure));
    form->setRowVisible(m_others, k == int(Kind::Move) && !byComponents);
    form->setRowVisible(m_axesBox, k == int(Kind::Axes) || (k == int(Kind::Move) && byComponents));
    form->setRowVisible(m_forceBox, k == int(Kind::Force) && byComponents);
    for (int i = 0; i < 3; ++i)
        m_axisValue[i]->setEnabled(m_axisState[i]->currentIndex() == 2);
    if (k == int(Kind::Move))
        m_unit->setText(m_context.length);
    else if (k == int(Kind::Force))
        m_unit->setText(tr("%1, total, spread over the faces by area").arg(m_context.force));
    else if (k == int(Kind::Pressure))
        m_unit->setText(tr("%1, positive pushes into the faces").arg(m_context.stress));
    QString note;
    if (!chosen)
        note = tr("Choose a support or a load for these faces.");
    else if (k == int(Kind::Fixed))
        note = tr("Held in x, y and z.");
    else if (k == int(Kind::Symmetry))
        note = tr("No movement across the faces; they must be square to x, y or z.");
    m_note->setText(note);
    form->setRowVisible(m_note, !note.isEmpty());
    showDirection();
}

void LoadForm::showDirection()
{
    QString text;
    QColor colour = palette().color(QPalette::WindowText);
    switch (m_lastDirection) {
    case X:
    case Y:
    case Z:
        text = QString("%1%2").arg(m_reversed ? '-' : '+').arg(QChar('x' + m_lastDirection - X));
        break;
    case Normal: {
        QString problem;
        const auto n = m_context.normal ? m_context.normal(&problem) : std::nullopt;
        if (!n) {
            text = problem.isEmpty() ? tr("no faces to measure") : problem;
            colour = toneColour(Tone::Problem);
        } else {
            const double s = m_reversed ? -1 : 1;
            text = tr("%1 normal of face %2: x %3, y %4, z %5")
                       .arg(m_reversed ? tr("inward") : tr("outward"))
                       .arg(m_context.firstFace)
                       .arg(shown(s * n->x), shown(s * n->y), shown(s * n->z));
        }
        break;
    }
    case Edge: {
        QString problem;
        const auto e = m_edge > 0 && m_context.edge ? m_context.edge(m_edge, &problem) : std::nullopt;
        if (m_edge == 0) {
            text = tr("Click an edge of the model.");
            colour = toneColour(Tone::Warning);
        } else if (!e) {
            text = problem;
            colour = toneColour(Tone::Problem);
        } else {
            const double s = m_reversed ? -1 : 1;
            text = tr("%1 edge %2: x %3, y %4, z %5")
                       .arg(m_reversed ? tr("against") : tr("along"))
                       .arg(m_edge)
                       .arg(shown(s * e->x), shown(s * e->y), shown(s * e->z));
        }
        break;
    }
    default:
        text = tr("As given below.");
        break;
    }
    m_directionText->setText(text);
    QPalette p = m_directionText->palette();
    p.setColor(QPalette::WindowText, colour);
    m_directionText->setPalette(p);
}

void LoadForm::changed()
{
    // As ParaView's Apply: lit when there is something to apply.
    const bool ready = m_kinds->checkedId() >= 0;
    m_apply->setEnabled(ready);
    light(m_apply, ready);
}

void LoadForm::setEdge(int edge)
{
    m_edge = edge;
    m_reversed = false;
    m_lastDirection = Edge;
    m_directions->button(Edge)->setChecked(true);
    showKind();
    changed();
}

bool LoadForm::wantsEdge() const { return m_lastDirection == Edge; }

QString LoadForm::place() const { return m_place->currentData().toString(); }

core::LoadChoice LoadForm::choice() const
{
    core::LoadChoice c;
    c.group = m_choice.group;
    c.kind = kind();
    c.direction = core::Direction{};
    c.direction.reversed = m_reversed;
    switch (m_lastDirection) {
    case X:
    case Y:
    case Z:
        c.direction.from = From::Axis;
        c.direction.axis = m_lastDirection - X;
        break;
    case Normal:
        c.direction.from = From::Normal;
        c.direction.number = m_context.firstFace;
        if (m_context.normal)
            if (const auto n = m_context.normal(nullptr))
                c.unit = *n;
        break;
    case Edge:
        c.direction.from = From::Edge;
        c.direction.number = m_edge;
        if (m_context.edge && m_edge > 0)
            if (const auto e = m_context.edge(m_edge, nullptr))
                c.unit = *e;
        break;
    default:
        c.direction.from = From::Components;
        c.direction.reversed = false;
        break;
    }
    c.magnitude = m_size->text().trimmed().toDouble();
    c.othersHeld = m_others->currentIndex() == 1;
    for (int i = 0; i < 3; ++i)
        c.axes[i] = {core::AxisState::Kind(m_axisState[i]->currentIndex()), m_axisValue[i]->text().trimmed().toDouble()};
    c.components = {m_component[0]->text().trimmed().toDouble(), m_component[1]->text().trimmed().toDouble(),
                    m_component[2]->text().trimmed().toDouble()};
    return c;
}

QString LoadForm::problem() const
{
    const int k = m_kinds->checkedId();
    if (k < 0)
        return tr("choose a support or a load");
    auto number = [](const QLineEdit *e, bool blankIsZero) {
        bool ok = false;
        const QString t = e->text().trimmed();
        if (t.isEmpty())
            return blankIsZero;
        t.toDouble(&ok);
        return ok;
    };
    const bool directed = k == int(Kind::Move) || k == int(Kind::Force);
    if (directed && m_lastDirection == Normal) {
        QString why;
        if (!m_context.normal || !m_context.normal(&why))
            return why.isEmpty() ? tr("the faces have no one normal") : why;
    }
    if (directed && m_lastDirection == Edge) {
        if (m_edge == 0)
            return tr("click an edge of the model first");
        QString why;
        if (!m_context.edge || !m_context.edge(m_edge, &why))
            return why;
    }
    const bool byComponents = directed && m_lastDirection == Components;
    if ((directed && !byComponents) || k == int(Kind::Pressure)) {
        if (!number(m_size, false))
            return tr("the size is not a number");
        if (directed && m_size->text().toDouble() < 0)
            return tr("the size is below 0; Flip turns the direction round");
    }
    if (k == int(Kind::Axes) || (k == int(Kind::Move) && byComponents))
        for (int i = 0; i < 3; ++i)
            if (m_axisState[i]->currentIndex() == 2 && !number(m_axisValue[i], false))
                return tr("the value for %1 is not a number").arg(QChar('x' + i));
    if (k == int(Kind::Force) && byComponents)
        for (int i = 0; i < 3; ++i)
            if (!number(m_component[i], true))
                return tr("the force's %1 is not a number").arg(QChar('x' + i));
    return {};
}

} // namespace gf::gui
