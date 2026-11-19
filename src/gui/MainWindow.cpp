#include "gui/MainWindow.h"
#include "core/TestCurve.h"
#include "gui/CurvePlot.h"
#include "gui/LoadForm.h"
#include "gui/Markers.h"
#include "gui/Style.h"

#include "core/ApplicationInfo.h"
#include "core/Job.h"
#include "core/LoadChoice.h"
#include "core/MaterialCard.h"
#include "core/StudyEdit.h"
#include "mesh/Surface.h"
#include "core/StudyOutline.h"
#include "core/Toolkit.h"
#include "mesh/Groups.h"
#include "mesh/GroupsFile.h"
#include "mesh/Mesher.h"
#include "mesh/Quality.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QSaveFile>
#include <QSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenuBar>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QFont>
#include <QPalette>
#include <QProgressBar>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QScrollArea>
#include <QTableWidget>
#include <QMouseEvent>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <array>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkArrowSource.h>
#include <vtkConeSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkCamera.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLightKit.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkCellPicker.h>
#include <vtkIdList.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkStaticCellLocator.h>
#include <vtkCellData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPolyData.h>
#include <vtkTextRenderer.h>
#include <vtkCommand.h>
#include <vtkTextProperty.h>

#include <QProcess>
#include <QStandardPaths>

namespace gf::gui {

namespace {

QIcon icon(const QString &name) { return QIcon(":/icons/" + name + ".svg"); }

// GridForge's identity (David, 2026-11-17): ParaView's layout, but its own
// colours, so the two are told apart at a glance. Ember orange for the
// accent (the banner, selections, the selected tab's line: gui/Style),
// dark graphite for the view.
// A shade lighter than first (David, 2026-11-18; was 0.105 to 0.305).
const double graphiteBottom[3] = {0.180, 0.188, 0.200};
const double graphiteTop[3] = {0.400, 0.412, 0.431};

// The tree's icon for an outline item: FreeCAD's FEM icons for what FreeCAD
// users know (supports, loads, materials, the mesh), ParaView's for the rest.
QIcon iconFor(const core::OutlineItem &item)
{
    const QString &t = item.title;
    if (item.kind == "mesh")
        return icon("FEM_MeshNetgenFromShape");
    if (item.kind == "material" || item.kind == "materials")
        return icon("FEM_MaterialSolid");
    if (item.kind == "step" || item.kind == "steps")
        return icon("FEM_Analysis");
    if (item.kind == "results")
        return icon("FEM_PostPipelineFromResult");
    if (item.kind == "group" || item.kind == "groups")
        return icon("pqSelectBlock");
    if (item.kind == "jobs" || item.kind == "job")
        return icon("pqVcrPlay");
    if (item.kind == "geometry")
        return icon("pqOpen");
    if (t.startsWith("Fixed"))
        return icon("FEM_ConstraintFixed");
    if (t.startsWith("Force"))
        return icon("FEM_ConstraintForce");
    if (t.startsWith("Pressure"))
        return icon("FEM_ConstraintPressure");
    if (t.startsWith("Gravity"))
        return icon("FEM_ConstraintSelfWeight");
    if (item.kind == "support" || item.kind == "load" || item.kind == "supports")
        return icon("FEM_ConstraintDisplacement");
    return {};
}

void addItems(QTreeWidgetItem *parent, QTreeWidget *tree, const std::vector<core::OutlineItem> &items)
{
    for (const core::OutlineItem &i : items) {
        auto *row = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        row->setText(0, i.title);
        row->setText(1, i.detail);
        row->setToolTip(0, i.detail.isEmpty() ? i.title : i.title + ": " + i.detail);
        row->setToolTip(1, i.detail);
        row->setIcon(0, iconFor(i));
        row->setData(0, Qt::UserRole, i.key);
        row->setData(0, Qt::UserRole + 1, i.kind);
        addItems(row, tree, i.children);
    }
}

QString unitsText(core::Units u)
{
    return u == core::Units::MetreNewtonSecond ? QStringLiteral("m, N, s, Pa") : QStringLiteral("mm, N, s, MPa");
}

QDockWidget *dock(QMainWindow *w, const QString &title, const QString &name, QWidget *content)
{
    auto *d = new QDockWidget(title, w);
    d->setObjectName(name);
    d->setWidget(content);
    return d;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(core::applicationName());  // the name alone, as FreeCAD and ParaView
    QPalette accent = palette();
    accent.setColor(QPalette::Highlight, ember);
    accent.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(accent);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);  // as ParaView

    // The centre: layout tabs over the view, as ParaView's.
    auto *centre = new QWidget(this);
    auto *column = new QVBoxLayout(centre);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    // The one next step, across the top of the view (as TenSense's banner).
    m_banner = new QLabel(centre);
    m_banner->setObjectName(QStringLiteral("nextStep"));
    m_banner->setAutoFillBackground(true);
    QPalette bannerColours = m_banner->palette();
    bannerColours.setColor(QPalette::Window, ember);
    bannerColours.setColor(QPalette::WindowText, Qt::white);
    m_banner->setPalette(bannerColours);
    m_banner->setMargin(7);
    QFont bannerFont = m_banner->font();
    bannerFont.setPointSizeF(bannerFont.pointSizeF() * 1.08);
    bannerFont.setWeight(QFont::DemiBold);
    m_banner->setFont(bannerFont);
    m_banner->setWordWrap(true);
    column->addWidget(m_banner);
    m_layouts = new QTabBar(centre);
    m_layouts->setObjectName(QStringLiteral("layouts"));
    m_layouts->setExpanding(false);  // tabs at the left, as ParaView's
    m_layouts->addTab(tr("Model"));
    m_layouts->addTab(tr("Results"));
    connect(m_layouts, &QTabBar::currentChanged, this, &MainWindow::showLayer);
    m_view = new QVTKOpenGLNativeWidget(centre);
    m_view->setObjectName(QStringLiteral("view"));
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    vtkNew<vtkRenderer> renderer;
    renderer->GradientBackgroundOn();  // graphite, lighter towards the top: not ParaView's blue-grey
    renderer->SetBackground(graphiteBottom[0], graphiteBottom[1], graphiteBottom[2]);
    renderer->SetBackground2(graphiteTop[0], graphiteTop[1], graphiteTop[2]);
    window->AddRenderer(renderer);
    m_renderer = renderer;
    // The legend placed for the view's size as it is drawn, resized or not.
    renderer->AddObserver(vtkCommand::StartEvent, this, &MainWindow::placeLegend);
    vtkNew<vtkLightKit> lights;  // a key, fill and back light, as ParaView's light kit
    lights->AddLightsToRenderer(renderer);
    m_view->setRenderWindow(window);
    m_view->installEventFilter(this);
    m_view->setMouseTracking(true);  // the face under the mouse, before a click
    column->addWidget(m_layouts);
    column->addWidget(m_view, 1);
    setCentralWidget(centre);

    buildDocks();
    buildActions();

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("jobProgress"));
    m_progress->setRange(0, 100);
    m_progress->setMaximumWidth(160);
    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_progress);
    m_units = new QLabel(this);
    m_units->setObjectName(QStringLiteral("units"));
    statusBar()->addPermanentWidget(m_units);
    connect(&m_runner, &core::JobRunner::line, this, &MainWindow::onJobLine);
    connect(&m_runner, &core::JobRunner::finished, this, &MainWindow::onJobFinished);
    statusBar()->showMessage(tr("Open a study (File, Open Study)"));

    connect(&m_meshWatch, &QFutureWatcher<MeshDone>::finished, this, &MainWindow::onMeshed);
    connect(&m_meshRun, &core::MeshRun::finished, this, &MainWindow::onMeshRunEnded);
    resize(1400, 900);
}

MainWindow::~MainWindow()
{
    m_meshWatch.waitForFinished();
    for (vtkObject *o : std::initializer_list<vtkObject *>{m_meshActor, m_edgesActor, m_resultsActor, m_scalarBar})
        if (o)
            o->UnRegister(nullptr);
}

void MainWindow::buildDocks()
{
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("studyTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *now, QTreeWidgetItem *) {
        showProperties(now);
        // A step's support or load shown in its step, lit in ember.
        const QString key = now ? now->data(0, Qt::UserRole).toString() : QString();
        const QStringList parts = key.split(':');
        if ((parts.value(0) == "load" || parts.value(0) == "support") && parts.size() == 3 && m_step
            && parts[1].toInt() < m_step->count() && parts[1].toInt() != m_step->currentIndex())
            m_step->setCurrentIndex(parts[1].toInt());  // redraws
        else
            refreshMarkers();
    });

    m_properties = new QWidget(this);
    m_properties->setObjectName(QStringLiteral("properties"));
    m_propertyForm = new QFormLayout(m_properties);
    m_information = new QLabel(this);
    m_information->setObjectName(QStringLiteral("information"));
    m_information->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_information->setWordWrap(true);
    m_information->setMargin(8);

    m_messages = new QPlainTextEdit(this);
    m_messages->setObjectName(QStringLiteral("messages"));
    m_messages->setReadOnly(true);
    m_job = new QPlainTextEdit(this);
    m_job->setObjectName(QStringLiteral("job"));
    m_job->setReadOnly(true);
    m_deck = new QPlainTextEdit(this);
    m_deck->setObjectName(QStringLiteral("deck"));
    m_deck->setReadOnly(true);
    m_deck->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_deck->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_deck->setPlaceholderText(tr("The solver's input deck, as the toolkit writes it when a run starts."));
    // Loads and supports by step: a row each, a column per step; a click
    // selects what set the value, to edit in Properties.
    m_steps = new QTableWidget(this);
    m_steps->setObjectName(QStringLiteral("stepsTable"));
    m_steps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_steps->setSelectionMode(QAbstractItemView::SingleSelection);
    m_steps->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(m_steps, &QTableWidget::cellClicked, this, [this](int row, int column) {
        const QTableWidgetItem *cell = m_steps->item(row, column);
        const QString key = cell ? cell->data(Qt::UserRole).toString() : QString();
        for (QTreeWidgetItemIterator it(m_tree); *it && !key.isEmpty(); ++it)
            if ((*it)->data(0, Qt::UserRole).toString() == key) {
                m_tree->setCurrentItem(*it);
                break;
            }
    });
    auto *steps = m_steps;

    QDockWidget *study = dock(this, tr("Study"), "studyDock", m_tree);
    // Scrolled when taller than its dock; long text wraps rather than
    // scrolling sideways.
    auto *propertiesScroll = new QScrollArea(this);
    propertiesScroll->setObjectName(QStringLiteral("propertiesScroll"));
    propertiesScroll->setWidget(m_properties);
    propertiesScroll->setWidgetResizable(true);
    propertiesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    propertiesScroll->setFrameShape(QFrame::NoFrame);
    QDockWidget *properties = dock(this, tr("Properties"), "propertiesDock", propertiesScroll);
    QDockWidget *information = dock(this, tr("Information"), "informationDock", m_information);
    addDockWidget(Qt::LeftDockWidgetArea, study);
    addDockWidget(Qt::LeftDockWidgetArea, properties);
    splitDockWidget(study, properties, Qt::Vertical);
    tabifyDockWidget(properties, information);
    properties->raise();

    QDockWidget *messages = dock(this, tr("Messages"), "messagesDock", m_messages);
    QDockWidget *job = dock(this, tr("Job"), "jobDock", m_job);
    QDockWidget *stepsDock = dock(this, tr("Steps"), "stepsDock", steps);
    QDockWidget *deck = dock(this, tr("Deck"), "deckDock", m_deck);
    m_curvePlot = new CurvePlot(this);
    QDockWidget *compare = dock(this, tr("Measured against Predicted"), "compareDock", m_curvePlot);
    addDockWidget(Qt::BottomDockWidgetArea, messages);
    tabifyDockWidget(messages, job);
    tabifyDockWidget(job, stepsDock);
    tabifyDockWidget(stepsDock, deck);
    tabifyDockWidget(deck, compare);
    messages->raise();
    resizeDocks({study}, {470}, Qt::Horizontal);  // room for the values beside the names
    resizeDocks({messages}, {160}, Qt::Vertical);
}

