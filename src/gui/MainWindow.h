#pragma once

// The application window: layout A of docs/survey/gui-layouts.md (David,
// 2026-11-17), ParaView's layout with one study tree. It binds widgets to
// gf_core and gf_mesh and holds no logic of its own (headless core, thin
// GUI): what the tree shows is core::studyOutline, what a mesh is comes
// from gf_mesh.
//
//   Study dock (top left)        the study's outline
//   Properties | Information     the selected item (below it, tabbed)
//   the view (centre)            Model and Results tabs; the mesh
//   Messages | Job | Steps | Deck  docks along the bottom
//   status bar                   what is happening, and the study's units
//
// The window never waits (tenet 7): meshing runs on another thread.

#include "core/JobRunner.h"
#include "core/ResultsSeries.h"
#include "core/Study.h"

#include <QFutureWatcher>
#include <QMainWindow>

class QComboBox;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QAction;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVTKOpenGLNativeWidget;
class QFormLayout;
class vtkRenderer;

namespace gf::gui {

// What meshing on another thread hands back: the mesh, or why not, and
// which opening of a study it was for.
struct MeshDone {
    gf::core::ResultsSeries mesh;
    QString error;
    int opening = 0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    // Waits for a meshing still running: it must not outlive the window.
    ~MainWindow() override;

    // Opens a study folder (or its study.json): fills the tree, the step
    // chooser and the units, and starts meshing it in the background.
    // False, and said in the Messages dock, if it cannot be read.
    bool openStudy(const QString &path);

    const gf::core::Study &study() const { return m_study; }
    // True from the start of meshing until the window has taken the mesh
    // in (or said why not), not only while the meshing thread runs.
    bool meshing() const { return m_meshPending; }

signals:
    void meshReady();

private:
    void buildActions();
    void buildDocks();
    void say(const QString &message, bool problem = false);
    void showProperties(QTreeWidgetItem *item);
    void startMeshing();
    void onMeshed();
    void setCamera(double dx, double dy, double dz, double ux, double uy, double uz);
    void run();
    void onJobLine(const gf::core::ProtocolLine &line);
    void onJobFinished(int exitCode, const QString &results, bool stopped);
    void setJobState(const QString &state);
    void updateActions();

    gf::core::Study m_study;
    gf::core::ResultsSeries m_mesh;
    int m_opening = 0;  // counts openStudy calls; a mesh for an older one is dropped
    bool m_meshPending = false;
    QFutureWatcher<MeshDone> m_meshWatch;
    gf::core::JobRunner m_runner;
    QString m_solveStep, m_toolkitName;
    int m_runNumber = 0;

    QVTKOpenGLNativeWidget *m_view = nullptr;
    vtkRenderer *m_renderer = nullptr;
    QTabBar *m_layouts = nullptr;
    QTreeWidget *m_tree = nullptr;
    QWidget *m_properties = nullptr;
    QFormLayout *m_propertyForm = nullptr;
    QLabel *m_information = nullptr;
    QPlainTextEdit *m_messages = nullptr;
    QPlainTextEdit *m_job = nullptr;
    QPlainTextEdit *m_deck = nullptr;
    QComboBox *m_step = nullptr;
    QLabel *m_units = nullptr;
    QProgressBar *m_progress = nullptr;
    QAction *m_runAction = nullptr;
    QAction *m_stopAction = nullptr;
};

} // namespace gf::gui
