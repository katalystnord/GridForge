#pragma once

// The Properties form for a support or a load on a group of faces, laid
// out from the precedents (docs/survey/loads-and-supports.md):
//
//   Support   [Fixed] [Per direction] [Symmetry]
//   Load      [Move] [Force] [Pressure]
//   Where     From the start, in every step | In step Pull | ...
//   Direction [Normal] [X] [Y] [Z] [Edge] [Components]  [Flip]
//             +z                       (what it comes to, in words)
//   Size      2 mm                      (a force: "N, total, spread over the faces by area")
//   Others    free | held               (a move only)
//   [Apply] [Delete]
//
// An axis button clicked again turns the direction round, as Altair
// Inspire's do; Flip does the same for any direction. The form holds a
// core::LoadChoice; the window writes it (core::entryOf, addItem).

#include "core/LoadChoice.h"

#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>

class QButtonGroup;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace gf::gui {

class LoadForm : public QWidget
{
    Q_OBJECT
public:
    struct Context {
        QString length = "mm", force = "N", stress = "MPa";
        QStringList steps;  // the analysis steps' names
        // The group's faces' outward normal, measured on the mesh; none,
        // with *problem, if they have no one normal.
        std::function<std::optional<gf::core::Vec3>(QString *problem)> normal;
        int firstFace = 0;  // named in "outward normal of face <n>"
        bool editing = false;
    };

    // Shows `start`, placed at `place` ("supports" or "step:<i>").
    LoadForm(const Context &context, const gf::core::LoadChoice &start, const QString &place,
             QWidget *parent = nullptr);

    // The choice as the form now stands, the group left as given.
    gf::core::LoadChoice choice() const;
    QString place() const;
    // Why the choice cannot be written as it stands ("the size is not a
    // number"); empty if it can.
    QString problem() const;

signals:
    void applyClicked();
    void removeClicked();

private:
    void showKind();
    void showDirection();
    void changed();
    gf::core::LoadChoice::Kind kind() const;

    Context m_context;
    gf::core::LoadChoice m_choice;  // what the form started from, for its group and edge
    QButtonGroup *m_kinds = nullptr;
    QButtonGroup *m_directions = nullptr;
    int m_lastDirection = -1;
    bool m_reversed = false;
    QComboBox *m_place = nullptr;
    bool m_placeChosen = false;  // by hand: no longer follows the kind
    QWidget *m_directionRow = nullptr;
    QLabel *m_directionText = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLineEdit *m_size = nullptr;
    QLabel *m_unit = nullptr;
    QWidget *m_othersRow = nullptr;
    QComboBox *m_others = nullptr;
    QWidget *m_axesBox = nullptr;
    QComboBox *m_axisState[3] = {};
    QLineEdit *m_axisValue[3] = {};
    QWidget *m_forceBox = nullptr;
    QLineEdit *m_component[3] = {};
    QLabel *m_note = nullptr;
    QPushButton *m_apply = nullptr;
};

} // namespace gf::gui