void MainWindow::buildActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *open = file->addAction(icon("pqOpen"), tr("&Open Study..."));
    open->setObjectName(QStringLiteral("openStudy"));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Open a study folder"));
        if (!dir.isEmpty())
            openStudy(dir);
    });
    QAction *create = file->addAction(tr("&New Study from Geometry..."));
    create->setObjectName(QStringLiteral("newStudy"));
    create->setShortcut(QKeySequence::New);
    create->setToolTip(tr("A study for a STEP, or a .brep from FreeCAD with its groups"));
    connect(create, &QAction::triggered, this, [this] {
        const QString geometry = QFileDialog::getOpenFileName(
            this, tr("The geometry to study"), {}, tr("Geometry (*.step *.stp *.brep *.brp)"));
        if (geometry.isEmpty())
            return;
        const QString folder =
            QFileDialog::getExistingDirectory(this, tr("An empty folder for the study"), QFileInfo(geometry).absolutePath());
        if (!folder.isEmpty())
            newStudy(geometry, folder);
    });
    QAction *reload = file->addAction(icon("pqReloadFile"), tr("&Reload"));
    reload->setObjectName(QStringLiteral("reloadStudy"));
    connect(reload, &QAction::triggered, this, [this] {
        if (!m_study.folder.isEmpty())
            openStudy(m_study.folder);
    });
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    for (QDockWidget *d : findChildren<QDockWidget *>())
        view->addAction(d->toggleViewAction());
    QMenu *studyMenu = menuBar()->addMenu(tr("&Study"));
    m_addFreecadGroups = studyMenu->addAction(tr("Add Groups From FreeCAD"));
    m_addFreecadGroups->setObjectName(QStringLiteral("addFreecadGroups"));
    m_addFreecadGroups->setToolTip(tr("The groups named in FreeCAD, from the group file beside the geometry"));
    connect(m_addFreecadGroups, &QAction::triggered, this, &MainWindow::addFreecadGroups);
    m_addMaterial = studyMenu->addAction(tr("Add Material..."));
    m_addMaterial->setObjectName(QStringLiteral("addMaterial"));
    m_addMaterial->setToolTip(tr("A material from a FreeCAD card or its values, for a group of solids"));
    connect(m_addMaterial, &QAction::triggered, this, &MainWindow::showMaterialForm);
    studyMenu->addSeparator();
    m_compareTest = studyMenu->addAction(tr("Compare with a Test..."));
    m_compareTest->setObjectName(QStringLiteral("compareTest"));
    m_compareTest->setToolTip(tr("A test's curve, as TenSense analysed it, beside the run's"));
    connect(m_compareTest, &QAction::triggered, this, [this] { showCompareForm(); });
    menuBar()->addMenu(tr("&Mesh"));
    QMenu *run = menuBar()->addMenu(tr("&Run"));
    menuBar()->addMenu(tr("R&esults"));
    menuBar()->addMenu(tr("&Help"));

    QToolBar *main = addToolBar(tr("Main"));
    main->setObjectName(QStringLiteral("mainToolbar"));
    main->addAction(open);
    main->addAction(reload);

    QToolBar *runBar = addToolBar(tr("Run"));
    runBar->setObjectName(QStringLiteral("runToolbar"));
    m_runAction = run->addAction(icon("pqVcrPlay"), tr("&Run"));
    m_runAction->setObjectName(QStringLiteral("run"));
    m_runAction->setShortcut(Qt::Key_F5);
    m_runAction->setToolTip(tr("Solve the study (F5)"));
    connect(m_runAction, &QAction::triggered, this, &MainWindow::run);
    m_stopAction = run->addAction(icon("pqCancel"), tr("&Stop"));
    m_stopAction->setObjectName(QStringLiteral("stop"));
    m_stopAction->setToolTip(tr("Stop the meshing, or the solve (its files are kept)"));
    connect(m_stopAction, &QAction::triggered, this, [this] {
        if (m_meshRun.running())
            m_meshRun.stop();
        else
            m_runner.stop();
    });
    runBar->addAction(m_runAction);
    runBar->addAction(m_stopAction);

    QToolBar *camera = addToolBar(tr("Camera"));
    camera->setObjectName(QStringLiteral("cameraToolbar"));
    QAction *reset = camera->addAction(icon("pqResetCamera"), tr("Reset Camera"));
    reset->setObjectName(QStringLiteral("resetCamera"));
    connect(reset, &QAction::triggered, this, [this] {
        m_renderer->ResetCamera();
        m_view->renderWindow()->Render();
    });
    struct Axis {
        const char *name, *text;
        double d[3], up[3];
    };
    // As ParaView's: +X looks down +X (from -X), with z or y up.
    const Axis axes[] = {{"pqXPlus", "Set view direction to +X", {1, 0, 0}, {0, 0, 1}},
                         {"pqXMinus", "Set view direction to -X", {-1, 0, 0}, {0, 0, 1}},
                         {"pqYPlus", "Set view direction to +Y", {0, 1, 0}, {0, 0, 1}},
                         {"pqYMinus", "Set view direction to -Y", {0, -1, 0}, {0, 0, 1}},
                         {"pqZPlus", "Set view direction to +Z", {0, 0, 1}, {0, 1, 0}},
                         {"pqZMinus", "Set view direction to -Z", {0, 0, -1}, {0, 1, 0}},
                         {"pqIsometricView", "Apply isometric view", {-1, -1, -1}, {0, 0, 1}}};
    for (const Axis &a : axes) {
        QAction *act = camera->addAction(icon(a.name), tr(a.text));
        act->setObjectName(a.name);
        const Axis copy = a;
        connect(act, &QAction::triggered, this,
                [this, copy] { setCamera(copy.d[0], copy.d[1], copy.d[2], copy.up[0], copy.up[1], copy.up[2]); });
    }

    QToolBar *vcr = addToolBar(tr("Increments"));
    vcr->setObjectName(QStringLiteral("vcrToolbar"));
    const struct {
        const char *icon, *name, *text;
        int step;  // -2 first, -1 back, +1 forward, +2 last
    } buttons[] = {{"pqVcrFirst", "firstIncrement", "First increment", -2},
                   {"pqVcrBack", "previousIncrement", "Previous increment", -1},
                   {"pqVcrForward", "nextIncrement", "Next increment", 1},
                   {"pqVcrLast", "lastIncrement", "Last increment", 2}};
    for (const auto &b : buttons) {
        QAction *a = vcr->addAction(icon(b.icon), tr(b.text));
        a->setObjectName(b.name);
        const int step = b.step;
        connect(a, &QAction::triggered, this, [this, step] {
            const int last = m_results.timeCount() - 1;
            showIncrement(step == -2 ? 0 : step == 2 ? last : std::clamp(m_increment + step, 0, last));
        });
        m_vcr << a;
    }
    m_increments = new QLabel(this);
    m_increments->setObjectName(QStringLiteral("incrementLabel"));
    vcr->addWidget(m_increments);

    addToolBarBreak();
    QToolBar *stepBar = addToolBar(tr("Step"));
    stepBar->setObjectName(QStringLiteral("stepToolbar"));
    stepBar->addWidget(new QLabel(tr(" Step: "), this));
    m_step = new QComboBox(this);
    m_step->setObjectName(QStringLiteral("stepChooser"));
    m_step->setMinimumContentsLength(16);
    m_step->setToolTip(tr("The step whose supports and loads are drawn on the model"));
    stepBar->addWidget(m_step);
    connect(m_step, &QComboBox::currentIndexChanged, this, [this] {
        if (m_renderer)
            refreshMarkers();
    });

    QToolBar *results = addToolBar(tr("Results"));
    results->setObjectName(QStringLiteral("resultsToolbar"));
    results->addWidget(new QLabel(tr(" Colour by: "), this));
    m_colourBy = new QComboBox(this);
    m_colourBy->setObjectName(QStringLiteral("colourBy"));
    m_colourBy->setMinimumContentsLength(22);
    connect(m_colourBy, &QComboBox::currentTextChanged, this, [this] { showIncrement(m_increment); });
    results->addWidget(m_colourBy);
    m_rescale = results->addAction(icon("pqResetRange"), tr("Rescale to data range"));
    m_rescale->setObjectName(QStringLiteral("rescale"));
    connect(m_rescale, &QAction::triggered, this, [this] { showIncrement(m_increment); });
    // The deformed shape, as ParaView's Warp By Vector: true scale first.
    results->addWidget(new QLabel(tr(" Deform \u00d7 "), this));
    m_deform = new QDoubleSpinBox(this);
    m_deform->setObjectName(QStringLiteral("deformFactor"));
    m_deform->setRange(0, 1e7);
    m_deform->setDecimals(2);
    m_deform->setValue(1);
    m_deform->setToolTip(tr("The displacements times this: 1 is true scale, 0 the undeformed shape"));
    m_deform->setEnabled(false);
    connect(m_deform, &QDoubleSpinBox::valueChanged, this, [this] { showIncrement(m_increment); });
    results->addWidget(m_deform);
    m_autoDeform = results->addAction(icon("pqWarp"), tr("Exaggerate the deformation to see it"));
    m_autoDeform->setObjectName(QStringLiteral("autoDeform"));
    m_autoDeform->setToolTip(tr("Exaggerate the deformation so the largest displacement is about a tenth of the "
                                "model, and look across it; again: true scale"));
    // Lit while the shape is not at true scale, however the factor was set.
    m_autoDeform->setCheckable(true);
    connect(m_deform, &QDoubleSpinBox::valueChanged, this, [this](double f) { m_autoDeform->setChecked(f != 1); });
    connect(m_autoDeform, &QAction::triggered, this, [this](bool exaggerate) {
        if (!exaggerate) {
            m_deform->setValue(1);
            return;
        }
        vtkSmartPointer<vtkUnstructuredGrid> g = m_results.model(m_increment, {});
        m_deform->setValue(autoDeform(g ? g->GetLength() : 0, m_results.largestDisplacement(m_increment)));
        // Seen end-on, a movement does not show however large it is drawn
        // (a bend looked at from above it, 2026-11-18).
        vtkCamera *c = m_renderer->GetActiveCamera();
        std::array<double, 3> view, up;
        c->GetDirectionOfProjection(view.data());
        c->GetViewUp(up.data());
        const auto [turned, upright] = viewAcross(view, up, m_results.largestDisplacementVector(m_increment));
        if (turned != view) {
            double focal[3];
            c->GetFocalPoint(focal);
            const double distance = c->GetDistance();
            c->SetPosition(focal[0] - distance * turned[0], focal[1] - distance * turned[1],
                           focal[2] - distance * turned[2]);
            c->SetViewUp(upright.data());
            m_renderer->ResetCamera();
        }
        m_view->renderWindow()->Render();
    });
    m_openParaView = results->addAction(icon("pqOpen"), tr("Open in ParaView"));
    m_openParaView->setObjectName(QStringLiteral("openInParaView"));
    m_openParaView->setToolTip(tr("Hand the results file to ParaView, for everything beyond a quick look"));
    connect(m_openParaView, &QAction::triggered, this, [this] {
        const QStringList cmd =
            paraviewCommand(m_results.path(), !QStandardPaths::findExecutable("paraview").isEmpty(),
                            !QStandardPaths::findExecutable("flatpak").isEmpty()
                                && QProcess::execute("flatpak", {"info", "org.paraview.ParaView"}) == 0);
        if (cmd.isEmpty())
            say(tr("ParaView was not found (neither paraview on PATH nor its Flatpak)"), Tone::Problem);
        else if (QProcess::startDetached(cmd.first(), cmd.mid(1)))
            say(tr("Opened in ParaView: %1").arg(m_results.path()));
        else
            say(tr("ParaView did not start: %1").arg(cmd.join(' ')), Tone::Problem);
    });
    updateActions();  // every action and control now exists
}

void MainWindow::setCamera(double dx, double dy, double dz, double ux, double uy, double uz)
{
    vtkCamera *c = m_renderer->GetActiveCamera();
    c->SetFocalPoint(0, 0, 0);
    c->SetPosition(-dx, -dy, -dz);
    c->SetViewUp(ux, uy, uz);
    m_renderer->ResetCamera();
    m_view->renderWindow()->Render();
}

void MainWindow::say(const QString &message, Tone tone)
{
    // "Problem:" in words as well as red, for whoever cannot tell the colours.
    appendLine(m_messages, (tone == Tone::Problem ? QStringLiteral("Problem: ") : QString()) + message, tone);
    // Shortened to fit beside the units; the whole of it is in Messages.
    const int room = statusBar()->width() - m_units->sizeHint().width()
                     - (m_progress->isVisible() ? m_progress->width() : 0) - 48;
    statusBar()->showMessage(statusBar()->fontMetrics().elidedText(message, Qt::ElideRight, std::max(200, room)));
}

