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
#include "gui/NextStep.h"
#include "gui/ResultsFile.h"
#include "gui/Tone.h"
#include "core/ResultsSeries.h"
#include "core/Study.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QPoint>

#include <vtkSmartPointer.h>

#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QAction;
class QTabBar;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVTKOpenGLNativeWidget;
class QFormLayout;
class vtkRenderer;
class vtkActor;
class vtkScalarBarActor;
class vtkPolyData;
class vtkStaticCellLocator;

namespace gf::gui {

// What meshing on another thread hands back: the mesh, or why not, and
// which opening of a study it was for.
struct MeshDone {
    gf::core::ResultsSeries mesh;
    QString error;
    int opening = 0;
    size_t ownSets = 0;  // the mesh's own sets (Face<n>, Solid<n>) come first; the groups' after
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

    // Shows a run's results in the Results tab (and switches to it).
    bool showResults(const QString &resultsFile);

    // The command that opens a file in ParaView: paraview where it is on
    // PATH, else ParaView's Flatpak (sharing the file's folder), else none.
    static QStringList paraviewCommand(const QString &file, bool paraviewOnPath, bool flatpak);

    // Where the colour legend's box starts, as a fraction of the view's
    // width: at 0.86, or further left so its title (which VTK centres over
    // the box) ends 8 pixels inside the view.
    // The mesh as the next run gets it, with the groups' sets.
    const gf::core::ResultsSeries &mesh() const { return m_mesh; }

    // A factor for the deformed shape that makes the largest displacement
    // about a tenth of the model's size, rounded down to 1, 2 or 5 times a
    // power of ten; never below 1 (true scale).
    static double autoDeform(double modelSize, double largestDisplacement);

    static double legendLeft(double viewPixels, double boxPixels, double titlePixels);

signals:
    void meshReady();

protected:
    // A click in the view (pressed and let go without moving) picks a face.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildActions();
    void buildDocks();
    void say(const QString &message, Tone tone = Tone::Normal);
    void showProperties(QTreeWidgetItem *item);
    void startMeshing();
    void onMeshed();
    void setCamera(double dx, double dy, double dz, double ux, double uy, double uz);
    void run();
    void onJobLine(const gf::core::ProtocolLine &line);
    void onJobFinished(int exitCode, const QString &results, bool stopped);
    void setJobState(const QString &state);
    void updateActions();
    void showLayer(int layout);
    void placeLegend();
    void showIncrement(int k);
    QString latestResults() const;
    void updateBanner();
    void applyEdits();
    QString selectedKey() const;
    int faceAt(const QPoint &at);  // the geometry face drawn there; 0 if none
    void hover(int face);
    void pickAt(const QPoint &at, bool add);
    void lightFaces(const std::vector<int> &faces);
    void showPicked();
    void makeGroup();
    void showGroupEditing(const QString &group);
    void showDeck(const QString &path, int run);
    void fillSteps();

    gf::core::Study m_study;
    gf::core::ResultsSeries m_mesh;
    int m_opening = 0;  // counts openStudy calls; a mesh for an older one is dropped
    bool m_meshPending = false;
    QFutureWatcher<MeshDone> m_meshWatch;
    gf::core::JobRunner m_runner;
    QString m_solveStep, m_toolkitName;
    int m_runNumber = 0;
    QString m_runFolder;
    QString m_meshProblem, m_progressText;
    int m_percent = 0;
    WindowState::Outcome m_outcome = WindowState::Outcome::None;
    ResultsFile m_results;
    int m_increment = 0;
    vtkActor *m_meshActor = nullptr;     // owned by the renderer while shown
    vtkActor *m_resultsActor = nullptr;
    vtkScalarBarActor *m_scalarBar = nullptr;
    vtkSmartPointer<vtkPolyData> m_surface;  // the mesh as shown, each triangle with its geometry face
    size_t m_meshOwnSets = 0;
    std::vector<int> m_picked;  // faces picked in the view, in order
    std::vector<int> m_lit;     // faces lit in ember now
    int m_hovered = 0;          // the face under the mouse; 0 if none
    vtkSmartPointer<vtkStaticCellLocator> m_surfaceLocator;
    QPoint m_pressedAt{-1, -1};

    QVTKOpenGLNativeWidget *m_view = nullptr;
    vtkRenderer *m_renderer = nullptr;
    QTabBar *m_layouts = nullptr;
    QTreeWidget *m_tree = nullptr;
    QWidget *m_properties = nullptr;
    QFormLayout *m_propertyForm = nullptr;
    QString m_editKey;  // the item whose values the Properties fields edit
    QLabel *m_information = nullptr;
    QPlainTextEdit *m_messages = nullptr;
    QPlainTextEdit *m_job = nullptr;
    QPlainTextEdit *m_deck = nullptr;
    QTableWidget *m_steps = nullptr;
    QComboBox *m_step = nullptr;
    QLabel *m_units = nullptr;
    QLabel *m_banner = nullptr;
    QProgressBar *m_progress = nullptr;
    QAction *m_runAction = nullptr;
    QAction *m_stopAction = nullptr;
    QComboBox *m_colourBy = nullptr;
    QDoubleSpinBox *m_deform = nullptr;
    QAction *m_autoDeform = nullptr;
    QLabel *m_increments = nullptr;
    QAction *m_openParaView = nullptr;
    QAction *m_rescale = nullptr;
    QList<QAction *> m_vcr;  // first, back, forward, last
};

} // namespace gf::gui
