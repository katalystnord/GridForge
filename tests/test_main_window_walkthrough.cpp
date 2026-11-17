// The window, walked through as a user would (layout A of
// docs/survey/gui-layouts.md): the dogbone study opened, the tree, the
// properties, the mesh made in the background into the view. On a private
// X server, never the desktop (TenSense's ensureHeadless, below).
//
//   test_main_window_walkthrough [--screenshot FILE]   also saves the window

#include "gui/MainWindow.h"
#include "gui/Style.h"
#include "gui/Tone.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QTextBlock>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor2DCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>

#if !defined(GF_EXAMPLES) || !defined(GF_FIXTURES) || !defined(GF_ADAPTER_DIR)
#error "GF_EXAMPLES and GF_FIXTURES must be defined"
#endif

using gf::gui::MainWindow;

namespace {
QString screenshot;  // --screenshot FILE

// The box study copied into `dir` (runs land in the copy), its path.
QString boxStudy(QTemporaryDir &dir)
{
    const QString from = QString(GF_FIXTURES) + "/studies/box-tension", to = QDir(dir.path()).filePath("box-tension");
    QDir().mkpath(to);
    for (const char *f : {"study.json", "box.step"})
        QFile::copy(from + "/" + f, to + "/" + f);
    return to;
}

// ccx stood in for by fixtures/fake-ccx, replaying ccx 2.23's own files for
// box-setup (which has this study's mesh); slow, to be cancelled.
void stand_in_ccx(bool slow)
{
    qputenv("GF_CCX", QByteArray(GF_FIXTURES) + "/fake-ccx/ccx");
    qputenv("FAKE_CCX_FROM", QByteArray(GF_FIXTURES) + "/calculix/box-setup");
    if (slow)
        qputenv("FAKE_CCX_SLOW", "1");
    else
        qunsetenv("FAKE_CCX_SLOW");
}

QStringList topLevel(QTreeWidget *t)
{
    QStringList out;
    for (int i = 0; i < t->topLevelItemCount(); ++i)
        out << t->topLevelItem(i)->text(0);
    return out;
}

QTreeWidgetItem *findItem(QTreeWidget *t, const QString &title)
{
    const auto found = t->findItems(title, Qt::MatchExactly | Qt::MatchRecursive);
    return found.isEmpty() ? nullptr : found.first();
}

QString propertyValue(QWidget *properties, const QString &label)
{
    auto *form = qobject_cast<QFormLayout *>(properties->layout());
    for (int r = 0; form && r < form->rowCount(); ++r) {
        auto *l = qobject_cast<QLabel *>(form->itemAt(r, QFormLayout::LabelRole)->widget());
        auto *v = qobject_cast<QLabel *>(form->itemAt(r, QFormLayout::FieldRole)->widget());
        if (l && v && l->text() == label)
            return v->text();
    }
    return {};
}
} // namespace

class MainWindowWalkthrough : public QObject
{
    Q_OBJECT
private slots:
    void the_window_has_layout_a()
    {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (const char *name : {"studyDock", "propertiesDock", "informationDock", "messagesDock", "jobDock",
                                 "stepsDock", "deckDock"})
            QVERIFY2(w.findChild<QDockWidget *>(name), name);
        // Study over Properties on the left; Properties and Information tabbed.
        auto *study = w.findChild<QDockWidget *>("studyDock");
        auto *properties = w.findChild<QDockWidget *>("propertiesDock");
        QCOMPARE(w.dockWidgetArea(study), Qt::LeftDockWidgetArea);
        QCOMPARE(w.dockWidgetArea(properties), Qt::LeftDockWidgetArea);
        QVERIFY(study->geometry().bottom() <= properties->geometry().top() + 5);
        QVERIFY(w.tabifiedDockWidgets(properties).contains(w.findChild<QDockWidget *>("informationDock")));
        QCOMPARE(w.dockWidgetArea(w.findChild<QDockWidget *>("jobDock")), Qt::BottomDockWidgetArea);
        for (const char *bar : {"mainToolbar", "runToolbar", "cameraToolbar", "vcrToolbar", "stepToolbar"})
            QVERIFY2(w.findChild<QToolBar *>(bar), bar);
        // ParaView's icons are there (the resource is in the binary).
        auto *reset = w.findChild<QAction *>("resetCamera");
        QVERIFY(reset && !reset->icon().isNull());
        QVERIFY(!reset->icon().pixmap(24, 24).isNull());
    }

