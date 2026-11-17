#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"
#include "core/StudyOutline.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"
#include "mesh/Quality.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace gf::gui {

namespace {

QIcon icon(const QString &name) { return QIcon(":/icons/" + name + ".svg"); }

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
    setWindowTitle(core::applicationName() + " - " + core::applicationSubtitle());
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);  // as ParaView

    // The centre: layout tabs over the view, as ParaView's.
    auto *centre = new QWidget(this);
    auto *column = new QVBoxLayout(centre);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_layouts = new QTabBar(centre);
    m_layouts->setObjectName(QStringLiteral("layouts"));
    m_layouts->setExpanding(false);  // tabs at the left, as ParaView's
    m_layouts->addTab(tr("Model"));
    m_layouts->addTab(tr("Results"));
    m_view = new QVTKOpenGLNativeWidget(centre);
    m_view->setObjectName(QStringLiteral("view"));
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.32, 0.34, 0.43);  // ParaView's default background
    window->AddRenderer(renderer);
    m_renderer = renderer;
    m_view->setRenderWindow(window);
    column->addWidget(m_layouts);
    column->addWidget(m_view, 1);
    setCentralWidget(centre);

    buildDocks();
    buildActions();

    m_units = new QLabel(this);
    m_units->setObjectName(QStringLiteral("units"));
    statusBar()->addPermanentWidget(m_units);
    statusBar()->showMessage(tr("Open a study (File, Open Study)"));

    connect(&m_meshWatch, &QFutureWatcher<MeshDone>::finished, this, &MainWindow::onMeshed);
    resize(1400, 900);
}

MainWindow::~MainWindow()
{
    m_meshWatch.waitForFinished();
}

