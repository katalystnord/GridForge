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
#include "core/MeshRun.h"
#include "gui/NextStep.h"
#include "gui/ResultsFile.h"
#include "gui/Tone.h"
#include "core/ResultsSeries.h"
#include "core/Study.h"

#include <QFutureWatcher>
#include <QJsonObject>
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QTemporaryDir>

#include <vtkSmartPointer.h>

#include <array>
#include <utility>
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

class LoadForm;

// What meshing on another thread hands back: the mesh, or why not, and
// which opening of a study it was for.
struct MeshDone {
    gf::core::ResultsSeries mesh;
    QString error;
    QString groupProblem;  // the groups do not fit the mesh (the mesh is fine)
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
    // A new study in `folder` for the geometry at `geometry` (a STEP or
    // FreeCAD's .brep, with its group file if beside it), opened; its
    // mesh size suggested from the part's size.
    bool newStudy(const QString &geometry, const QString &folder);

    static double autoDeform(double modelSize, double largestDisplacement);

    // A view (the direction looked in, and up) turned to show a movement
    // along `moved`: tipped in the plane of the view and the movement until
    // it is at least 70 degrees from it, so the movement is seen at 94 % of
    // its length and the view keeps some depth; up kept as near as it was.
    // A view already that far across, or nothing moved, is left as given.
    static std::pair<std::array<double, 3>, std::array<double, 3>>
    viewAcross(const std::array<double, 3> &view, const std::array<double, 3> &up, const std::array<double, 3> &moved);

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
    void onMeshRunEnded(const gf::core::MeshEnd &end);
    void meshArrived(const MeshDone &done);
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
    // The group the faces are already, or a free name for a new one ("Face 6").
    QString groupFor(const std::vector<int> &faces) const;
    // The support and load form: for the picked faces (no key), or for the
    // item at `key` as study.json holds it.
    void showLoadForm(const QString &key, const QJsonObject &entry);
    void applyLoad(LoadForm *form, const QString &key);
    void showGroupEditing(const QString &group);
    void showDeck(const QString &path, int run);
    void fillSteps();
    // The supports and loads that hold in the step chosen in the toolbar,
    // drawn on the model; the selected item's in ember.
    void refreshMarkers();
    // Each signed group checked against the mesh (study spec 4.2): one
    // whose faces changed is marked in the tree, said, and stops a solve.
    void checkGroups();
    // The group file FreeCAD's macro writes beside the study's geometry
    // (<name>.groups.json, docs/specs/groups-v1.md); empty if there is none.
    QString freecadGroupsFile() const;
    // Said once a mesh is ready, if that file holds groups the study lacks.
    void offerFreecadGroups();
    void addFreecadGroups();
    // How many of the FreeCAD group file's groups the study lacks.
    int freecadGroupsMissing() const;
    // Study, Add Material: its form in Properties (a FreeCAD card or values,
    // assigned to a group of solids).
    void showMaterialForm();
    // The picked faces' signatures, as the mesh measured them.
    std::map<int, gf::core::FaceSignature> signaturesOf(const std::vector<int> &faces) const;
    // Edge picking for a load along an edge: the geometry's edges drawn,
    // the next click in the view picks the nearest for `form`.
    void startEdgePick(LoadForm *form);
    void pickEdgeAt(const QPoint &at);
    void endEdgePick();

    gf::core::Study m_study;
    gf::core::ResultsSeries m_mesh;
    int m_opening = 0;  // counts openStudy calls; a mesh for an older one is dropped
    bool m_meshPending = false;
    QFutureWatcher<MeshDone> m_meshWatch;  // the worker's mesh read, and the groups added to it
    // The mesher runs as a process of its own (core/MeshRun), its mesh
    // written to a folder of the window's own, then read on another thread.
    gf::core::MeshRun m_meshRun;
    QTemporaryDir m_meshFolder;
    gf::core::Setup m_meshSetup;  // the set-up the mesh's groups come from
    int m_meshOpening = 0;        // the opening the running meshing is for
    bool m_meshStopped = false;
    gf::core::JobRunner m_runner;
    QString m_solveStep, m_toolkitName;
    int m_runNumber = 0;
    QString m_runFolder;
    QString m_meshProblem, m_progressText;
    QString m_changedGroup;  // the first group whose faces changed, if any
    int m_percent = 0;
    WindowState::Outcome m_outcome = WindowState::Outcome::None;
    ResultsFile m_results;
    int m_increment = 0;
    vtkActor *m_meshActor = nullptr;     // owned by the renderer while shown
    vtkActor *m_edgesActor = nullptr;    // the mesh's element edges, drawn over it
    vtkActor *m_resultsActor = nullptr;
    vtkActor *m_markersActor = nullptr;  // the supports' and loads' arrows and cones
    vtkActor *m_edgesPickActor = nullptr;  // the geometry's edges, drawn while one is picked
    vtkSmartPointer<vtkPolyData> m_geometryEdges;
    QPointer<LoadForm> m_edgeFor;  // the form an edge is being picked for
    vtkScalarBarActor *m_scalarBar = nullptr;
    vtkSmartPointer<vtkPolyData> m_surface;  // the mesh's outside, each triangle with its geometry face
    vtkSmartPointer<vtkPolyData> m_drawn;    // the same triangles as drawn, three points each
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
    QAction *m_addFreecadGroups = nullptr;
    QAction *m_addMaterial = nullptr;
    QLabel *m_increments = nullptr;
    QAction *m_openParaView = nullptr;
    QAction *m_rescale = nullptr;
    QList<QAction *> m_vcr;  // first, back, forward, last
};

} // namespace gf::gui