    void gridforge_is_told_from_paraview_at_a_glance()
    {
        // David, 2026-11-17: ParaView's layout, GridForge's own colours.
        MainWindow w;
        w.show();
        auto *banner = w.findChild<QLabel *>("nextStep");
        QVERIFY(banner && banner->isVisible());
        QCOMPARE(banner->text(), QString("Open a study: File, Open Study (a folder with a study.json)."));
        QCOMPARE(banner->palette().color(QPalette::Window), QColor(226, 92, 29));  // ember
        QCOMPARE(w.palette().color(QPalette::Highlight), QColor(226, 92, 29));
        auto *view = w.findChild<QVTKOpenGLNativeWidget *>("view");
        vtkRenderer *r = view->renderWindow()->GetRenderers()->GetFirstRenderer();
        QVERIFY(r->GetGradientBackground());
        double bottom[3];
        r->GetBackground(bottom);
        QVERIFY(bottom[0] < 0.15 && bottom[2] < 0.15);  // graphite, not ParaView's (0.32, 0.34, 0.43)
    }

    void the_banner_follows_the_study_through_a_solve()
    {
        stand_in_ccx(false);
        MainWindow w;
        w.show();
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        auto *banner = w.findChild<QLabel *>("nextStep");
        QCOMPARE(banner->text(), QString("Meshing box.step ... the window stays yours meanwhile."));
        QTRY_COMPARE_WITH_TIMEOUT(banner->text(), QString("Mesh ready. Press Run (F5) to solve with CalculiX."), 60000);
        w.findChild<QAction *>("run")->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(
            banner->text(), QString("Run 1 solved. Step through the increments in Results, or Open in ParaView."), 60000);
    }

    void the_selected_tab_has_an_ember_line_under_it()
    {
        // Drawn by GridForge's proxy over Fusion (gui/Style), no style sheet.
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *tabs = w.findChild<QTabBar *>("layouts");
        tabs->setCurrentIndex(1);
        const QImage image = tabs->grab().toImage();
        const QRect selected = tabs->tabRect(1), other = tabs->tabRect(0);
        const int dpr = qRound(image.devicePixelRatio());
        auto pixel = [&](int x, int y) { return QColor(image.pixel(x * dpr, y * dpr)); };
        QCOMPARE(pixel(selected.center().x(), selected.bottom() - 1), gf::gui::ember);
        QVERIFY(pixel(other.center().x(), other.bottom() - 1) != gf::gui::ember);
    }

