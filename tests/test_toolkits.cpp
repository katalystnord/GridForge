// Toolkits (docs/specs/toolkits-v1.md): every toolkit, the shipped CalculiX
// and OpenFOAM ones included, is a folder with a toolkit.json, reached
// through the same door. These cases read the manifests, say which toolkit
// can do what a study needs and why another cannot, and refuse a broken
// manifest with its reason.

#include "core/Toolkit.h"

#include <QCoreApplication>
#include <QDir>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace fs::core;

#ifndef FS_FIXTURES
#error "FS_FIXTURES must point at tests/fixtures"
#endif
#ifndef FS_SHIPPED_TOOLKITS
#error "FS_SHIPPED_TOOLKITS must point at toolkits/"
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

QString fixture(const char *rel) { return QString(FS_FIXTURES) + "/toolkits/" + rel; }

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
    const ToolkitSet set = findToolkits({FS_SHIPPED_TOOLKITS});
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

CASE(a_toolkit_is_offered_only_for_what_it_declares)
{
    const ToolkitSet set = findToolkits({FS_SHIPPED_TOOLKITS});
    const auto offers = set.offers("solve", "structural.static", {"plasticity"});
    const Offer *ccx = offerFor(offers, "calculix");
    const Offer *of = offerFor(offers, "openfoam");
    CHECK(ccx && ccx->can);
    CHECK(of && !of->can && of->why.contains("structural.static"));
}

CASE(a_missing_feature_is_named)
{
    const ToolkitSet set = findToolkits({FS_SHIPPED_TOOLKITS, fixture("elastic-only/..")});
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
    CHECK(a.missing.contains("no-such-program-fs-test"));
    // Its own program, in its folder, is found there.
    CHECK(!a.missing.contains("run.sh"));
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