bool MainWindow::openStudy(const QString &path)
{
    QString error;
    core::Study study = core::readStudy(path, &error);
    if (!error.isEmpty()) {
        say(tr("Cannot open the study: %1").arg(error), Tone::Problem);
        return false;
    }
    // The mesh is kept when nothing it is made from changed (an edited load,
    // say): the same folder, geometry, mesh recipe and groups.
    auto meshInputs = [](const core::Study &s) {
        // Not the groups: they are sets on the mesh, made again on it;
        // but a refined group's faces are part of the recipe.
        QJsonObject o{{"folder", s.folder}};
        // The inputs the mesh steps take (the geometry), not every input:
        // a test's curve brought in for a comparison does not touch it.
        for (const core::StudyStep *m : s.stepsOf("mesh")) {
            o[m->id] = core::meshParameters(m->parameters, s.setup, nullptr);
            for (const auto &[name, links] : m->inputs)
                for (const QString &link : links)
                    if (const core::StudyStep *i = s.step(link.section('.', 0, 0)); i && i->kind == "input")
                        o["input:" + i->id] = i->file;
        }
        return o;
    };
    const bool keepMesh = !m_mesh.points.empty() && !m_meshPending && meshInputs(study) == meshInputs(m_study);
    const core::ResultsSeries keptMesh = m_mesh;
    m_study = study;
    ++m_opening;
    m_meshProblem.clear();
    m_meshStopped = false;
    m_outcome = WindowState::Outcome::None;
    m_runNumber = 0;
    m_toolkitName.clear();
    m_mesh = {};
    endEdgePick();
    m_geometryEdges = nullptr;
    m_renderer->RemoveAllViewProps();
    m_tree->clear();
    m_deck->clear();
    if (auto *d = findChild<QDockWidget *>(QStringLiteral("deckDock"))) {
        d->setWindowTitle(tr("Deck"));
        d->setToolTip({});
    }
    addItems(nullptr, m_tree, core::studyOutline(m_study));
    fillSteps();
    m_tree->expandAll();
    m_step->clear();
    for (const core::AnalysisStep &s : m_study.setup.steps)
        m_step->addItem(s.name);
    m_units->setText(unitsText(m_study.setup.units));
    showProperties(nullptr);  // the study, until an item is selected
    setWindowTitle(QStringLiteral("%1 - %2").arg(core::applicationName(), QDir(m_study.folder).dirName()));
    say(tr("Opened %1").arg(m_study.folder));
    m_results = ResultsFile();
    if (m_meshActor && !keepMesh) {
        m_meshActor->UnRegister(nullptr);
        m_meshActor = nullptr;
        m_edgesActor->UnRegister(nullptr);
        m_edgesActor = nullptr;
        m_surface = nullptr;
        m_drawn = nullptr;
        m_surfaceLocator = nullptr;
        m_hovered = 0;
    }
    m_layouts->setCurrentIndex(0);
    const QString earlier = latestResults();
    if (!earlier.isEmpty() && showResults(earlier))
        m_layouts->setCurrentIndex(0);  // the model first; the results are a tab away
    m_picked.clear();
    if (keepMesh) {
        m_mesh = keptMesh;
        // The groups' sets made again, as they are in the study now.
        m_mesh.sets.resize(std::min(m_mesh.sets.size(), m_meshOwnSets));
        QString groupProblem;
        const bool fits = mesh::addGroupSets(m_mesh, m_study.setup, &groupProblem);
        checkGroups();
        offerFreecadGroups();
        if (!fits && m_changedGroup.isEmpty())
            say(tr("The groups do not fit the mesh: %1").arg(groupProblem), Tone::Problem);
        lightFaces({});
        for (QTreeWidgetItem *item : m_tree->findItems(tr("Mesh"), Qt::MatchExactly)) {
            const mesh::Quality q = mesh::measure(m_mesh.points.front().grid);
            const QString detail = tr("%1 tetrahedra, %2 nodes; %3")
                                       .arg(m_mesh.points.front().grid->GetNumberOfCells())
                                       .arg(m_mesh.points.front().grid->GetNumberOfPoints())
                                       .arg(q.solverReady() ? tr("ready for a solver") : tr("NOT ready for a solver"));
            item->setText(1, detail);
            item->setToolTip(1, detail);
        }
        refreshMarkers();
        showLayer(m_layouts->currentIndex());
    } else {
        startMeshing();
    }
    updateActions();
    return true;
}

void MainWindow::showProperties(QTreeWidgetItem *item)
{
    endEdgePick();  // the form it was for is going
    while (m_propertyForm->count() > 0) {
        QLayoutItem *row = m_propertyForm->takeAt(0);
        delete row->widget();
        delete row;
    }
    auto value = [this](const QString &text) {
        auto *l = new QLabel(text, m_properties);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setWordWrap(true);
        return l;
    };
    if (item) {
        // A tree item replaces a pick; a group's faces are lit in the view.
        m_picked.clear();
        const QString key = item->data(0, Qt::UserRole).toString();
        const core::Group *g = key.startsWith("group:") ? m_study.setup.group(key.mid(6)) : nullptr;
        lightFaces(g ? g->faces : std::vector<int>{});
    } else if (!m_picked.empty()) {
        showPicked();
        return;
    } else {
        lightFaces({});
    }
    if (!item) {
        // Nothing selected: the study itself, and how to see more.
        if (m_study.folder.isEmpty()) {
            m_propertyForm->addRow(value(tr("Open a study to see it here.")));
            return;
        }
        m_propertyForm->addRow(tr("Study"), value(QDir(m_study.folder).dirName()));
        // A long path shortened in the middle to fit, whole on hover.
        QLabel *folder = value(m_study.folder);
        folder->setWordWrap(false);
        folder->setText(folder->fontMetrics().elidedText(m_study.folder, Qt::ElideMiddle,
                                                          std::max(120, m_properties->width() - 110)));
        folder->setToolTip(m_study.folder);
        m_propertyForm->addRow(tr("Folder"), folder);
        m_propertyForm->addRow(tr("Units"), value(m_units->text()));
        m_propertyForm->addRow(tr("Steps"), value(QString::number(m_study.setup.steps.size())));
        const QString latest = latestResults();
        m_propertyForm->addRow(tr("Runs"), value(latest.isEmpty() ? tr("none yet")
                                                                  : tr("latest: run %1").arg(latest.section('/', -3, -3))));
        m_propertyForm->addRow(value(tr("Select an item in the Study tree to see it here. Click a face of the "
                                        "mesh to pick it, Ctrl+click to pick more, and make them a group.")));
        return;
    }
    m_propertyForm->addRow(tr("Name"), value(item->text(0)));
    if (!item->text(1).isEmpty())
        m_propertyForm->addRow(tr("Value"), value(item->text(1)));
    m_propertyForm->addRow(tr("Kind"), value(item->data(0, Qt::UserRole + 1).toString()));

    // What can be edited: a field each, Apply and Reset as ParaView's.
    m_editKey = item->data(0, Qt::UserRole).toString();
    if (m_editKey.startsWith("group:")) {
        showGroupEditing(m_editKey.mid(6));
        return;
    }
    if (m_editKey.startsWith("compare:")) {
        showCompareForm(m_editKey.mid(8));
        return;
    }
    const QJsonObject entry = core::itemEntry(m_study.setupJson, m_editKey);
    if (!entry.isEmpty() && !entry.contains("gravity")) {
        showLoadForm(m_editKey, entry);
        return;
    }
    const std::vector<core::EditableField> fields = core::editableFields(m_study, m_editKey);
    if (fields.empty())
        return;
    auto *buttons = new QWidget(m_properties);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 6, 0, 6);
    auto *apply = new QPushButton(icon("pqApply"), tr("Apply"), buttons);
    apply->setObjectName(QStringLiteral("apply"));
    apply->setEnabled(false);
    auto *reset = new QPushButton(icon("pqCancel"), tr("Reset"), buttons);
    reset->setObjectName(QStringLiteral("reset"));
    reset->setEnabled(false);
    row->addWidget(apply);
    row->addWidget(reset);
    row->addStretch();
    m_propertyForm->addRow(buttons);
    auto changed = [apply, reset] {
        // As ParaView's Apply: lit up when there is something to apply.
        QPalette lit = apply->palette();
        lit.setColor(QPalette::Button, ember);
        lit.setColor(QPalette::ButtonText, Qt::white);
        apply->setPalette(lit);
        apply->setEnabled(true);
        reset->setEnabled(true);
    };
    for (const core::EditableField &f : fields) {
        auto *edit = new QLineEdit(m_properties);
        edit->setObjectName("edit:" + f.name);
        edit->setText(f.value ? QString::number(*f.value, 'g', 12) : QString());
        edit->setPlaceholderText(f.hint);
        edit->setProperty("given", edit->text());
        connect(edit, &QLineEdit::textEdited, this, changed);
        connect(edit, &QLineEdit::returnPressed, this, &MainWindow::applyEdits);
        m_propertyForm->addRow(f.unit.isEmpty() ? f.label : QString("%1 (%2)").arg(f.label, f.unit), edit);
    }
    connect(apply, &QPushButton::clicked, this, &MainWindow::applyEdits);
    connect(reset, &QPushButton::clicked, this, [this] { showProperties(m_tree->currentItem()); });
}

void MainWindow::startMeshing()
{
    const std::vector<const core::StudyStep *> meshes = m_study.stepsOf("mesh");
    if (meshes.empty() || meshes.front()->by != "gridforge")
        return;
    const core::StudyStep &step = *meshes.front();
    const QString link = step.inputs.count("geometry") ? step.inputs.at("geometry").value(0) : QString();
    const core::StudyStep *input = m_study.step(link.section('.', 0, 0));
    if (!input) {
        say(tr("The mesh step has no geometry"), Tone::Problem);
        return;
    }
    const QString file = QDir(m_study.folder).filePath(input->file);
    m_meshSetup = m_study.setup;
    m_meshOpening = m_opening;
    say(tr("Meshing %1...").arg(input->file));
    m_meshPending = true;
    // In a process of its own (core/MeshRun): the window keeps answering
    // (tenet 7), a crash in Netgen or OpenCASCADE ends that process and
    // not the window, and Stop ends it at once. A meshing for a study
    // opened before is replaced, and its end not said. GF_MESH_WORKER
    // names another worker, as GF_CCX another ccx.
    const QString worker = qEnvironmentVariableIsSet("GF_MESH_WORKER")
                               ? qEnvironmentVariable("GF_MESH_WORKER")
                               : core::meshWorkerProgram(QCoreApplication::applicationDirPath(), GF_BUILT_STUDY);
    // The recipe as the mesher takes it: "refine"'s groups as their faces.
    QString unresolved;
    const QJsonObject parameters = core::meshParameters(step.parameters, m_study.setup, &unresolved);
    if (!unresolved.isEmpty()) {
        MeshDone done;
        done.opening = m_meshOpening;
        done.error = unresolved;
        meshArrived(done);
        return;
    }
    m_meshRun.start(worker, file, parameters, m_meshFolder.filePath("mesh.vtu"));
    updateActions();
}

void MainWindow::onMeshRunEnded(const core::MeshEnd &end)
{
    if (m_meshOpening != m_opening)
        return;  // for a study since replaced
    using Kind = core::MeshEnd::Kind;
    if (end.kind == Kind::Stopped) {
        m_meshPending = false;
        m_meshStopped = true;
        say(tr("Meshing stopped"), Tone::Warning);
        updateActions();
        return;
    }
    if (end.kind != Kind::Made) {
        MeshDone done;
        done.opening = m_meshOpening;
        done.error = end.message;
        meshArrived(done);
        return;
    }
    // Read on another thread, and the groups' sets added there: a large
    // mesh takes a moment, and the window never waits (tenet 7).
    const QString file = m_meshFolder.filePath("mesh.vtu");
    const core::Setup setup = m_meshSetup;
    const int opening = m_meshOpening;
    m_meshWatch.setFuture(QtConcurrent::run([file, setup, opening] {
        MeshDone done;
        done.opening = opening;
        done.mesh = core::readMeshFile(file, &done.error);
        done.ownSets = done.mesh.sets.size();
        if (done.error.isEmpty())
            mesh::addGroupSets(done.mesh, setup, &done.groupProblem);
        return done;
    }));
}

void MainWindow::onMeshed()
{
    meshArrived(m_meshWatch.result());
}

void MainWindow::meshArrived(const MeshDone &done)
{
    if (done.opening != m_opening)
        return;  // for a study since replaced
    m_meshPending = false;
    if (!done.error.isEmpty() || done.mesh.points.empty()) {
        m_meshProblem = done.error;
        say(tr("The mesh could not be made: %1").arg(done.error), Tone::Problem);
        updateActions();
        return;
    }
    m_mesh = done.mesh;
    m_meshOwnSets = done.ownSets;
    vtkUnstructuredGrid *grid = m_mesh.points.front().grid;
    const mesh::Quality quality = mesh::measure(grid);

    // The surface with its edges, as ParaView's "Surface With Edges": the
    // outer faces of the elements, each knowing its geometry face, so a
    // face can be picked and a group's faces lit (in ember).
    m_surface = mesh::outerSurface(m_mesh);
    m_surfaceLocator = nullptr;
    // Drawn with three points of its own per triangle, coloured by point:
    // colours given per triangle (cell scalars) were not drawn at all on a
    // software GL (Mesa's llvmpipe, 2026-11-17) once the faces' own edges
    // were off. Triangle k drawn is triangle k of the surface, so a pick
    // still names its face.
    m_drawn = vtkSmartPointer<vtkPolyData>::New();
    {
        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        vtkNew<vtkCellArray> triangles;
        vtkIdType n = 0;
        const vtkIdType *ids;
        vtkCellArray *polys = m_surface->GetPolys();
        polys->InitTraversal();
        while (polys->GetNextCell(n, ids)) {
            const vtkIdType first = points->GetNumberOfPoints();
            for (vtkIdType k = 0; k < n; ++k)
                points->InsertNextPoint(m_surface->GetPoint(ids[k]));
            triangles->InsertNextCell({first, first + 1, first + 2});
        }
        m_drawn->SetPoints(points);
        m_drawn->SetPolys(triangles);
    }
    m_hovered = 0;
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_drawn);
    mapper->SetScalarModeToUsePointData();
    mapper->SetColorModeToDirectScalars();
    mapper->ScalarVisibilityOn();
    // The faces a little behind the edges drawn on them, so the edges show.
    mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(1, 1);
    lightFaces(m_picked);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    // Light faces and soft edges, so the part reads on graphite.
    actor->GetProperty()->SetColor(0.86, 0.87, 0.89);
    actor->GetProperty()->SetAmbient(0.15);
    actor->GetProperty()->SetDiffuse(0.85);
    m_meshActor = actor;
    actor->Register(nullptr);  // kept while the Results layer is shown instead
    // The elements' own edges (through their middle nodes, for a quadratic
    // mesh), not the four triangles a curved face is drawn with.
    vtkNew<vtkPolyDataMapper> edgesMapper;
    edgesMapper->SetInputData(mesh::outerEdges(m_mesh));
    edgesMapper->ScalarVisibilityOff();
    vtkNew<vtkActor> edges;
    edges->SetMapper(edgesMapper);
    edges->GetProperty()->SetColor(0.52, 0.55, 0.60);
    edges->GetProperty()->LightingOff();
    edges->PickableOff();
    m_edgesActor = edges;
    edges->Register(nullptr);
    refreshMarkers();
    showLayer(m_layouts->currentIndex());
    setCamera(-1, -1, -1, 0, 0, 1);
    checkGroups();
    offerFreecadGroups();
    if (!done.groupProblem.isEmpty() && m_changedGroup.isEmpty())
        say(tr("The groups do not fit the mesh: %1").arg(done.groupProblem), Tone::Problem);

    const QString detail = tr("%1 tetrahedra, %2 nodes; %3")
                               .arg(grid->GetNumberOfCells())
                               .arg(grid->GetNumberOfPoints())
                               .arg(quality.solverReady() ? tr("ready for a solver") : tr("NOT ready for a solver"));
    for (QTreeWidgetItem *item : m_tree->findItems(tr("Mesh"), Qt::MatchExactly)) {
        item->setText(1, detail);
        item->setToolTip(1, detail);
    }
    m_information->setText(mesh::summary(quality).join('\n'));
    say(tr("Meshed: %1").arg(detail), quality.solverReady() ? Tone::Done : Tone::Warning);
    updateActions();
    emit meshReady();
}