void MainWindow::buildDocks()
{
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("studyTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *now, QTreeWidgetItem *) { showProperties(now); });

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
    auto *steps = new QLabel(tr("Loads and supports by step: not yet"), this);
    steps->setObjectName(QStringLiteral("stepsTable"));

    QDockWidget *study = dock(this, tr("Study"), "studyDock", m_tree);
    QDockWidget *properties = dock(this, tr("Properties"), "propertiesDock", m_properties);
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
    addDockWidget(Qt::BottomDockWidgetArea, messages);
    tabifyDockWidget(messages, job);
    tabifyDockWidget(job, stepsDock);
    tabifyDockWidget(stepsDock, deck);
    messages->raise();
    resizeDocks({study}, {380}, Qt::Horizontal);
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
    menuBar()->addMenu(tr("&Study"));
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
    QAction *runAction = run->addAction(icon("pqVcrPlay"), tr("&Run"));
    runAction->setObjectName(QStringLiteral("run"));
    runAction->setEnabled(false);  // the next slice
    QAction *stop = run->addAction(icon("pqCancel"), tr("&Stop"));
    stop->setObjectName(QStringLiteral("stop"));
    stop->setEnabled(false);
    runBar->addAction(runAction);
    runBar->addAction(stop);

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
    for (const char *name : {"pqVcrFirst", "pqVcrBack", "pqVcrPlay", "pqVcrForward", "pqVcrLast"}) {
        QAction *a = vcr->addAction(icon(name), QString(name).mid(5));
        a->setEnabled(false);  // results come in a later slice
    }

    addToolBarBreak();
    QToolBar *stepBar = addToolBar(tr("Step"));
    stepBar->setObjectName(QStringLiteral("stepToolbar"));
    stepBar->addWidget(new QLabel(tr(" Step: "), this));
    m_step = new QComboBox(this);
    m_step->setObjectName(QStringLiteral("stepChooser"));
    m_step->setMinimumContentsLength(16);
    stepBar->addWidget(m_step);
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

void MainWindow::say(const QString &message, bool problem)
{
    m_messages->appendPlainText((problem ? QStringLiteral("Problem: ") : QString()) + message);
    statusBar()->showMessage(message);
}

bool MainWindow::openStudy(const QString &path)
{
    QString error;
    core::Study study = core::readStudy(path, &error);
    if (!error.isEmpty()) {
        say(tr("Cannot open the study: %1").arg(error), true);
        return false;
    }
    m_study = study;
    ++m_opening;
    m_mesh = {};
    m_renderer->RemoveAllViewProps();
    m_tree->clear();
    addItems(nullptr, m_tree, core::studyOutline(m_study));
    m_tree->expandAll();
    m_step->clear();
    for (const core::AnalysisStep &s : m_study.setup.steps)
        m_step->addItem(s.name);
    m_units->setText(unitsText(m_study.setup.units));
    setWindowTitle(QStringLiteral("%1 - %2").arg(core::applicationName(), QDir(m_study.folder).dirName()));
    say(tr("Opened %1").arg(m_study.folder));
    startMeshing();
    return true;
}

void MainWindow::showProperties(QTreeWidgetItem *item)
{
    while (QLayoutItem *row = m_propertyForm->takeAt(0)) {
        delete row->widget();
        delete row;
    }
    if (!item)
        return;
    auto value = [this](const QString &text) {
        auto *l = new QLabel(text, m_properties);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setWordWrap(true);
        return l;
    };
    m_propertyForm->addRow(tr("Name"), value(item->text(0)));
    if (!item->text(1).isEmpty())
        m_propertyForm->addRow(tr("Value"), value(item->text(1)));
    m_propertyForm->addRow(tr("Kind"), value(item->data(0, Qt::UserRole + 1).toString()));
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
        say(tr("The mesh step has no geometry"), true);
        return;
    }
    const QString file = QDir(m_study.folder).filePath(input->file);
    const mesh::Recipe recipe{step.parameters["maxSize"].toDouble(), step.parameters["order"].toInt(1)};
    const core::Setup setup = m_study.setup;
    const int opening = m_opening;
    say(tr("Meshing %1...").arg(input->file));
    // On another thread: the window keeps answering (tenet 7). It touches
    // nothing of the window's; the result comes back through the future.
    // A meshing for a study opened before may still run: it touches nothing
    // of the window's, and its result is dropped (onMeshed), so it is not
    // waited for here (the window never waits, tenet 7).
    m_meshWatch.setFuture(QtConcurrent::run([file, recipe, setup, opening] {
        MeshDone done;
        done.opening = opening;
        done.mesh = mesh::meshStep(file, recipe, &done.error);
        if (done.error.isEmpty())
            mesh::addGroupSets(done.mesh, setup, &done.error);
        return done;
    }));
}

void MainWindow::onMeshed()
{
    const MeshDone done = m_meshWatch.result();
    if (done.opening != m_opening)
        return;  // for a study since replaced
    if (!done.error.isEmpty() || done.mesh.points.empty()) {
        say(tr("The mesh could not be made: %1").arg(done.error), true);
        return;
    }
    m_mesh = done.mesh;
    vtkUnstructuredGrid *grid = m_mesh.points.front().grid;
    const mesh::Quality quality = mesh::measure(grid);

    // The surface with its edges, as ParaView's "Surface With Edges".
    vtkNew<vtkDataSetSurfaceFilter> surface;
    surface->SetInputData(grid);
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(surface->GetOutputPort());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.9, 0.9, 0.9);
    actor->GetProperty()->EdgeVisibilityOn();
    actor->GetProperty()->SetEdgeColor(0.25, 0.25, 0.35);
    m_renderer->RemoveAllViewProps();
    m_renderer->AddActor(actor);
    setCamera(-1, -1, -1, 0, 0, 1);

    const QString detail = tr("%1 tetrahedra, %2 nodes; %3")
                               .arg(grid->GetNumberOfCells())
                               .arg(grid->GetNumberOfPoints())
                               .arg(quality.solverReady() ? tr("ready for a solver") : tr("NOT ready for a solver"));
    for (QTreeWidgetItem *item : m_tree->findItems(tr("Mesh"), Qt::MatchExactly)) {
        item->setText(1, detail);
        item->setToolTip(1, detail);
    }
    m_information->setText(mesh::summary(quality).join('\n'));
    say(tr("Meshed: %1").arg(detail));
    emit meshReady();
}

} // namespace gf::gui
