// Toolkits (docs/specs/toolkits-v1.md): every toolkit, the shipped CalculiX
// and OpenFOAM ones included, is a folder with a toolkit.json, reached
// through the same door. These cases read the manifests, say which toolkit
// can do what a study needs and why another cannot, and refuse a broken
// manifest with its reason.

#include "core/JobRunner.h"
#include "core/Toolkit.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace gf::core;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif
#ifndef GF_SHIPPED_TOOLKITS
#error "GF_SHIPPED_TOOLKITS must point at toolkits/"
#endif

namespace {

int failures = 0;
std::vector<std::pair<const char *, std::function<void()>>> &cases()
{
    static std::vector<std::pair<const char *, std::function<void()>>> c;
    return c;
}
struct Register {
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define CASE(name)                                  \
    void name();                                    \
    static Register reg_##name(#name, name);        \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

QString fixture(const char *rel) { return QString(GF_FIXTURES) + "/toolkits/" + rel; }

const Offer *offerFor(const std::vector<Offer> &offers, const QString &id)
{
    for (const auto &o : offers)
        if (o.toolkit->id == id)
            return &o;
    return nullptr;
}

} // namespace

CASE(the_shipped_calculix_and_openfoam_manifests_are_read)
{
    const ToolkitSet set = findToolkits({GF_SHIPPED_TOOLKITS});
    CHECK(set.problems.isEmpty());
    const Toolkit *ccx = set.find("calculix");
    const Toolkit *of = set.find("openfoam");
    CHECK(ccx && ccx->name == "CalculiX");
    CHECK(of && of->name == QString::fromUtf8("OpenFOAM®"));
    // OpenCFD's Disclaimer travels with the toolkit, to be shown with it.
    CHECK(of && of->notices.size() == 2
          && of->notices[0].startsWith("This offering is not approved or endorsed by OpenCFD"));
    CHECK(of && of->stages.size() == 2);
}

CASE(the_shipped_toolkits_are_found_beside_the_program_first)
{
    // Installed (the AppImage): <bin>/../share/gridforge/toolkits, and only
    // that: the source tree a build was made from is on the building machine
    // alone, and a toolkit there would replace the installed one of its
    // name (found running the AppImage, 2026-11-18). Else the source tree.
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("usr/bin");
    QDir(dir.path()).mkpath("usr/share/gridforge/toolkits");
    QDir(dir.path()).mkpath("source/toolkits");
    const QString bin = dir.filePath("usr/bin"), source = dir.filePath("source/toolkits");
    CHECK((shippedToolkitFolders(bin, source) == QStringList{QDir::cleanPath(dir.filePath("usr/share/gridforge/toolkits"))}));
    CHECK((shippedToolkitFolders(dir.filePath("elsewhere"), source) == QStringList{source}));
    CHECK(shippedToolkitFolders(dir.filePath("elsewhere"), dir.filePath("gone")).isEmpty());
}

CASE(a_toolkit_is_offered_only_for_what_it_declares)
{
    const ToolkitSet set = findToolkits({GF_SHIPPED_TOOLKITS});
    const auto offers = set.offers("solve", "structural.static", {"plasticity"});
    const Offer *ccx = offerFor(offers, "calculix");
    const Offer *of = offerFor(offers, "openfoam");
    CHECK(ccx && ccx->can);
    CHECK(of && !of->can && of->why.contains("structural.static"));
}

CASE(a_missing_feature_is_named)
{
    const ToolkitSet set = findToolkits({GF_SHIPPED_TOOLKITS, fixture("elastic-only/..")});
    const auto offers = set.offers("solve", "structural.static", {"plasticity"});
    const Offer *e = offerFor(offers, "elastic-only");
    CHECK(e && !e->can && e->why.contains("plasticity"));
}

CASE(a_surrogate_says_it_is_one)
{
    const ToolkitSet set = findToolkits({fixture("ann-surrogate/..")});
    const auto offers = set.offers("solve", "structural.static", {"plasticity"});
    const Offer *ann = offerFor(offers, "ann-surrogate");
    CHECK(ann && ann->can && ann->trust == "surrogate");
}

CASE(a_broken_manifest_is_refused_with_its_reason)
{
    const ToolkitSet set = findToolkits({fixture("")});
    for (const char *id : {"no-command", "bad-stage", "no-job", "bad-trust"})
        CHECK(set.find(id) == nullptr);
    const QString all = set.problems.join("\n");
    CHECK(all.contains("no-command") && all.contains("no command"));
    CHECK(all.contains("bad-stage") && all.contains("\"simulate\""));
    CHECK(all.contains("no-job") && all.contains("{job}"));
    CHECK(all.contains("bad-trust") && all.contains("\"guessed\""));
    // The good ones in the same folder are still read.
    CHECK(set.find("elastic-only") != nullptr);
}

CASE(a_later_folder_overrides_an_earlier_one_and_says_so)
{
    const ToolkitSet set = findToolkits({fixture("override-a"), fixture("override-b")});
    const Toolkit *t = set.find("calculix");
    CHECK(t && t->command.value(0) == "second");
    CHECK(set.notes.join("\n").contains("calculix"));
}

CASE(a_toolkit_whose_programs_are_missing_says_which)
{
    const ToolkitSet set = findToolkits({fixture("needs-missing/..")});
    const Toolkit *t = set.find("needs-missing");
    CHECK(t != nullptr);
    if (!t)
        return;
    const Availability a = availability(*t);
    CHECK(!a.ready);
    CHECK(a.missing.contains("no-such-program-gf-test"));
    // Its own program, in its folder, is found there.
    CHECK(!a.missing.contains("run.sh"));
}

CASE(a_required_program_may_be_named_by_the_environment)
{
    // GF_<PROGRAM> names where a required program is (toolkits spec,
    // section 3): ccx from FreeCAD's Flatpak, say, through a wrapper.
    const ToolkitSet set = findToolkits({fixture("needs-missing/..")});
    const Toolkit *t = set.find("needs-missing");
    CHECK(t);
    if (!t)
        return;
    qputenv("GF_NO_SUCH_PROGRAM_GF_TEST", QDir(fixture("needs-missing")).absoluteFilePath("run.sh").toLocal8Bit());
    CHECK(availability(*t).ready);
    qputenv("GF_NO_SUCH_PROGRAM_GF_TEST", "/no/such/file");
    CHECK(!availability(*t).ready);  // named, but not there: still missing
    qunsetenv("GF_NO_SUCH_PROGRAM_GF_TEST");
    CHECK(!availability(*t).ready);
}

CASE(the_protocol_lines_are_read_by_type)
{
    // toolkits spec, section 5: one JSON object per line; anything else is
    // the log, kept as text.
    ProtocolLine p = readProtocolLine(R"({"type": "progress", "fraction": 0.42, "text": "step 1, increment 7"})");
    CHECK(p.type == ProtocolLine::Type::Progress && p.fraction == 0.42 && p.text == "step 1, increment 7");
    p = readProtocolLine(R"({"type": "metric", "name": "force residual", "value": 3.1e-5, "text": "iteration 2"})");
    CHECK(p.type == ProtocolLine::Type::Metric && p.name == "force residual" && p.value == 3.1e-5
          && p.text == "iteration 2");
    p = readProtocolLine(R"({"type": "log", "level": "warning", "text": "PE below 0"})");
    CHECK(p.type == ProtocolLine::Type::Log && p.level == "warning" && p.text == "PE below 0");
    p = readProtocolLine(R"({"type": "file", "path": "results/results.vtkhdf"})");
    CHECK(p.type == ProtocolLine::Type::File && p.path == "results/results.vtkhdf" && p.role.isEmpty());
    // A file with a role: the toolkit's deck, shown, not taken for results.
    p = readProtocolLine(R"({"type": "file", "path": "work/job.inp", "role": "deck"})");
    CHECK(p.type == ProtocolLine::Type::File && p.path == "work/job.inp" && p.role == "deck");
    p = readProtocolLine("ccx printed this");
    CHECK(p.type == ProtocolLine::Type::Other && p.text == "ccx printed this");
    p = readProtocolLine(R"({"type": "something new"})");
    CHECK(p.type == ProtocolLine::Type::Other && p.text == R"({"type": "something new"})");
}

CASE(the_command_names_the_job_folder)
{
    const ToolkitSet set = findToolkits({fixture("needs-missing/..")});
    const Toolkit *t = set.find("needs-missing");
    CHECK(t != nullptr);
    if (!t)
        return;
    const QStringList argv = commandFor(*t, "/tmp/job 1");
    CHECK(argv.size() == 2);
    CHECK(argv.value(0) == QDir(fixture("needs-missing")).absoluteFilePath("run.sh"));
    CHECK(argv.value(1) == "/tmp/job 1");
}

CASE(a_deck_said_after_the_results_is_not_taken_for_them)
{
    // A toolkit may say its files in any order; only a file without a role
    // is the results (toolkits spec, section 5).
    Toolkit t;
    t.command = {"/bin/sh", "-c",
                 R"(echo '{"type": "file", "path": "results/r.vtkhdf"}'; )"
                 R"(echo '{"type": "file", "path": "work/job.inp", "role": "deck"}')",
                 "{job}"};
    JobRunner runner;
    QString results;
    bool done = false;
    QObject::connect(&runner, &JobRunner::finished, [&](int, const QString &r, bool) {
        results = r;
        done = true;
    });
    QString error;
    CHECK(runner.start(t, QDir::tempPath(), &error));
    for (int i = 0; i < 500 && !done; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    CHECK(done && results == QDir(QDir::tempPath()).absoluteFilePath("results/r.vtkhdf"));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