QString MainWindow::selectedKey() const
{
    return m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
}

void MainWindow::applyEdits()
{
    core::Edit edit;
    for (QLineEdit *e : m_properties->findChildren<QLineEdit *>()) {
        if (!e->objectName().startsWith("edit:") || e->text() == e->property("given").toString())
            continue;
        const QString name = e->objectName().mid(5);
        if (e->text().trimmed().isEmpty()) {
            edit[name] = std::nullopt;
            continue;
        }
        bool ok = false;
        const double v = e->text().trimmed().toDouble(&ok);
        if (!ok) {
            say(tr("Cannot apply: \"%1\" is not a number").arg(e->text()), Tone::Problem);
            return;
        }
        edit[name] = v;
    }
    if (edit.empty())
        return;
    QString error;
    const QString key = m_editKey;
    if (!core::editStudy(m_study.folder, key, edit, &error)) {
        say(tr("Cannot apply: %1").arg(error), Tone::Problem);
        return;
    }
    say(tr("Applied to study.json: %1").arg(key));
    openStudy(m_study.folder);
    // The same item selected again.
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == key) {
            m_tree->setCurrentItem(*it);
            break;
        }
}

void MainWindow::updateBanner()
{
    WindowState s;
    s.studyOpen = !m_study.folder.isEmpty();
    for (const core::StudyStep *i : m_study.stepsOf("input"))
        s.geometry = QFileInfo(i->file).fileName();
    s.meshing = m_meshPending;
    s.meshProblem = m_meshProblem;
    s.meshStopped = m_meshStopped;
    s.changedGroup = m_changedGroup;
    if (!m_mesh.points.empty())
        s.freecadGroups = freecadGroupsMissing();
    s.hasMaterial = !m_study.setup.assign.empty();
    s.hasSupport = !m_study.setup.supports.empty();
    s.hasLoad = false;
    for (const core::AnalysisStep &st : m_study.setup.steps) {
        s.hasSupport = s.hasSupport || !st.supports.empty();
        s.hasLoad = s.hasLoad || !st.loads.empty();
    }
    s.meshed = !m_mesh.points.empty();
    const std::vector<const core::StudyStep *> solves = m_study.stepsOf("solve");
    s.canSolve = !solves.empty();
    if (!m_toolkitName.isEmpty())
        s.toolkit = m_toolkitName;
    else if (!solves.empty())
        s.toolkit = solves.front()->by == "calculix" ? QStringLiteral("CalculiX") : solves.front()->by;
    s.running = m_runner.running();
    s.run = m_runNumber;
    s.progress = m_progressText;
    s.percent = m_percent;
    s.outcome = m_outcome;
    m_banner->setText(nextStep(s));
}

void MainWindow::updateActions()
{
    const bool meshed = !m_mesh.points.empty();
    m_addFreecadGroups->setEnabled(meshed && !freecadGroupsFile().isEmpty());
    m_addMaterial->setEnabled(!m_study.folder.isEmpty());
    m_runAction->setEnabled(meshed && m_changedGroup.isEmpty() && !m_runner.running()
                            && !m_study.stepsOf("solve").empty());
    m_stopAction->setEnabled(m_runner.running() || m_meshRun.running());
    const bool results = m_results.timeCount() > 0;
    m_compareTest->setEnabled(results);
    for (QAction *a : m_vcr)
        a->setEnabled(results);
    m_colourBy->setEnabled(results);
    m_rescale->setEnabled(results);
    m_deform->setEnabled(results && m_results.canDeform());
    m_autoDeform->setEnabled(results && m_results.canDeform());
    m_openParaView->setEnabled(results);
    updateBanner();
}

void MainWindow::setJobState(const QString &state)
{
    QTreeWidgetItem *item = nullptr;
    for (QTreeWidgetItem *i : m_tree->findItems(m_solveStep, Qt::MatchExactly | Qt::MatchRecursive))
        if (i->data(0, Qt::UserRole + 1).toString() == "job")
            item = i;
    if (item) {
        item->setText(1, QString("%1, %2").arg(m_toolkitName, state));
        item->setToolTip(1, item->text(1));
    }
}

void MainWindow::run()
{
    const std::vector<const core::StudyStep *> solves = m_study.stepsOf("solve");
    if (solves.empty() || m_mesh.points.empty() || m_runner.running())
        return;
    const core::StudyStep &step = *solves.front();

    // The toolkit, found by its manifest; the shipped adapters sit beside
    // the window's own program.
    const QString here = QCoreApplication::applicationDirPath();
    if (!qEnvironmentVariable("PATH").startsWith(here + ":"))
        qputenv("PATH", (here + ":" + qEnvironmentVariable("PATH")).toLocal8Bit());
    const core::ToolkitSet toolkits = core::findToolkits(core::shippedToolkitFolders(QCoreApplication::applicationDirPath(), GF_SHIPPED_TOOLKITS));
    QString why;
    const core::Toolkit *toolkit = core::readyToolkit(toolkits, "solve", step.parameters["kind"].toString("structural.static"), &why);
    if (!toolkit) {
        say(tr("Cannot run: %1").arg(why), Tone::Problem);
        return;
    }

    // The set-up filled from its cards; the job folder for this run.
    core::Setup setup = m_study.setup;
    if (!core::fillFromCards(setup, m_study.folder, &why)) {
        say(tr("Cannot run: %1").arg(why), Tone::Problem);
        return;
    }
    const QString folder = core::nextRunFolder(m_study, step.id);
    if (!core::writeSolveJob(folder, step.parameters["kind"].toString("structural.static"), setup,
                             m_study.setupJson, m_study.folder, m_mesh, &why)) {
        say(tr("Cannot run: %1").arg(why), Tone::Problem);
        return;
    }
    m_solveStep = step.id;
    m_toolkitName = toolkit->name;
    m_runNumber = QDir(folder).dirName().toInt();
    m_runFolder = folder;
    m_job->clear();
    appendLine(m_job, tr("Run %1 with %2, in %3").arg(m_runNumber).arg(toolkit->name, folder), Tone::Detail);
    if (auto *d = findChild<QDockWidget *>("jobDock")) {
        d->raise();
        resizeDocks({d}, {160}, Qt::Vertical);  // the view keeps its room
    }
    if (!m_runner.start(*toolkit, folder, &why)) {
        say(tr("Cannot run: %1").arg(why), Tone::Problem);
        return;
    }
    setJobState(tr("running (run %1)").arg(m_runNumber));
    m_outcome = WindowState::Outcome::None;
    m_progressText.clear();
    m_percent = 0;
    m_progress->setValue(0);
    m_progress->setVisible(true);
    say(tr("Solving with %1 (run %2)...").arg(toolkit->name).arg(m_runNumber));
    updateActions();
}

void MainWindow::onJobLine(const core::ProtocolLine &l)
{
    using Type = core::ProtocolLine::Type;
    switch (l.type) {
    case Type::Progress:
        appendLine(m_job, QString("%1%  %2").arg(100 * l.fraction, 3, 'f', 0).arg(l.text), toneOf(l));
        m_progress->setValue(qRound(100 * l.fraction));
        m_progressText = l.text;
        m_percent = qRound(100 * l.fraction);
        updateBanner();
        statusBar()->showMessage(tr("%1: %2").arg(m_toolkitName, l.text));
        break;
    case Type::Metric:
        appendLine(m_job, QString("        %1: %2 (%3)").arg(l.name).arg(l.value).arg(l.text), toneOf(l));
        break;
    case Type::Log:
        appendLine(m_job, QString("  %1: %2").arg(l.level, l.text), toneOf(l));
        if (l.level == "warning" || l.level == "error")
            say(QString("%1 (%2): %3").arg(m_toolkitName, l.level, l.text), toneOf(l));
        break;
    case Type::File:
        if (l.role == "deck")
            showDeck(QDir(m_runFolder).filePath(l.path), m_runNumber);
        break;
    case Type::Other:
        appendLine(m_job, "  " + l.text, toneOf(l));
        break;
    }
}

void MainWindow::onJobFinished(int exitCode, const QString &results, bool)
{
    m_progress->setVisible(false);
    m_outcome = exitCode == 3 ? WindowState::Outcome::Cancelled
                : (exitCode != 0 || results.isEmpty()) ? WindowState::Outcome::Failed
                                                       : WindowState::Outcome::Solved;
    if (exitCode == 3) {
        setJobState(tr("cancelled (run %1)").arg(m_runNumber));
        say(tr("Run %1 cancelled; its files are kept").arg(m_runNumber), Tone::Warning);
    } else if (exitCode != 0 || results.isEmpty()) {
        setJobState(tr("failed (run %1)").arg(m_runNumber));
        say(tr("Run %1 failed (exit code %2): see the Job dock").arg(m_runNumber).arg(exitCode), Tone::Problem);
    } else {
        setJobState(tr("solved (run %1)").arg(m_runNumber));
        say(tr("Run %1 solved: %2").arg(m_runNumber).arg(results), Tone::Done);
        showResults(results);
    }
    // Properties says the run that just finished, unless it holds edits not
    // yet applied: those are the user's, and never wiped.
    const auto *apply = m_properties->findChild<QPushButton *>(QStringLiteral("apply"));
    if (!apply || !apply->isEnabled())
        showProperties(m_tree->currentItem());
    updateActions();
}

QStringList MainWindow::paraviewCommand(const QString &file, bool paraviewOnPath, bool flatpak)
{
    if (paraviewOnPath)
        return {"paraview", file};
    if (flatpak)
        return {"flatpak", "run", "--filesystem=" + QFileInfo(file).absolutePath(), "org.paraview.ParaView", file};
    return {};
}

namespace {

QString facesText(const std::vector<int> &faces)
{
    QStringList n;
    for (int f : faces)
        n << QString::number(f);
    return (faces.size() == 1 ? QStringLiteral("face ") : QStringLiteral("faces ")) + n.join(", ");
}

} // namespace

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view && event->type() == QEvent::MouseMove) {
        auto *e = static_cast<QMouseEvent *>(event);
        // While a button is held the view turns: no tint then.
        hover(e->buttons() == Qt::NoButton && m_layouts->currentIndex() == 0 ? faceAt(e->position().toPoint()) : 0);
    } else if (watched == m_view && event->type() == QEvent::Leave) {
        hover(0);
    }
    if (watched == m_view && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
        auto *e = static_cast<QMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) {
            if (event->type() == QEvent::MouseButtonPress) {
                m_pressedAt = e->position().toPoint();
            } else if (m_pressedAt.x() >= 0) {
                // Let go where it was pressed: a click, not a turn of the view.
                if ((e->position().toPoint() - m_pressedAt).manhattanLength() < 4 && m_layouts->currentIndex() == 0)
                    pickAt(e->position().toPoint(), e->modifiers() & Qt::ControlModifier);
                m_pressedAt = {-1, -1};
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);  // the view still gets it
}