    void opening_the_dogbone_fills_the_tree_and_the_units()
    {
        MainWindow w;
        w.show();
        QVERIFY(w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic"));
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QCOMPARE(topLevel(tree), QStringList({"Geometry", "Mesh", "Materials", "Groups", "Supports", "Steps", "Jobs",
                                              "Results"}));
        QTreeWidgetItem *load = findItem(tree, "Displacement: Pulled grip");
        QVERIFY(load);
        QCOMPARE(load->text(1), QString("x 1.5, y 0, z 0 mm"));
        QVERIFY(!load->icon(0).isNull());  // FreeCAD's FEM icon
        QCOMPARE(w.findChild<QLabel *>("units")->text(), QString("mm, N, s, MPa"));
        QCOMPARE(w.findChild<QComboBox *>("stepChooser")->currentText(), QString("Pull"));
        QVERIFY(w.windowTitle().contains("dogbone-plastic"));
    }

    void selecting_an_item_shows_it_in_properties()
    {
        MainWindow w;
        w.show();
        w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic");
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        tree->setCurrentItem(findItem(tree, "Measured steel"));
        auto *properties = w.findChild<QWidget *>("properties");
        QCOMPARE(propertyValue(properties, "Name"), QString("Measured steel"));
        QCOMPARE(propertyValue(properties, "Value"), QString("card tensense-ro.FCMat, plastic"));
    }

    void the_mesh_is_made_without_freezing_the_window_and_shown()
    {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        // While it meshes, the window's own event loop keeps running: a
        // timer set before meshing fires during it (tenet 7).
        int ticks = 0;
        QTimer tick;
        connect(&tick, &QTimer::timeout, [&] { ++ticks; });
        tick.start(20);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic"));
        QVERIFY(w.meshing());
        QTRY_VERIFY_WITH_TIMEOUT(!w.meshing(), 120000);
        QVERIFY2(ticks > 5, qPrintable(QString("only %1 ticks in %2 ms").arg(ticks).arg(clock.elapsed())));
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QTreeWidgetItem *mesh = findItem(tree, "Mesh");
        QVERIFY(mesh);
        QCOMPARE(mesh->text(1), QString("1624 tetrahedra, 3575 nodes; ready for a solver"));
        auto *view = w.findChild<QVTKOpenGLNativeWidget *>("view");
        vtkRenderer *r = view->renderWindow()->GetRenderers()->GetFirstRenderer();
        QCOMPARE(r->GetActors()->GetNumberOfItems(), 1);
        QVERIFY(w.findChild<QLabel *>("information")->text().startsWith("ready for a solver"));
        if (!screenshot.isEmpty()) {
            w.resize(1500, 950);
            QTest::qWait(500);
            view->renderWindow()->Render();
            QVERIFY(w.grab().save(screenshot));
        }
    }

    void run_waits_for_the_mesh()
    {
        MainWindow w;
        w.show();
        auto *run = w.findChild<QAction *>("run");
        QVERIFY(run && !run->isEnabled());
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        QVERIFY(!run->isEnabled());  // meshing
        QTRY_VERIFY_WITH_TIMEOUT(!w.meshing(), 60000);
        QTRY_VERIFY(run->isEnabled());
    }

    void a_solve_runs_from_the_window_and_says_its_progress()
    {
        stand_in_ccx(false);
        MainWindow w;
        w.show();
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QAction *>("run")->isEnabled(), 60000);
        w.findChild<QAction *>("run")->trigger();
        auto *job = w.findChild<QPlainTextEdit *>("job");
        QTRY_VERIFY_WITH_TIMEOUT(job->toPlainText().contains("100%  step 1, increment 1"), 60000);
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QTRY_COMPARE_WITH_TIMEOUT(findItem(tree, "solve")->text(1), QString("CalculiX, solved (run 1)"), 60000);
        QCOMPARE(findItem(tree, "Results")->text(1), QString("run 1, solved by CalculiX Version 2.23"));
        QVERIFY(QFileInfo(QDir(dir.path()).filePath("box-tension/steps/solve/runs/1/results/results.vtkhdf")).isFile());
        QVERIFY(w.findChild<QAction *>("run")->isEnabled() && !w.findChild<QAction *>("stop")->isEnabled());
        // A second run keeps the first (every run's outputs kept).
        w.findChild<QAction *>("run")->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(findItem(tree, "solve")->text(1), QString("CalculiX, solved (run 2)"), 60000);
        QVERIFY(QDir(QDir(dir.path()).filePath("box-tension/steps/solve/runs/1")).exists());
    }

