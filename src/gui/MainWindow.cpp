#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"
#include "core/Job.h"
#include "core/MaterialCard.h"
#include "core/StudyOutline.h"
#include "core/Toolkit.h"
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
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSignalBlocker>

#include <algorithm>
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
#include <vtkColorTransferFunction.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>

#include <QProcess>
#include <QStandardPaths>

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
    connect(m_layouts, &QTabBar::currentChanged, this, &MainWindow::showLayer);
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
    resize(1400, 900);
}

MainWindow::~MainWindow()
{
    m_meshWatch.waitForFinished();
    for (vtkObject *o : std::initializer_list<vtkObject *>{m_meshActor, m_resultsActor, m_scalarBar})
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
    m_runAction = run->addAction(icon("pqVcrPlay"), tr("&Run"));
    m_runAction->setObjectName(QStringLiteral("run"));
    m_runAction->setShortcut(Qt::Key_F5);
    m_runAction->setToolTip(tr("Solve the study (F5)"));
    connect(m_runAction, &QAction::triggered, this, &MainWindow::run);
    m_stopAction = run->addAction(icon("pqCancel"), tr("&Stop"));
    m_stopAction->setObjectName(QStringLiteral("stop"));
    m_stopAction->setToolTip(tr("Stop the solve; its files are kept"));
    connect(m_stopAction, &QAction::triggered, &m_runner, &core::JobRunner::stop);
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
    stepBar->addWidget(m_step);

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
    m_openParaView = results->addAction(icon("pqOpen"), tr("Open in ParaView"));
    m_openParaView->setObjectName(QStringLiteral("openInParaView"));
    m_openParaView->setToolTip(tr("Hand the results file to ParaView, for everything beyond a quick look"));
    connect(m_openParaView, &QAction::triggered, this, [this] {
        const QStringList cmd =
            paraviewCommand(m_results.path(), !QStandardPaths::findExecutable("paraview").isEmpty(),
                            !QStandardPaths::findExecutable("flatpak").isEmpty()
                                && QProcess::execute("flatpak", {"info", "org.paraview.ParaView"}) == 0);
        if (cmd.isEmpty())
            say(tr("ParaView was not found (neither paraview on PATH nor its Flatpak)"), true);
        else if (QProcess::startDetached(cmd.first(), cmd.mid(1)))
            say(tr("Opened in ParaView: %1").arg(m_results.path()));
        else
            say(tr("ParaView did not start: %1").arg(cmd.join(' ')), true);
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
    m_results = ResultsFile();
    if (m_meshActor) {
        m_meshActor->UnRegister(nullptr);
        m_meshActor = nullptr;
    }
    m_layouts->setCurrentIndex(0);
    const QString earlier = latestResults();
    if (!earlier.isEmpty() && showResults(earlier))
        m_layouts->setCurrentIndex(0);  // the model first; the results are a tab away
    startMeshing();
    updateActions();
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
    m_meshPending = true;
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
    m_meshPending = false;
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
    m_meshActor = actor;
    actor->Register(nullptr);  // kept while the Results layer is shown instead
    showLayer(m_layouts->currentIndex());
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
    updateActions();
    emit meshReady();
}

void MainWindow::updateActions()
{
    const bool meshed = !m_mesh.points.empty();
    m_runAction->setEnabled(meshed && !m_runner.running() && !m_study.stepsOf("solve").empty());
    m_stopAction->setEnabled(m_runner.running());
    const bool results = m_results.timeCount() > 0;
    for (QAction *a : m_vcr)
        a->setEnabled(results);
    m_colourBy->setEnabled(results);
    m_rescale->setEnabled(results);
    m_openParaView->setEnabled(results);
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
    const core::ToolkitSet toolkits = core::findToolkits({GF_SHIPPED_TOOLKITS});
    QString why;
    const core::Toolkit *toolkit = core::readyToolkit(toolkits, "solve", step.parameters["kind"].toString("structural.static"), &why);
    if (!toolkit) {
        say(tr("Cannot run: %1").arg(why), true);
        return;
    }

    // The set-up filled from its cards; the job folder for this run.
    core::Setup setup = m_study.setup;
    if (!core::fillFromCards(setup, m_study.folder, &why)) {
        say(tr("Cannot run: %1").arg(why), true);
        return;
    }
    const QString folder = core::nextRunFolder(m_study, step.id);
    if (!core::writeSolveJob(folder, step.parameters["kind"].toString("structural.static"), setup,
                             m_study.setupJson, m_study.folder, m_mesh, &why)) {
        say(tr("Cannot run: %1").arg(why), true);
        return;
    }
    m_solveStep = step.id;
    m_toolkitName = toolkit->name;
    m_runNumber = QDir(folder).dirName().toInt();
    m_job->clear();
    m_job->appendPlainText(tr("Run %1 with %2, in %3").arg(m_runNumber).arg(toolkit->name, folder));
    if (auto *d = findChild<QDockWidget *>("jobDock"))
        d->raise();
    if (!m_runner.start(*toolkit, folder, &why)) {
        say(tr("Cannot run: %1").arg(why), true);
        return;
    }
    setJobState(tr("running (run %1)").arg(m_runNumber));
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
        m_job->appendPlainText(QString("%1%  %2").arg(100 * l.fraction, 3, 'f', 0).arg(l.text));
        m_progress->setValue(qRound(100 * l.fraction));
        statusBar()->showMessage(tr("%1: %2").arg(m_toolkitName, l.text));
        break;
    case Type::Metric:
        m_job->appendPlainText(QString("        %1: %2 (%3)").arg(l.name).arg(l.value).arg(l.text));
        break;
    case Type::Log:
        m_job->appendPlainText(QString("  %1: %2").arg(l.level, l.text));
        if (l.level == "warning" || l.level == "error")
            m_messages->appendPlainText(QString("%1 (%2): %3").arg(m_toolkitName, l.level, l.text));
        break;
    case Type::File:
        break;
    case Type::Other:
        m_job->appendPlainText("  " + l.text);
        break;
    }
}

void MainWindow::onJobFinished(int exitCode, const QString &results, bool)
{
    m_progress->setVisible(false);
    if (exitCode == 3) {
        setJobState(tr("cancelled (run %1)").arg(m_runNumber));
        say(tr("Run %1 cancelled; its files are kept").arg(m_runNumber));
    } else if (exitCode != 0 || results.isEmpty()) {
        setJobState(tr("failed (run %1)").arg(m_runNumber));
        say(tr("Run %1 failed (exit code %2): see the Job dock").arg(m_runNumber).arg(exitCode), true);
    } else {
        QFile report(QFileInfo(results).absoluteDir().filePath("results.json"));
        report.open(QIODevice::ReadOnly);
        const QJsonObject r = QJsonDocument::fromJson(report.readAll()).object();
        setJobState(tr("solved (run %1)").arg(m_runNumber));
        const QString detail = tr("run %1, %2 by %3").arg(m_runNumber).arg(r["trust"].toString(), r["solver"].toString());
        for (QTreeWidgetItem *i : m_tree->findItems(tr("Results"), Qt::MatchExactly)) {
            i->setText(1, detail);
            i->setToolTip(1, results);
        }
        say(tr("Run %1 solved: %2").arg(m_runNumber).arg(results));
        showResults(results);
    }
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
        say(tr("Cannot show the results: %1").arg(error), true);
        return false;
    }
    const QString run = QFileInfo(resultsFile).absoluteDir().absolutePath().section('/', -2, -2);
    QFile report(QFileInfo(resultsFile).absoluteDir().filePath("results.json"));
    report.open(QIODevice::ReadOnly);
    const QJsonObject r = QJsonDocument::fromJson(report.readAll()).object();
    for (QTreeWidgetItem *i : m_tree->findItems(tr("Results"), Qt::MatchExactly)) {
        i->setText(1, tr("run %1, %2 by %3").arg(run, r["trust"].toString(), r["solver"].toString()));
        i->setToolTip(1, resultsFile);
    }
    // The job that made them, and what the results say about themselves
    // (tenet 9: a value that cannot be, extrapolated by the solver).
    if (!m_runner.running()) {
        const std::vector<const core::StudyStep *> solves = m_study.stepsOf("solve");
        if (!solves.empty()) {
            m_solveStep = solves.front()->id;
            const core::ToolkitSet toolkits = core::findToolkits({GF_SHIPPED_TOOLKITS});
            const core::Toolkit *t = toolkits.find(solves.front()->by);
            m_toolkitName = t ? t->name : solves.front()->by;
            setJobState(tr("solved (run %1)").arg(run));
        }
    }
    for (const QJsonValue &note : r["notes"].toArray())
        say(tr("Note on run %1: %2").arg(run, note.toString()));
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
    return true;
}