int MainWindow::faceAt(const QPoint &at)
{
    if (!m_surface || !m_meshActor)
        return 0;
    if (!m_surfaceLocator) {
        // Built once per mesh: a ray finds its triangle without trying all.
        m_surfaceLocator = vtkSmartPointer<vtkStaticCellLocator>::New();
        m_surfaceLocator->SetDataSet(m_drawn);  // what the picker sees
        m_surfaceLocator->BuildLocator();
    }
    vtkNew<vtkCellPicker> picker;
    picker->SetTolerance(0.0005);
    picker->PickFromListOn();
    picker->AddPickList(m_meshActor);
    picker->AddLocator(m_surfaceLocator);
    const double k = m_view->devicePixelRatioF();
    if (!picker->Pick(at.x() * k, (m_view->height() - at.y()) * k, 0, m_renderer) || picker->GetCellId() < 0)
        return 0;
    vtkDataArray *faces = m_surface->GetCellData()->GetArray("face");
    return faces && picker->GetCellId() < faces->GetNumberOfTuples() ? int(faces->GetComponent(picker->GetCellId(), 0))
                                                                     : 0;
}

void MainWindow::hover(int face)
{
    if (face == m_hovered)
        return;
    m_hovered = face;
    if (face > 0)
        statusBar()->showMessage(tr("Face %1: click to pick it, Ctrl+click to add it").arg(face));
    else if (statusBar()->currentMessage().startsWith(tr("Face ")))
        statusBar()->clearMessage();  // the name goes with the tint
    lightFaces(m_lit);
}

void MainWindow::pickAt(const QPoint &at, bool add)
{
    if (m_edgesPickActor && m_edgeFor && !m_edgeFor->wantsEdge())
        endEdgePick();  // another direction chosen since
    if (m_edgesPickActor) {
        // Waiting for an edge: the click is for that, not for a face.
        pickEdgeAt(at);
        return;
    }
    if (!m_surface || !m_meshActor)
        return;
    const int face = faceAt(at);
    if (!add) {
        m_picked = face > 0 ? std::vector<int>{face} : std::vector<int>{};
    } else if (face > 0) {
        // Ctrl+click: a face added, or taken out again.
        const auto at = std::find(m_picked.begin(), m_picked.end(), face);
        if (at == m_picked.end())
            m_picked.push_back(face);
        else
            m_picked.erase(at);
    }
    // The pick, not a tree item, is what Properties shows now.
    const QSignalBlocker quiet(m_tree);
    m_tree->setCurrentItem(nullptr);
    m_tree->clearSelection();
    lightFaces(m_picked);
    showProperties(nullptr);
}

void MainWindow::lightFaces(const std::vector<int> &faces)
{
    m_lit = faces;
    if (!m_surface || !m_drawn)
        return;
    vtkDataArray *face = m_surface->GetCellData()->GetArray("face");
    vtkNew<vtkUnsignedCharArray> colours;
    colours->SetName("colours");
    colours->SetNumberOfComponents(3);
    colours->SetNumberOfTuples(m_drawn->GetNumberOfPoints());
    for (vtkIdType c = 0; c < m_surface->GetNumberOfCells(); ++c) {
        const int f = face ? int(face->GetComponent(c, 0)) : 0;
        const bool lit = std::find(faces.begin(), faces.end(), f) != faces.end();
        // Light grey so the part reads on graphite; ember where lit; pale
        // ember under the mouse (FreeCAD's preselection, in our colour).
        const QColor c3 = lit ? ember : f > 0 && f == m_hovered ? QColor(250, 160, 110) : QColor(219, 222, 227);
        const std::array<unsigned char, 3> rgb{uchar(c3.red()), uchar(c3.green()), uchar(c3.blue())};
        for (int k = 0; k < 3; ++k)  // the triangle's own three points
            colours->SetTypedTuple(3 * c + k, rgb.data());
    }
    m_drawn->GetPointData()->SetScalars(colours);
    m_drawn->Modified();
    m_view->renderWindow()->Render();
}

void MainWindow::showPicked()
{
    auto *picked = new QLabel(facesText(m_picked), m_properties);
    picked->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_propertyForm->addRow(tr("Picked"), picked);
    auto *name = new QLineEdit(m_properties);
    name->setObjectName(QStringLiteral("groupName"));
    // The group these faces are already, or a name for a new one: used
    // when the field is left empty.
    name->setPlaceholderText(groupFor(m_picked));
    name->setToolTip(tr("Left empty: %1").arg(name->placeholderText()));
    m_propertyForm->addRow(tr("Group"), name);
    auto *buttons = new QWidget(m_properties);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 6, 0, 6);
    auto *make = new QPushButton(icon("pqApply"), tr("Make group"), buttons);
    make->setObjectName(QStringLiteral("makeGroup"));
    // Plain: the form below is where the faces usually go, and its Apply
    // is the one lit.
    row->addWidget(make);
    row->addStretch();
    m_propertyForm->addRow(buttons);  // as Apply: its own width, on the left
    auto *hint = new QLabel(tr("Ctrl+click adds a face or takes it out; a click where there is no model lets "
                               "the pick go."),
                            m_properties);
    hint->setWordWrap(true);
    m_propertyForm->addRow(hint);
    connect(make, &QPushButton::clicked, this, &MainWindow::makeGroup);
    connect(name, &QLineEdit::returnPressed, this, &MainWindow::makeGroup);
    showLoadForm({}, {});
}

QString MainWindow::groupFor(const std::vector<int> &faces) const
{
    std::vector<int> sorted = faces;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    for (const core::Group &g : m_study.setup.groups) {
        std::vector<int> own = g.faces;
        std::sort(own.begin(), own.end());
        if (own == sorted && g.solids.empty())
            return g.name;
    }
    QString name = facesText(sorted);
    name[0] = name[0].toUpper();
    for (int n = 2; m_study.setup.group(name); ++n)
        name = QString("%1 (%2)").arg(facesText(sorted)).arg(n).replace(0, 1, facesText(sorted)[0].toUpper());
    return name;
}

void MainWindow::showLoadForm(const QString &key, const QJsonObject &entry)
{
    // The faces it acts on: the picked ones, or its group's.
    std::vector<int> faces = m_picked;
    if (!key.isEmpty()) {
        const core::Group *g = m_study.setup.group(entry["group"].toString());
        faces = g ? g->faces : std::vector<int>{};
        lightFaces(faces);
    }
    std::sort(faces.begin(), faces.end());
    LoadForm::Context context;
    const bool metres = m_study.setup.units == core::Units::MetreNewtonSecond;
    context.length = metres ? "m" : "mm";
    context.stress = metres ? "Pa" : "MPa";
    for (const core::AnalysisStep &st : m_study.setup.steps)
        context.steps << st.name;
    context.firstFace = faces.empty() ? 0 : faces.front();
    vtkSmartPointer<vtkPolyData> surface = m_surface;
    context.normal = [surface, faces](QString *problem) -> std::optional<core::Vec3> {
        if (!surface || faces.empty()) {
            if (problem)
                *problem = faces.empty() ? QString("no faces") : QString("the mesh is not made yet");
            return std::nullopt;
        }
        const auto n = mesh::outwardNormal(surface, faces, problem);
        if (!n)
            return std::nullopt;
        return core::Vec3{(*n)[0], (*n)[1], (*n)[2]};
    };
    context.edge = [this](int edge, QString *problem) -> std::optional<core::Vec3> {
        const auto d = mesh::edgeDirection(m_mesh, edge, problem);
        if (!d)
            return std::nullopt;
        return core::Vec3{(*d)[0], (*d)[1], (*d)[2]};
    };
    context.editing = !key.isEmpty();
    // Where it is, or where a new one goes: the step chosen in the Steps
    // dock, else the first.
    QString place = QString("step:%1").arg(std::max(0, m_steps ? m_steps->currentColumn() : 0));
    if (key.startsWith("support:"))
        place = key.count(':') == 1 ? QString("supports") : "step:" + key.section(':', 1, 1);
    else if (key.startsWith("load:"))
        place = "step:" + key.section(':', 1, 1);
    const core::LoadChoice start = core::choiceOf(entry, key.startsWith("support:"));
    auto *form = new LoadForm(context, start, place, m_properties);
    m_propertyForm->addRow(form);
    connect(form, &LoadForm::applyClicked, this, [this, form, key] { applyLoad(form, key); });
    connect(form, &LoadForm::edgeWanted, this, [this, form] { startEdgePick(form); });
    connect(form, &LoadForm::removeClicked, this, [this, key] {
        const QString title = m_tree->currentItem() ? m_tree->currentItem()->text(0) : key;
        QString error;
        if (!core::removeItem(m_study.folder, key, &error)) {
            say(tr("Cannot delete: %1").arg(error), Tone::Problem);
            return;
        }
        say(tr("Deleted from study.json: %1").arg(title), Tone::Done);
        openStudy(m_study.folder);
    });
}

void MainWindow::applyLoad(LoadForm *form, const QString &key)
{
    const QString problem = form->problem();
    if (!problem.isEmpty()) {
        say(tr("Cannot apply: %1").arg(problem), Tone::Problem);
        return;
    }
    core::LoadChoice choice = form->choice();
    QString error, made;
    if (key.isEmpty()) {
        // New, on the picked faces: their group, made first if need be.
        auto *name = m_properties->findChild<QLineEdit *>(QStringLiteral("groupName"));
        choice.group = name && !name->text().trimmed().isEmpty() ? name->text().trimmed() : groupFor(m_picked);
    }
    const QJsonObject entry = core::entryOf(choice, &error);
    if (entry.isEmpty()) {
        say(tr("Cannot apply: %1").arg(error), Tone::Problem);
        return;
    }
    if (key.isEmpty() && !m_study.setup.group(choice.group)) {
        if (!core::addFaceGroup(m_study.folder, choice.group, m_picked, &error, signaturesOf(m_picked))) {
            say(tr("Cannot make the group: %1").arg(error), Tone::Problem);
            return;
        }
        made = choice.group;
    } else if (key.isEmpty() && groupFor(m_picked) != choice.group) {
        say(tr("Cannot apply: there is a group named %1 already, of other faces").arg(choice.group), Tone::Problem);
        return;
    }
    const QString now = key.isEmpty() ? core::addItem(m_study.folder, form->place(), entry, &error)
                                      : core::replaceItem(m_study.folder, key, entry, &error);
    if (now.isEmpty()) {
        if (!made.isEmpty())
            core::deleteGroup(m_study.folder, made, nullptr);  // as it was
        say(tr("Cannot apply: %1").arg(error), Tone::Problem);
        return;
    }
    openStudy(m_study.folder);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == now) {
            m_tree->setCurrentItem(*it);
            say(tr("%1 in study.json: %2, %3")
                    .arg(key.isEmpty() ? tr("Added") : tr("Applied"), (*it)->text(0), (*it)->text(1)),
                Tone::Done);
            break;
        }
}

void MainWindow::fillSteps()
{
    const core::StepTable t = core::stepTable(m_study);
    m_steps->clear();
    m_steps->setColumnCount(int(t.steps.size()));
    m_steps->setRowCount(int(t.rows.size()));
    m_steps->setHorizontalHeaderLabels(t.steps);
    for (int r = 0; r < int(t.rows.size()); ++r) {
        m_steps->setVerticalHeaderItem(r, new QTableWidgetItem(t.rows[r].title));
        for (int c = 0; c < int(t.rows[r].cells.size()); ++c) {
            const core::StepCell &cell = t.rows[r].cells[c];
            if (cell.text.isEmpty())
                continue;
            auto *item = new QTableWidgetItem(cell.kept ? tr("%1 (kept)").arg(cell.text) : cell.text);
            item->setData(Qt::UserRole, cell.key);
            if (cell.kept) {
                // Kept from an earlier step: said, and quieter than a value set here.
                item->setForeground(toneColour(Tone::Detail));
                QFont italic = item->font();
                italic.setItalic(true);
                item->setFont(italic);
                item->setToolTip(tr("Set in an earlier step and kept in this one"));
            }
            m_steps->setItem(r, c, item);
        }
    }
}

void MainWindow::showDeck(const QString &path, int run)
{
    // The toolkit's own input deck, as it wrote it: read-only, whole.
    QFile f(path);
    auto *dock = findChild<QDockWidget *>(QStringLiteral("deckDock"));
    if (!f.open(QIODevice::ReadOnly)) {
        m_deck->setPlainText(tr("Cannot read %1").arg(path));
        return;
    }
    m_deck->setPlainText(QString::fromUtf8(f.readAll()));
    if (dock) {
        dock->setWindowTitle(tr("Deck (run %1)").arg(run));
        dock->setToolTip(QDir::cleanPath(path));
    }
}