    void the_results_are_shown_after_a_solve_and_on_reopening()
    {
        stand_in_ccx(false);
        QTemporaryDir dir;
        {
            MainWindow w;
            w.show();
            QVERIFY(w.openStudy(boxStudy(dir)));
            QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QAction *>("run")->isEnabled(), 60000);
            QVERIFY(!w.findChild<QAction *>("lastIncrement")->isEnabled());  // no results yet
            w.findChild<QAction *>("run")->trigger();
            auto *tree = w.findChild<QTreeWidget *>("studyTree");
            QTRY_COMPARE_WITH_TIMEOUT(findItem(tree, "solve")->text(1), QString("CalculiX, solved (run 1)"), 60000);
            QCOMPARE(w.findChild<QTabBar *>("layouts")->currentIndex(), 1);  // Results
            auto *colourBy = w.findChild<QComboBox *>("colourBy");
            QCOMPARE(colourBy->currentText(), QString("von Mises stress (derived)"));
            QVERIFY(colourBy->findText("DISP (magnitude)") >= 0);
            QCOMPARE(w.findChild<QLabel *>("incrementLabel")->text().trimmed(), QString("increment 1 of 1, time 1"));
            QVERIFY(w.findChild<QAction *>("lastIncrement")->isEnabled());
            QVERIFY(w.findChild<QAction *>("openInParaView")->isEnabled());
            auto *view = w.findChild<QVTKOpenGLNativeWidget *>("view");
            vtkRenderer *r = view->renderWindow()->GetRenderers()->GetFirstRenderer();
            QCOMPARE(r->GetActors()->GetNumberOfItems(), 1);
            QCOMPARE(r->GetActors2D()->GetNumberOfItems(), 1);  // the legend
            colourBy->setCurrentText("DISP (magnitude)");
            // The Model tab shows the mesh again, without the legend.
            w.findChild<QTabBar *>("layouts")->setCurrentIndex(0);
            QCOMPARE(r->GetActors2D()->GetNumberOfItems(), 0);
            if (!screenshot.isEmpty()) {
                w.findChild<QTabBar *>("layouts")->setCurrentIndex(1);
                colourBy->setCurrentText("von Mises stress (derived)");
                w.resize(1500, 950);
                QTest::qWait(500);
                view->renderWindow()->Render();
                w.grab().save(screenshot.chopped(4) + "-results.png");
            }
        }
        MainWindow again;
        again.show();
        QVERIFY(again.openStudy(QDir(dir.path()).filePath("box-tension")));
        auto *tree = again.findChild<QTreeWidget *>("studyTree");
        QCOMPARE(findItem(tree, "Results")->text(1), QString("run 1, solved by CalculiX Version 2.23"));
        QCOMPARE(findItem(tree, "solve")->text(1), QString("CalculiX, solved (run 1)"));
        QCOMPARE(again.findChild<QTabBar *>("layouts")->currentIndex(), 0);  // the model first
        QVERIFY(again.findChild<QAction *>("lastIncrement")->isEnabled());
    }

    void the_results_notes_are_said_when_they_load()
    {
        // results.json's notes (a plastic strain below zero, extrapolated by
        // ccx) are what the window must not hide (tenet 9).
        stand_in_ccx(false);
        QTemporaryDir dir;
        const QString study = boxStudy(dir);
        {
            MainWindow w;
            w.show();
            QVERIFY(w.openStudy(study));
            QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QAction *>("run")->isEnabled(), 60000);
            w.findChild<QAction *>("run")->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(findItem(w.findChild<QTreeWidget *>("studyTree"), "solve")->text(1),
                                      QString("CalculiX, solved (run 1)"), 60000);
        }
        const QString report = QDir(study).filePath("steps/solve/runs/1/results/results.json");
        QFile f(report);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        o["notes"] = QJsonArray{"PE below 0 at 3 node values in 1 of 1 time points (least -2.7e-05): ccx "
                                "extrapolates it from the integration points to the nodes"};
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(o).toJson());
        f.close();
        MainWindow w;
        w.show();
        QVERIFY(w.openStudy(study));
        QVERIFY(w.findChild<QPlainTextEdit *>("messages")->toPlainText().contains(
            "Note on run 1: PE below 0 at 3 node values"));
    }

    void paraview_is_found_on_path_or_as_its_flatpak()
    {
        QCOMPARE(MainWindow::paraviewCommand("/r/results.vtkhdf", true, true),
                 QStringList({"paraview", "/r/results.vtkhdf"}));
        QCOMPARE(MainWindow::paraviewCommand("/r/results.vtkhdf", false, true),
                 QStringList({"flatpak", "run", "--filesystem=/r", "org.paraview.ParaView", "/r/results.vtkhdf"}));
        QVERIFY(MainWindow::paraviewCommand("/r/results.vtkhdf", false, false).isEmpty());
    }

    void stop_cancels_a_running_solve()
    {
        stand_in_ccx(true);
        MainWindow w;
        w.show();
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QAction *>("run")->isEnabled(), 60000);
        w.findChild<QAction *>("run")->trigger();
        auto *job = w.findChild<QPlainTextEdit *>("job");
        QTRY_VERIFY_WITH_TIMEOUT(job->toPlainText().contains("step 1, increment 1"), 30000);
        auto *stop = w.findChild<QAction *>("stop");
        QVERIFY(stop->isEnabled());
        stop->trigger();
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QTRY_COMPARE_WITH_TIMEOUT(findItem(tree, "solve")->text(1), QString("CalculiX, cancelled (run 1)"), 15000);
        QVERIFY(job->toPlainText().contains("cancelled: ccx stopped after step 1, increment 1"));
        stand_in_ccx(false);
    }

    void no_ccx_is_said_and_nothing_runs()
    {
        qputenv("GF_CCX", "/no/such/ccx");
        MainWindow w;
        w.show();
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QAction *>("run")->isEnabled(), 60000);
        w.findChild<QAction *>("run")->trigger();
        QVERIFY(w.findChild<QPlainTextEdit *>("messages")->toPlainText().contains(
            "no solve toolkit for structural.static is ready; CalculiX: missing ccx"));
        QVERIFY(!QDir(QDir(dir.path()).filePath("box-tension/steps")).exists());
        stand_in_ccx(false);
    }

    void messages_are_coloured_by_what_they_mean()
    {
        MainWindow w;
        w.show();
        QTemporaryDir empty;
        w.openStudy(empty.path());  // a problem
        QTemporaryDir dir;
        QVERIFY(w.openStudy(boxStudy(dir)));
        QTRY_VERIFY_WITH_TIMEOUT(!w.meshing(), 60000);
        auto *messages = w.findChild<QPlainTextEdit *>("messages");
        auto colourOf = [&](const QString &start) {
            for (QTextBlock b = messages->document()->begin(); b.isValid(); b = b.next())
                if (b.text().startsWith(start) && b.begin() != b.end())
                    return b.begin().fragment().charFormat().foreground().color();
            return QColor();
        };
        QCOMPARE(colourOf("Problem: Cannot open the study"), gf::gui::toneColour(gf::gui::Tone::Problem));
        QCOMPARE(colourOf("Meshed: "), gf::gui::toneColour(gf::gui::Tone::Done));
        QCOMPARE(colourOf("Opened "), w.palette().color(QPalette::Text));
    }

    void a_study_that_cannot_be_read_is_said_in_messages()
    {
        MainWindow w;
        w.show();
        QTemporaryDir empty;
        QVERIFY(!w.openStudy(empty.path()));
        QVERIFY(w.findChild<QPlainTextEdit *>("messages")->toPlainText().contains(
            "Cannot open the study: no study.json in " + empty.path()));
        QVERIFY(w.statusBar()->currentMessage().startsWith("Cannot open the study"));
    }
};

