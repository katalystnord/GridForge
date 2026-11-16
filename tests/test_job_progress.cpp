// A running ccx job's progress, from the files ccx writes as it goes: the
// .sta (a row per increment attempt) and the .cvg (a row per iteration).
// Read while ccx still writes them, so a line not yet finished is left
// for the next read. Fixture cantilever-cutbacks (its README has the
// answers, read by hand).

#include "calculix/JobProgress.h"

#include <QFile>

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <vector>

using namespace gf::calculix;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
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

QByteArray bytes(const char *rel)
{
    QFile f(QString(GF_FIXTURES) + "/calculix/" + rel);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

const StaFile &sta()
{
    static const StaFile s = [] {
        StaFile r = parseSta(bytes("cantilever-cutbacks/cantilever.sta"));
        if (!r.ok())
            std::printf("  (%s)\n", qPrintable(r.error));
        return r;
    }();
    return s;
}

const CvgFile &cvg()
{
    static const CvgFile c = [] {
        CvgFile r = parseCvg(bytes("cantilever-cutbacks/cantilever.cvg"));
        if (!r.ok())
            std::printf("  (%s)\n", qPrintable(r.error));
        return r;
    }();
    return c;
}

} // namespace

CASE(every_attempt_is_a_row_and_failed_ones_are_marked)
{
    CHECK(sta().ok() && sta().rows.size() == 17);
    std::vector<std::pair<int, int>> failed;
    for (const StaRow &r : sta().rows)
        if (!r.converged)
            failed.push_back({r.increment, r.attempt});
    CHECK((failed == std::vector<std::pair<int, int>>{{1, 1}, {1, 2}, {5, 1}, {9, 1}}));
    if (sta().rows.size() > 2) {
        const StaRow &r = sta().rows[2];  // "1 1 3 4 0.625000E-01 0.625000E-01 0.625000E-01"
        CHECK(r.step == 1 && r.increment == 1 && r.attempt == 3 && r.converged
              && r.iterations == 4 && r.totalTime == 0.0625 && r.stepTime == 0.0625
              && r.incrementTime == 0.0625);
    }
}

CASE(the_converged_increments_fill_the_step)
{
    double sum = 0;
    int converged = 0;
    for (const StaRow &r : sta().rows)
        if (r.converged) {
            sum += r.incrementTime;
            ++converged;
        }
    CHECK(converged == 13);
    CHECK(std::abs(sum - 1.0) < 1e-5);  // six digits as written
    CHECK(!sta().rows.empty() && sta().rows.back().stepTime == 1.0);
}

CASE(every_iteration_is_a_row_with_its_residuals)
{
    CHECK(cvg().ok() && cvg().rows.size() == 64);
    if (!cvg().rows.empty()) {
        const CvgRow &r = cvg().rows[0];  // "1 1 1 1 0 0.9104E+03 0.1000E+03 0 0"
        CHECK(r.step == 1 && r.increment == 1 && r.attempt == 1 && r.iteration == 1
              && r.contactElements == 0 && r.forceResidual == 910.4
              && r.displacementCorrection == 100.0 && r.fluxResidual == 0
              && r.temperatureCorrection == 0);
    }
}

// The two files agree: each attempt's iterations in the .cvg number what
// its row in the .sta says.
CASE(the_cvg_iterations_match_the_sta_counts)
{
    std::map<std::tuple<int, int, int>, int> counted;
    for (const CvgRow &r : cvg().rows)
        ++counted[{r.step, r.increment, r.attempt}];
    CHECK(counted.size() == sta().rows.size());
    for (const StaRow &r : sta().rows)
        CHECK((counted[{r.step, r.increment, r.attempt}] == r.iterations));
}

// ccx appends while it runs: a read may end inside a line. That line is
// left out, and `consumed` says how far the complete lines go.
CASE(a_line_ccx_is_still_writing_is_left_for_the_next_read)
{
    const QByteArray all = bytes("cantilever-cutbacks/cantilever.sta");
    const qsizetype line9 = all.indexOf("     1          5");  // increment 5, attempt 1U
    const QByteArray partial = all.left(all.indexOf('\n', line9) - 20);
    const StaFile s = parseSta(partial);
    CHECK(s.ok() && s.rows.size() == 6);
    CHECK(s.consumed == line9);
    const StaFile empty = parseSta(QByteArray());
    CHECK(empty.ok() && empty.rows.empty() && empty.consumed == 0);
    const StaFile header = parseSta(all.left(20));  // inside the first header line
    CHECK(header.ok() && header.rows.empty());
}

CASE(a_line_that_is_no_row_is_refused_with_its_line)
{
    QByteArray broken = bytes("cantilever-cutbacks/cantilever.sta");
    broken.replace("     1          4     1", "     1          x     1");
    const StaFile s = parseSta(broken);
    CHECK(!s.ok() && s.error.contains("line 8"));
    // Its first line gone, the .cvg starts with a column header, not the summary line.
    const QByteArray noHeader = bytes("cantilever-cutbacks/cantilever.cvg").mid(36);
    CHECK(!parseCvg(noHeader).ok());
}

int main()
{
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d checks failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