void MainWindow::showGroupEditing(const QString &group)
{
    // A group's name changed (followed wherever the study uses it), or the
    // group deleted if nothing does.
    auto *name = new QLineEdit(group, m_properties);
    name->setObjectName(QStringLiteral("groupRename"));
    m_propertyForm->addRow(tr("New name"), name);
    auto *buttons = new QWidget(m_properties);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 6, 0, 6);
    auto *rename = new QPushButton(icon("pqApply"), tr("Rename"), buttons);
    rename->setObjectName(QStringLiteral("renameGroup"));
    auto *remove = new QPushButton(icon("pqDelete"), tr("Delete group"), buttons);
    remove->setObjectName(QStringLiteral("deleteGroup"));
    remove->setToolTip(tr("Only a group no support, load or material uses"));
    row->addWidget(rename);
    row->addWidget(remove);
    row->addStretch();
    m_propertyForm->addRow(buttons);
    // A finer mesh on a group of faces (the mesh step's "refine"): its
    // largest element there; empty, as the rest of the mesh.
    const core::Group *faceGroup = m_study.setup.group(group);
    const std::vector<const core::StudyStep *> meshes = m_study.stepsOf("mesh");
    if (faceGroup && !faceGroup->faces.empty() && !meshes.empty() && meshes.front()->by == "gridforge") {
        const QJsonValue now = meshes.front()->parameters["refine"].toObject()[group];
        const QString unit = m_study.setup.units == core::Units::MillimetreNewtonSecond ? "mm" : "m";
        auto *finer = new QLineEdit(now.isDouble() ? QString::number(now.toDouble()) : QString(), m_properties);
        finer->setObjectName(QStringLiteral("finerMesh"));
        finer->setPlaceholderText(tr("as the rest of the mesh"));
        finer->setToolTip(tr("The largest element on this group's faces, finer than the rest of the mesh"));
        auto *finerRow = new QWidget(m_properties);
        auto *finerLayout = new QHBoxLayout(finerRow);
        finerLayout->setContentsMargins(0, 0, 0, 0);
        finerLayout->addWidget(finer);
        finerLayout->addWidget(new QLabel(unit, finerRow));
        auto *setFiner = new QPushButton(icon("pqApply"), tr("Set"), finerRow);
        setFiner->setObjectName(QStringLiteral("applyFinerMesh"));
        finerLayout->addWidget(setFiner);
        m_propertyForm->addRow(tr("Mesh finer here"), finerRow);
        auto doFiner = [this, finer, group, unit] {
            bool ok = true;
            const QString text = finer->text().trimmed();
            const double size = text.isEmpty() ? 0 : text.toDouble(&ok);
            QString error;
            if (!ok || !core::setFinerMesh(m_study.folder, group, size, &error)) {
                say(tr("Cannot mesh %1 finer: %2").arg(group, ok ? error : tr("%1 is not a size").arg(text)),
                    Tone::Problem);
                return;
            }
            say(size > 0 ? tr("%1 meshed finer: %2 %3").arg(group, QString::number(size), unit)
                         : tr("%1 meshed as the rest").arg(group),
                Tone::Done);
            openStudy(m_study.folder);
            for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toString() == "group:" + group) {
                    m_tree->setCurrentItem(*it);
                    break;
                }
        };
        connect(setFiner, &QPushButton::clicked, this, doFiner);
        connect(finer, &QLineEdit::returnPressed, this, doFiner);
    }
    // Faces no longer those it was made on: said, and kept as they are
    // now only when asked (study spec 4.2).
    if (const core::Group *g = m_study.setup.group(group)) {
        const QString changed = mesh::faceChange(*g, m_mesh, m_study.setup.units);
        if (!changed.isEmpty()) {
            auto *why = new QLabel(changed, m_properties);
            why->setObjectName(QStringLiteral("facesChanged"));
            why->setWordWrap(true);
            QPalette p = why->palette();
            p.setColor(QPalette::WindowText, toneColour(Tone::Problem));
            why->setPalette(p);
            m_propertyForm->addRow(tr("Faces"), why);
            auto *keep = new QPushButton(icon("pqApply"), tr("Keep the faces as they are now"), m_properties);
            keep->setObjectName(QStringLiteral("keepFaces"));
            keep->setToolTip(tr("Sign the group's faces again as the geometry has them now"));
            m_propertyForm->addRow(keep);
            const std::vector<int> faces = g->faces;
            connect(keep, &QPushButton::clicked, this, [this, group, faces] {
                QString error;
                if (!core::signGroup(m_study.folder, group, signaturesOf(faces), &error)) {
                    say(tr("Cannot keep the faces: %1").arg(error), Tone::Problem);
                    return;
                }
                say(tr("Group %1 kept on %2 as they are now").arg(group, facesText(faces)), Tone::Done);
                openStudy(m_study.folder);
                for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
                    if ((*it)->data(0, Qt::UserRole).toString() == "group:" + group) {
                        m_tree->setCurrentItem(*it);
                        break;
                    }
            });
        }
    }
    auto doRename = [this, name, group] {
        const QString to = name->text().trimmed();
        if (to == group)
            return;
        QString error;
        if (!core::renameGroup(m_study.folder, group, to, &error)) {
            say(tr("Cannot rename %1: %2").arg(group, error), Tone::Problem);
            return;
        }
        say(tr("Group %1 renamed %2, wherever the study uses it").arg(group, to), Tone::Done);
        openStudy(m_study.folder);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toString() == "group:" + to) {
                m_tree->setCurrentItem(*it);
                break;
            }
    };
    connect(rename, &QPushButton::clicked, this, doRename);
    connect(name, &QLineEdit::returnPressed, this, doRename);
    connect(remove, &QPushButton::clicked, this, [this, group] {
        const core::Group *g = m_study.setup.group(group);
        QStringList what;
        if (g && !g->faces.empty())
            what << facesText(g->faces);
        if (g && !g->solids.empty())
            what << (g->solids.size() == 1 ? tr("solid %1").arg(g->solids[0]) : tr("%1 solids").arg(g->solids.size()));
        QString error;
        if (!core::deleteGroup(m_study.folder, group, &error)) {
            say(tr("Cannot delete: %1").arg(error), Tone::Problem);
            return;
        }
        say(tr("Group %1 deleted (%2)").arg(group, what.join("; ")), Tone::Done);
        openStudy(m_study.folder);
    });
}

std::map<int, core::FaceSignature> MainWindow::signaturesOf(const std::vector<int> &faces) const
{
    std::map<int, core::FaceSignature> out;
    for (int f : faces)
        if (const auto s = mesh::faceSignature(m_mesh, f))
            out[f] = *s;
    return out;
}

void MainWindow::checkGroups()
{
    m_changedGroup.clear();
    if (m_mesh.points.empty())
        return;
    for (const core::Group &g : m_study.setup.groups) {
        const QString changed = mesh::faceChange(g, m_mesh, m_study.setup.units);
        if (changed.isEmpty())
            continue;
        if (m_changedGroup.isEmpty())
            m_changedGroup = g.name;
        say(changed, Tone::Problem);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toString() == "group:" + g.name) {
                (*it)->setText(1, tr("%1; its faces changed").arg((*it)->text(1)));
                (*it)->setForeground(1, toneColour(Tone::Problem));
                (*it)->setToolTip(1, changed);
            }
    }
    updateActions();
}

bool MainWindow::newStudy(const QString &geometry, const QString &folder)
{
    QString error;
    const double size = mesh::suggestedSize(geometry, &error);
    if (size <= 0 || !core::createStudy(folder, geometry, size, &error)) {
        say(tr("Cannot make the study: %1").arg(error), Tone::Problem);
        return false;
    }
    say(tr("Study made in %1 for %2, its largest element %3 (a 25th of the part's size; change it in the Mesh "
           "step's Properties)")
            .arg(folder, QFileInfo(geometry).fileName())
            .arg(size),
        Tone::Done);
    return openStudy(folder);
}

QString MainWindow::freecadGroupsFile() const
{
    for (const core::StudyStep *i : m_study.stepsOf("input")) {
        const QFileInfo part(QDir(m_study.folder).filePath(i->file));
        const QString path = part.dir().filePath(part.completeBaseName() + ".groups.json");
        if (QFile::exists(path))
            return path;
    }
    return {};
}

int MainWindow::freecadGroupsMissing() const
{
    QFile f(freecadGroupsFile());
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    int missing = 0;
    const QJsonObject groups = QJsonDocument::fromJson(f.readAll()).object()["groups"].toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it)
        missing += m_study.setup.group(it.key()) ? 0 : 1;
    return missing;
}

void MainWindow::offerFreecadGroups()
{
    const QString path = freecadGroupsFile();
    if (path.isEmpty() || m_mesh.points.empty())
        return;
    const int missing = freecadGroupsMissing();
    if (missing > 0)
        say(tr("%1 beside the geometry holds %2 not in the study: Study, Add Groups From FreeCAD")
                .arg(QFileInfo(path).fileName(), missing == 1 ? tr("1 group") : tr("%1 groups").arg(missing)));
}

void MainWindow::addFreecadGroups()
{
    const QString path = freecadGroupsFile();
    const std::vector<const core::StudyStep *> inputs = m_study.stepsOf("input");
    if (path.isEmpty() || inputs.empty() || m_mesh.points.empty())
        return;
    const mesh::GroupsFile found =
        mesh::readGroupsFile(path, QFileInfo(inputs.front()->file).fileName(), m_mesh, m_study.setup.units);
    const QString file = QFileInfo(path).fileName();
    if (!found.error.isEmpty()) {
        say(tr("%1: %2").arg(file, found.error), Tone::Problem);
        return;
    }
    for (const QString &problem : found.problems)
        say(problem, Tone::Problem);
    int added = 0;
    for (const core::Group &g : found.groups) {
        if (m_study.setup.group(g.name)) {
            say(tr("a group named %1 is in the study already; left as it is").arg(g.name), Tone::Warning);
            continue;
        }
        QString error;
        if (core::addGroup(m_study.folder, g, &error))
            ++added;
        else
            say(tr("Cannot add group %1: %2").arg(g.name, error), Tone::Problem);
    }
    if (added > 0) {
        say(tr("%1 from %2 added").arg(added == 1 ? tr("1 group") : tr("%1 groups").arg(added), file), Tone::Done);
        openStudy(m_study.folder);
    }
}

void MainWindow::showCompareForm(const QString &id)
{
    endEdgePick();
    while (m_propertyForm->count() > 0) {
        QLayoutItem *row = m_propertyForm->takeAt(0);
        delete row->widget();
        delete row;
    }
    // A comparison kept in the study (kind "compare"), or a new one.
    const core::StudyStep *kept = id.isEmpty() ? nullptr : m_study.step(id);
    const core::StudyStep *test = nullptr;
    if (kept && kept->inputs.count("measured"))
        test = m_study.step(kept->inputs.at("measured").value(0).section('.', 0, 0));
    auto *fileRow = new QWidget(m_properties);
    auto *fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    auto *file = new QLineEdit(test ? test->file : QString(), fileRow);
    file->setObjectName(QStringLiteral("testFile"));
    file->setPlaceholderText(tr("TenSense's analysed curve, a .vtp"));
    file->setEnabled(!kept);  // a kept comparison's curve is in the study's inputs
    auto *choose = new QPushButton(tr("Choose..."), fileRow);
    choose->setEnabled(!kept);
    fileLayout->addWidget(file, 1);
    fileLayout->addWidget(choose);
    m_propertyForm->addRow(tr("Test"), fileRow);
    connect(choose, &QPushButton::clicked, this, [this, file] {
        const QString f = QFileDialog::getOpenFileName(this, tr("A test's curve"), m_study.folder,
                                                       tr("TenSense curves (*.vtp)"));
        if (!f.isEmpty())
            file->setText(f);
    });
    // Where the force acts: a group of faces.
    auto *group = new QComboBox(m_properties);
    group->setObjectName(QStringLiteral("forceOn"));
    for (const core::Group &g : m_study.setup.groups)
        if (!g.faces.empty())
            group->addItem(g.name);
    if (kept)
        group->setCurrentText(kept->parameters["forceOn"].toString());
    m_propertyForm->addRow(tr("Force on"), group);
    // The extensometer: two points of the part as it was made.
    auto pointText = [&](int k) {
        if (!kept)
            return QString();
        const QJsonArray p = kept->parameters["gauge"].toArray()[k].toArray();
        return QString("%1, %2, %3").arg(p[0].toDouble()).arg(p[1].toDouble()).arg(p[2].toDouble());
    };
    auto *from = new QLineEdit(pointText(0), m_properties), *to = new QLineEdit(pointText(1), m_properties);
    from->setObjectName(QStringLiteral("gaugeFrom"));
    to->setObjectName(QStringLiteral("gaugeTo"));
    from->setPlaceholderText(tr("x, y, z"));
    to->setPlaceholderText(tr("x, y, z"));
    m_propertyForm->addRow(tr("Extensometer from"), from);
    m_propertyForm->addRow(tr("to"), to);
    auto *share = new QSpinBox(m_properties);
    share->setObjectName(QStringLiteral("modelShare"));
    share->setRange(1, 64);
    share->setPrefix(tr("1/"));
    share->setValue(kept ? kept->parameters["modelShare"].toInt(1) : 1);
    share->setToolTip(tr("The model's part of the test piece by symmetry (a quarter: 1/4); its force is taken that "
                         "many times"));
    m_propertyForm->addRow(tr("Model is"), share);
    auto *buttons = new QWidget(m_properties);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 6, 0, 6);
    auto *apply = new QPushButton(icon("pqApply"), kept ? tr("Apply") : tr("Compare"), buttons);
    apply->setObjectName(QStringLiteral("applyCompare"));
    row->addWidget(apply);
    if (kept) {
        auto *remove = new QPushButton(icon("pqDelete"), tr("Remove comparison"), buttons);
        remove->setObjectName(QStringLiteral("removeCompare"));
        row->addWidget(remove);
        connect(remove, &QPushButton::clicked, this, [this, id] {
            QString error;
            if (!core::removeComparison(m_study.folder, id, &error)) {
                say(tr("Cannot remove the comparison: %1").arg(error), Tone::Problem);
                return;
            }
            say(tr("Comparison %1 taken out of the study").arg(id), Tone::Done);
            openStudy(m_study.folder);
        });
    }
    row->addStretch();
    m_propertyForm->addRow(buttons);
    connect(apply, &QPushButton::clicked, this, [this, id, file, group, from, to, share] {
        auto point = [](const QString &text, std::array<double, 3> *out) {
            const QStringList xyz = text.split(',');
            bool ok = xyz.size() == 3;
            for (int d = 0; ok && d < 3; ++d)
                (*out)[d] = xyz[d].trimmed().toDouble(&ok);
            return ok;
        };
        std::array<double, 3> a{}, b{};
        if (!point(from->text(), &a) || !point(to->text(), &b)) {
            say(tr("Cannot compare: the gauge's points are each x, y, z"), Tone::Problem);
            return;
        }
        QString error, kept = id;
        const bool done = id.isEmpty()
                              ? core::addComparison(m_study.folder, file->text().trimmed(), group->currentText(), a, b,
                                                    share->value(), &kept, &error)
                              : core::setComparison(m_study.folder, id, group->currentText(), a, b, share->value(),
                                                    &error);
        if (!done) {
            say(tr("Cannot compare: %1").arg(error), Tone::Problem);
            return;
        }
        say(id.isEmpty() ? tr("Comparison %1 kept in the study; it runs again with every solve").arg(kept)
                         : tr("Comparison %1 changed").arg(kept),
            Tone::Done);
        openStudy(m_study.folder);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toString() == "compare:" + kept) {
                m_tree->setCurrentItem(*it);
                break;
            }
        if (m_results.timeCount() > 0)
            runComparisons(m_results.path(), kept);
        if (auto *d = findChild<QDockWidget *>("compareDock")) {
            d->show();
            d->raise();
        }
    });
}