// HEADLESS BY CONSTRUCTION (TenSense's lesson): the test shows real windows,
// so it puts itself on a private X server unless asked to use the desktop.
static void ensureHeadless(char *argv[])
{
    if (qEnvironmentVariableIsSet("GF_WALKTHROUGH_XVFB") || qEnvironmentVariable("GF_WALKTHROUGH_ON_DESKTOP") == "1")
        return;
    qputenv("GF_WALKTHROUGH_XVFB", "1");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
    qunsetenv("WAYLAND_DISPLAY");
    std::vector<char *> args{const_cast<char *>("xvfb-run"), const_cast<char *>("-a"), const_cast<char *>("-s"),
                             const_cast<char *>("-screen 0 1700x1100x24")};
    for (char **a = argv; *a; ++a)
        args.push_back(*a);
    args.push_back(nullptr);
    execvp("xvfb-run", args.data());
    std::perror("walkthrough: cannot start xvfb-run (install xvfb, or set GF_WALKTHROUGH_ON_DESKTOP=1)");
    std::exit(2);
}

int main(int argc, char *argv[])
{
    ensureHeadless(argv);
    std::vector<char *> rest;
    for (int i = 0; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--screenshot" && i + 1 < argc) {
            screenshot = QString::fromLocal8Bit(argv[++i]);
            continue;
        }
        rest.push_back(argv[i]);
    }
    int n = static_cast<int>(rest.size());
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(n, rest.data());
    gf::gui::applyStyle(app);
    Q_INIT_RESOURCE(icons);
    // The CalculiX adapter, as the window finds it beside itself when
    // installed; here it is in the build's src/.
    qputenv("PATH", QByteArray(GF_ADAPTER_DIR) + ":" + qgetenv("PATH"));
    MainWindowWalkthrough test;
    return QTest::qExec(&test, n, rest.data());
}

#include "test_main_window_walkthrough.moc"