void MainWindow::showIncrement(int k)
{
    if (m_results.timeCount() == 0)
        return;
    m_increment = std::clamp(k, 0, m_results.timeCount() - 1);
    const QString field = m_colourBy->currentText();
    vtkSmartPointer<vtkUnstructuredGrid> model = m_results.model(m_increment, field);
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
        m_scalarBar->SetPosition(0.86, 0.12);
        m_scalarBar->SetWidth(0.12);
        m_scalarBar->SetHeight(0.7);
    }
    m_resultsActor->SetMapper(mapper);
    m_scalarBar->SetLookupTable(colours);
    m_scalarBar->SetTitle(field.toUtf8().constData());
    m_increments->setText(tr(" increment %1 of %2, time %3 ")
                              .arg(m_increment + 1)
                              .arg(m_results.timeCount())
                              .arg(m_results.time(m_increment)));
    showLayer(m_layouts->currentIndex());
    updateActions();
}

void MainWindow::showLayer(int layout)
{
    m_renderer->RemoveAllViewProps();
    if (layout == 1 && m_resultsActor) {
        m_renderer->AddActor(m_resultsActor);
        m_renderer->AddActor2D(m_scalarBar);
    } else if (m_meshActor) {
        m_renderer->AddActor(m_meshActor);
    }
    m_view->renderWindow()->Render();
}

} // namespace gf::gui