void MainWindow::showMaterialForm()
{
    endEdgePick();
    while (m_propertyForm->count() > 0) {
        QLayoutItem *row = m_propertyForm->takeAt(0);
        delete row->widget();
        delete row;
    }
    const bool metres = m_study.setup.units == core::Units::MetreNewtonSecond;
    auto *name = new QLineEdit(m_properties);
    name->setObjectName(QStringLiteral("materialName"));
    name->setPlaceholderText(tr("e.g. Steel"));
    m_propertyForm->addRow(tr("Name"), name);
    // A FreeCAD card, or the values by hand; values typed override the card's.
    auto *cardRow = new QWidget(m_properties);
    auto *cardLayout = new QHBoxLayout(cardRow);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    auto *card = new QLineEdit(cardRow);
    card->setObjectName(QStringLiteral("materialCard"));
    card->setPlaceholderText(tr("a FreeCAD .FCMat, or leave empty"));
    auto *choose = new QPushButton(tr("Choose..."), cardRow);
    cardLayout->addWidget(card);
    cardLayout->addWidget(choose);
    connect(choose, &QPushButton::clicked, this, [this, card] {
        const QString f = QFileDialog::getOpenFileName(this, tr("A FreeCAD material card"), {},
                                                       tr("FreeCAD materials (*.FCMat)"));
        if (!f.isEmpty())
            card->setText(f);
    });
    m_propertyForm->addRow(tr("FreeCAD card"), cardRow);
    auto *e = new QLineEdit(m_properties);
    e->setObjectName(QStringLiteral("materialE"));
    e->setPlaceholderText(tr("from the card"));
    m_propertyForm->addRow(tr("Young's modulus (%1)").arg(metres ? "Pa" : "MPa"), e);
    auto *nu = new QLineEdit(m_properties);
    nu->setObjectName(QStringLiteral("materialNu"));
    nu->setPlaceholderText(tr("from the card"));
    m_propertyForm->addRow(tr("Poisson's ratio"), nu);
    // For a group of solids; with none, a group of every solid is made.
    auto *assign = new QComboBox(m_properties);
    assign->setObjectName(QStringLiteral("materialAssign"));
    for (const core::Group &g : m_study.setup.groups)
        if (!g.solids.empty())
            assign->addItem(g.name);
    const QString everySolid = tr("Every solid (a new group, Part)");
    if (assign->count() == 0)
        assign->addItem(everySolid);
    m_propertyForm->addRow(tr("For"), assign);
    auto *apply = new QPushButton(icon("pqApply"), tr("Add material"), m_properties);
    apply->setObjectName(QStringLiteral("applyMaterial"));
    m_propertyForm->addRow(apply);
    connect(apply, &QPushButton::clicked, this, [this, name, card, e, nu, assign, everySolid] {
        std::map<QString, double> values;
        bool ok = true;
        if (!e->text().trimmed().isEmpty())
            values["YoungsModulus"] = e->text().trimmed().toDouble(&ok);
        bool okNu = true;
        if (!nu->text().trimmed().isEmpty())
            values["PoissonRatio"] = nu->text().trimmed().toDouble(&okNu);
        if (!ok || !okNu) {
            say(tr("Cannot add the material: a value is not a number"), Tone::Problem);
            return;
        }
        const QString material = name->text().trimmed();
        QString error;
        if (!core::addMaterial(m_study.folder, material, values, card->text().trimmed(), &error)) {
            say(tr("Cannot add the material: %1").arg(error), Tone::Problem);
            return;
        }
        QString group = assign->currentText();
        if (group == everySolid) {
            group = QStringLiteral("Part");
            std::vector<int> solids;
            while (mesh::solidSignature(m_mesh, static_cast<int>(solids.size()) + 1))
                solids.push_back(static_cast<int>(solids.size()) + 1);
            if (solids.empty())
                solids.push_back(1);  // no mesh yet: a part of one solid
            if (!core::addGroup(m_study.folder, core::Group{group, {}, solids, {}}, &error)) {
                say(tr("Cannot make the group of every solid: %1").arg(error), Tone::Problem);
                return;
            }
        }
        if (!core::assignMaterial(m_study.folder, group, material, &error)) {
            say(tr("Cannot assign the material: %1").arg(error), Tone::Problem);
            return;
        }
        say(tr("Material %1 added, for %2").arg(material, group), Tone::Done);
        openStudy(m_study.folder);
    });
    name->setFocus();
}

void MainWindow::makeGroup()
{
    auto *name = m_properties->findChild<QLineEdit *>(QStringLiteral("groupName"));
    const QString named = name ? name->text().trimmed() : QString();
    const std::vector<int> faces = m_picked;
    QString error;
    if (!core::addFaceGroup(m_study.folder, named, faces, &error, signaturesOf(faces))) {
        say(tr("Cannot make the group: %1").arg(error), Tone::Problem);
        return;
    }
    say(tr("Group %1 made of %2, in study.json").arg(named, facesText(faces)), Tone::Done);
    openStudy(m_study.folder);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == "group:" + named) {
            m_tree->setCurrentItem(*it);
            break;
        }
}

double MainWindow::autoDeform(double modelSize, double largestDisplacement)
{
    if (!(largestDisplacement > 0) || !(modelSize > 0))
        return 1;
    // A hair over: 0.1 x 5 / 1e-5 is 49999.99999999999 in doubles, and must
    // still give 50000, not 20000.
    const double raw = 0.1 * modelSize / largestDisplacement * (1 + 1e-9);
    if (raw <= 1)
        return 1;
    const double power = std::pow(10.0, std::floor(std::log10(raw)));
    const double m = raw / power;
    return (m >= 5 ? 5 : m >= 2 ? 2 : 1) * power;
}

std::pair<std::array<double, 3>, std::array<double, 3>>
MainWindow::viewAcross(const std::array<double, 3> &view, const std::array<double, 3> &up, const std::array<double, 3> &moved)
{
    using V = std::array<double, 3>;
    auto dot = [](const V &a, const V &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    auto less = [](const V &a, double k, const V &b) { return V{a[0] - k * b[0], a[1] - k * b[1], a[2] - k * b[2]}; };
    auto unit = [&](const V &a) {
        const double n = std::sqrt(dot(a, a));
        return n > 1e-9 ? V{a[0] / n, a[1] / n, a[2] / n} : V{0, 0, 0};
    };
    const V d = unit(moved), v = unit(view);
    const double reach = std::cos(70 * M_PI / 180);
    if (dot(d, d) == 0 || dot(v, v) == 0)
        return {view, up};
    const double c = dot(v, d);
    if (std::abs(c) <= reach)
        return {view, up};
    // The way to tip towards: the view's own part across the movement;
    // looking straight along it, away from up, so the top comes into view.
    V across = unit(less(v, c, d));
    if (dot(across, across) == 0)
        across = unit(less(V{-up[0], -up[1], -up[2]}, -dot(up, d), d));
    if (dot(across, across) == 0) {
        const int i = std::abs(d[0]) <= std::abs(d[1]) && std::abs(d[0]) <= std::abs(d[2]) ? 0
                      : std::abs(d[1]) <= std::abs(d[2])                                   ? 1
                                                                                           : 2;
        V axis{0, 0, 0};
        axis[i] = 1;
        across = unit(less(axis, d[i], d));
    }
    const double side = c > 0 ? 1 : -1, tipped = std::sin(70 * M_PI / 180);
    const V turned{side * reach * d[0] + tipped * across[0], side * reach * d[1] + tipped * across[1],
                   side * reach * d[2] + tipped * across[2]};
    V upright = unit(less(up, dot(up, turned), turned));
    if (dot(upright, upright) == 0)
        upright = unit(less(V{-d[0], -d[1], -d[2]}, -dot(d, turned), turned));
    return {turned, upright};
}

double MainWindow::legendLeft(double viewPixels, double boxPixels, double titlePixels)
{
    if (viewPixels <= 0)
        return 0.86;
    const double fit = (viewPixels - 8 - (boxPixels + titlePixels) / 2) / viewPixels;
    return std::clamp(fit, 0.0, 0.86);
}

void MainWindow::placeLegend()
{
    if (!m_scalarBar || !m_scalarBar->GetTitle())
        return;
    const int *size = m_renderer->GetSize();
    int box[4] = {0, 0, 0, 0};
    vtkTextRenderer::GetInstance()->GetBoundingBox(m_scalarBar->GetTitleTextProperty(), m_scalarBar->GetTitle(), box,
                                                   m_view->renderWindow()->GetDPI());
    const double boxPixels = std::min(0.12 * size[0], double(m_scalarBar->GetMaximumWidthInPixels()));
    m_scalarBar->SetPosition(legendLeft(size[0], boxPixels, box[1] - box[0]), 0.12);
}

QString MainWindow::latestResults() const
{
    const std::vector<const core::StudyStep *> solves = m_study.stepsOf("solve");
    if (solves.empty())
        return {};
    const QDir runs(QDir(m_study.folder).filePath("steps/" + solves.front()->id + "/runs"));
    int best = 0;
    for (const QString &n : runs.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (QFileInfo(runs.filePath(n + "/results/results.vtkhdf")).isFile())
            best = std::max(best, n.toInt());
    return best ? runs.filePath(QString("%1/results/results.vtkhdf").arg(best)) : QString();
}

bool MainWindow::showResults(const QString &resultsFile)
{
    QString error;
    if (!m_results.open(resultsFile, &error)) {
        say(tr("Cannot show the results: %1").arg(error), Tone::Problem);
        return false;
    }
    const QString run = QFileInfo(resultsFile).absoluteDir().absolutePath().section('/', -2, -2);
    // Who solved them and how far to trust them (tenet 9): from results.json
    // beside them; if it is missing, that is said, never left blank.
    QFile report(QFileInfo(resultsFile).absoluteDir().filePath("results.json"));
    const bool reported = report.open(QIODevice::ReadOnly);
    const QJsonObject r = reported ? QJsonDocument::fromJson(report.readAll()).object() : QJsonObject();
    if (!reported)
        say(tr("Run %1 has no results.json: who solved it, and how far to trust it, is not known").arg(run),
            Tone::Warning);
    for (QTreeWidgetItem *i : m_tree->findItems(tr("Results"), Qt::MatchExactly)) {
        i->setText(1, reported ? tr("run %1, %2 by %3").arg(run, r["trust"].toString(), r["solver"].toString())
                               : tr("run %1, who solved it not known").arg(run));
        i->setToolTip(1, resultsFile);
    }
    // The job that made them, and what the results say about themselves
    // (tenet 9: a value that cannot be, extrapolated by the solver).
    if (!m_runner.running()) {
        const std::vector<const core::StudyStep *> solves = m_study.stepsOf("solve");
        if (!solves.empty()) {
            m_solveStep = solves.front()->id;
            const core::ToolkitSet toolkits = core::findToolkits(core::shippedToolkitFolders(QCoreApplication::applicationDirPath(), GF_SHIPPED_TOOLKITS));
            const core::Toolkit *t = toolkits.find(solves.front()->by);
            m_toolkitName = t ? t->name : solves.front()->by;
            setJobState(tr("solved (run %1)").arg(run));
            m_runNumber = run.toInt();
            m_outcome = WindowState::Outcome::Solved;
        }
    }
    if (r.contains("deck"))
        showDeck(QFileInfo(resultsFile).absoluteDir().filePath("../" + r["deck"].toString()), run.toInt());
    for (const QJsonValue &note : r["notes"].toArray())
        say(tr("Note on run %1: %2").arg(run, note.toString()), Tone::Warning);
    {
        const QSignalBlocker quiet(m_colourBy);
        m_colourBy->clear();
        m_colourBy->addItems(m_results.fields());
        m_colourBy->setCurrentText(m_results.preferredField());
    }
    m_increment = m_results.timeCount() - 1;
    m_layouts->setCurrentIndex(1);
    showIncrement(m_increment);
    updateActions();
    runComparisons(resultsFile);
    return true;
}

void MainWindow::runComparisons(const QString &resultsFile, const QString &shown)
{
    // The run the results are from: <study>/steps/<solve>/runs/<n>/results/.
    QDir runFolder = QFileInfo(resultsFile).absoluteDir();
    runFolder.cdUp();
    const int run = runFolder.dirName().toInt();
    const QString solve = QFileInfo(runFolder.absolutePath()).absoluteDir().absolutePath().section('/', -2, -2);
    bool plotted = false;
    for (const core::StudyStep *c : m_study.stepsOf("compare")) {
        const auto from = c->inputs.find("results");
        if (from == c->inputs.end() || from->second.value(0).section('.', 0, 0) != solve)
            continue;
        QString error;
        const core::ComparisonRun done = core::runComparison(m_study, *c, resultsFile, run, &error);
        if (!error.isEmpty()) {
            say(tr("Cannot compare %1 with run %2: %3").arg(c->id).arg(run).arg(error), Tone::Problem);
            continue;
        }
        const core::StudyStep *test = m_study.step(c->inputs.at("measured").value(0).section('.', 0, 0));
        say(tr("Run %1 against %2: %3").arg(run).arg(test ? QFileInfo(test->file).fileName() : c->id, done.words),
            Tone::Done);
        if (!plotted && (shown.isEmpty() || shown == c->id)) {
            m_curvePlot->setCurves(done.measured, done.predicted);
            plotted = true;
        }
    }
}

void MainWindow::showIncrement(int k)
{
    if (m_results.timeCount() == 0)
        return;
    m_increment = std::clamp(k, 0, m_results.timeCount() - 1);
    const QString field = m_colourBy->currentText();
    const double deform = m_results.canDeform() ? m_deform->value() : 0;
    vtkSmartPointer<vtkUnstructuredGrid> model = m_results.model(m_increment, field, deform);
    if (!model)
        return;
    double range[2] = {0, 1};
    if (vtkDataArray *shown = model->GetPointData()->GetArray("shown"))
        shown->GetRange(range);
    if (range[1] <= range[0])
        range[1] = range[0] + 1e-12;

    // ParaView's default colours, "Cool to Warm".
    vtkNew<vtkColorTransferFunction> colours;
    colours->SetColorSpaceToDiverging();
    colours->AddRGBPoint(range[0], 0.231373, 0.298039, 0.752941);
    colours->AddRGBPoint((range[0] + range[1]) / 2, 0.865003, 0.865003, 0.865003);
    colours->AddRGBPoint(range[1], 0.705882, 0.0156863, 0.14902);
    vtkNew<vtkDataSetSurfaceFilter> surface;
    surface->SetInputData(model);
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(surface->GetOutputPort());
    mapper->SetLookupTable(colours);
    mapper->SetScalarModeToUsePointFieldData();
    mapper->SelectColorArray("shown");
    mapper->SetScalarRange(range);
    mapper->ScalarVisibilityOn();
    if (!m_resultsActor) {
        m_resultsActor = vtkActor::New();
        m_scalarBar = vtkScalarBarActor::New();
        // As ParaView's legend: plain text at fixed sizes, four digits.
        m_scalarBar->SetNumberOfLabels(5);
        m_scalarBar->SetLabelFormat("%-#.4g");
        m_scalarBar->UnconstrainedFontSizeOn();
        m_scalarBar->SetBarRatio(0.2);
        m_scalarBar->SetMaximumWidthInPixels(130);
        for (vtkTextProperty *t : {m_scalarBar->GetTitleTextProperty(), m_scalarBar->GetLabelTextProperty()}) {
            t->BoldOff();
            t->ItalicOff();
            t->ShadowOff();
            t->SetColor(1, 1, 1);
        }
        m_scalarBar->GetTitleTextProperty()->SetFontSize(14);
        m_scalarBar->GetLabelTextProperty()->SetFontSize(13);
        m_scalarBar->SetVerticalTitleSeparation(6);  // the top label clear of the title
        m_scalarBar->SetPosition(0.86, 0.12);  // moved left for a long title: placeLegend
        m_scalarBar->SetWidth(0.12);
        m_scalarBar->SetHeight(0.7);
    }
    m_resultsActor->SetMapper(mapper);
    m_scalarBar->SetLookupTable(colours);
    m_scalarBar->SetTitle(field.toUtf8().constData());
    // Said beside the increment, so an exaggerated shape is never taken
    // for the real one (tenet 9).
    const QString shape = !m_results.canDeform() ? tr("undeformed (no displacements)")
                          : deform == 0          ? tr("undeformed")
                          : deform == 1          ? tr("true scale")
                                                 : tr("deformed \u00d7%1").arg(deform);
    m_increments->setText(tr(" increment %1 of %2, time %3, %4 ")
                              .arg(m_increment + 1)
                              .arg(m_results.timeCount())
                              .arg(m_results.time(m_increment))
                              .arg(shape));
    showLayer(m_layouts->currentIndex());
    updateActions();
}

void MainWindow::showLayer(int layout)
{
    m_renderer->RemoveAllViewProps();
    if (layout == 1 && m_resultsActor) {
        m_renderer->AddActor(m_resultsActor);
        m_renderer->AddViewProp(m_scalarBar);
    } else if (m_meshActor) {
        m_renderer->AddActor(m_meshActor);
        m_renderer->AddActor(m_edgesActor);
        if (m_markersActor)
            m_renderer->AddActor(m_markersActor);
    }
    m_view->renderWindow()->Render();
}

void MainWindow::refreshMarkers()
{
    if (m_markersActor) {
        m_renderer->RemoveActor(m_markersActor);
        m_markersActor->Delete();
        m_markersActor = nullptr;
    }
    const std::vector<Marker> markers = markersFor(m_study, m_step ? m_step->currentIndex() : 0, m_surface);
    if (markers.empty()) {
        m_view->renderWindow()->Render();
        return;
    }
    // One size for all, a part of the model's (not scaled by the load).
    double b[6];
    m_surface->GetBounds(b);
    const double length = 0.15 * std::sqrt((b[1] - b[0]) * (b[1] - b[0]) + (b[3] - b[2]) * (b[3] - b[2])
                                           + (b[5] - b[4]) * (b[5] - b[4]));
    // Coloured by kind (ANSYS, PrePoMax), each far from ember, which is
    // selection's alone: supports blue, moves violet, forces yellow (as
    // Abaqus/CAE's), pressures magenta.
    auto colourOf = [](const QString &role) -> std::array<unsigned char, 3> {
        if (role == "support")
            return {70, 130, 220};
        if (role == "move")
            return {160, 110, 230};
        if (role == "force")
            return {235, 200, 40};
        return {210, 70, 170};  // pressure
    };
    const QString selected = selectedKey();
    vtkNew<vtkAppendPolyData> all;
    for (const Marker &m : markers) {
        vtkSmartPointer<vtkPolyDataAlgorithm> source;
        // Both sources lie along +x; placed, turned onto the direction, sized.
        double start[3];
        double scale = length;
        if (m.shape == Marker::Shape::Arrow) {
            vtkNew<vtkArrowSource> arrow;
            arrow->SetTipResolution(16);
            arrow->SetShaftResolution(12);
            arrow->SetShaftRadius(0.05);  // bolder than VTK's 0.03, to read at a glance
            arrow->SetTipRadius(0.13);
            arrow->SetTipLength(0.3);
            source = arrow;
            for (int k = 0; k < 3; ++k)
                start[k] = m.outward ? m.at[k] : m.at[k] - length * m.direction[k];
        } else {
            vtkNew<vtkConeSource> cone;
            cone->SetResolution(16);
            cone->SetHeight(0.3);
            cone->SetRadius(0.1);
            cone->SetCenter(-0.15, 0, 0);  // its tip at the origin
            source = cone;
            for (int k = 0; k < 3; ++k)
                start[k] = m.at[k];
        }
        vtkNew<vtkTransform> place;
        place->Translate(start);
        // Turned from +x onto the direction, about their cross product.
        const double d[3] = {m.direction[0], m.direction[1], m.direction[2]};
        const double cross[3] = {0, -d[2], d[1]};
        const double sine = std::sqrt(cross[1] * cross[1] + cross[2] * cross[2]);
        const double angle = std::atan2(sine, d[0]) * 180 / 3.14159265358979323846;
        if (sine > 1e-12)
            place->RotateWXYZ(angle, cross[0], cross[1], cross[2]);
        else if (d[0] < 0)
            place->RotateWXYZ(180, 0, 0, 1);
        place->Scale(scale, scale, scale);
        vtkNew<vtkTransformPolyDataFilter> placed;
        placed->SetInputConnection(source->GetOutputPort());
        placed->SetTransform(place);
        placed->Update();
        auto piece = vtkSmartPointer<vtkPolyData>::New();
        piece->DeepCopy(placed->GetOutput());
        vtkNew<vtkUnsignedCharArray> colours;
        colours->SetName("colour");
        colours->SetNumberOfComponents(3);
        const std::array<unsigned char, 3> c =
            m.key == selected ? std::array<unsigned char, 3>{226, 92, 29} : colourOf(m.role);
        for (vtkIdType i = 0; i < piece->GetNumberOfPoints(); ++i)
            colours->InsertNextTypedTuple(c.data());
        piece->GetPointData()->SetScalars(colours);
        all->AddInputData(piece);
    }
    all->Update();
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(all->GetOutputPort());
    mapper->SetScalarModeToUsePointData();
    mapper->SetColorModeToDirectScalars();
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->PickableOff();  // a click goes through to the face under it
    m_markersActor = actor;
    actor->Register(nullptr);
    if (m_layouts->currentIndex() == 0)
        m_renderer->AddActor(actor);
    m_view->renderWindow()->Render();
}

void MainWindow::startEdgePick(LoadForm *form)
{
    endEdgePick();
    if (m_mesh.points.empty())
        return;
    if (!m_geometryEdges)
        m_geometryEdges = mesh::geometryEdges(m_mesh);
    m_edgeFor = form;
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_geometryEdges);
    mapper->ScalarVisibilityOff();
    // Over the faces, so every edge can be seen and clicked.
    mapper->SetRelativeCoincidentTopologyLineOffsetParameters(-2, -2);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.96, 0.96, 0.98);
    actor->GetProperty()->SetLineWidth(3);
    actor->GetProperty()->LightingOff();
    actor->PickableOff();
    m_edgesPickActor = actor;
    actor->Register(nullptr);
    m_renderer->AddActor(actor);
    m_view->renderWindow()->Render();
    statusBar()->showMessage(tr("Click an edge of the model: the direction is along it. Another direction "
                                "in the form leaves it."));
}

void MainWindow::pickEdgeAt(const QPoint &at)
{
    if (!m_edgeFor) {
        endEdgePick();
        return;
    }
    // The edges' segments where they are drawn, in the view's pixels (Qt's:
    // y down), as onScreen in the walkthrough finds them.
    vtkDataArray *edge = m_geometryEdges->GetCellData()->GetArray("edge");
    std::vector<std::array<double, 4>> drawn;
    std::vector<int> edgeOf;
    const double k = m_view->devicePixelRatioF();
    auto toView = [&](const double *p, double &x, double &y) {
        m_renderer->SetWorldPoint(p[0], p[1], p[2], 1);
        m_renderer->WorldToDisplay();
        double d[3];
        m_renderer->GetDisplayPoint(d);
        x = d[0] / k;
        y = m_view->height() - d[1] / k;
    };
    vtkNew<vtkIdList> ids;
    for (vtkIdType c = 0; c < m_geometryEdges->GetNumberOfCells(); ++c) {
        m_geometryEdges->GetCellPoints(c, ids);
        std::array<double, 4> s;
        toView(m_geometryEdges->GetPoint(ids->GetId(0)), s[0], s[1]);
        toView(m_geometryEdges->GetPoint(ids->GetId(1)), s[2], s[3]);
        drawn.push_back(s);
        edgeOf.push_back(int(edge->GetComponent(c, 0)));
    }
    const int picked = nearestEdge(drawn, edgeOf, at.x(), at.y(), 10);
    if (picked == 0) {
        statusBar()->showMessage(tr("No edge there: click on one of the lines drawn."));
        return;
    }
    LoadForm *form = m_edgeFor;
    endEdgePick();
    form->setEdge(picked);
    statusBar()->showMessage(tr("Edge %1 picked").arg(picked), 4000);
}

void MainWindow::endEdgePick()
{
    m_edgeFor = nullptr;
    if (!m_edgesPickActor)
        return;
    m_renderer->RemoveActor(m_edgesPickActor);
    m_edgesPickActor->UnRegister(nullptr);
    m_edgesPickActor = nullptr;
    statusBar()->clearMessage();
    m_view->renderWindow()->Render();
}

} // namespace gf::gui
