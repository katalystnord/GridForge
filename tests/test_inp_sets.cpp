// Named sets from a CalculiX deck (.inp): the .frd carries no set names,
// so the hand-off reads them from the deck that made it. The fixture
// bar-sets writes its sets in every way ccx 2.23 accepted (it ran).

#include "calculix/InpSets.h"

#include <cstdio>
#include <functional>
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

QString fixture(const char *rel) { return QString(GF_FIXTURES) + "/calculix/" + rel; }

const InpSets &bar()
{
    static const InpSets s = [] {
        InpSets r = readInpSets(fixture("bar-sets/bar.inp"));
        if (!r.ok())
            std::printf("  (%s)\n", qPrintable(r.error));
        return r;
    }();
    return s;
}

std::vector<int> ids(const InpSets &s, const char *name, bool nodes)
{
    const InpSet *set = s.find(name, nodes);
    return set ? set->ids : std::vector<int>{-1};
}

} // namespace

CASE(the_bars_sets_are_read_in_the_order_the_deck_names_them)
{
    const InpSets &s = bar();
    CHECK(s.ok());
    QStringList names;
    for (const InpSet &set : s.sets)
        names << QString(set.nodes ? "n:" : "e:") + set.name;
    CHECK(names == QStringList({"n:Nall", "e:Bar", "e:Odd", "e:Top", "n:Fixed", "n:Pulled",
                                "n:Ends", "n:X0", "n:Y0"}));
}

CASE(sets_on_the_node_and_element_keywords_collect_their_ids)
{
    std::vector<int> all;
    for (int n = 1; n <= 20; ++n)
        all.push_back(n);
    CHECK(ids(bar(), "Nall", true) == all);                                 // from the included file
    CHECK((ids(bar(), "Bar", false) == std::vector<int>{1, 2, 3, 4}));     // over two blocks
}

CASE(generate_lists_a_range_with_or_without_a_step)
{
    CHECK((ids(bar(), "Odd", false) == std::vector<int>{1, 3}));
    CHECK((ids(bar(), "Fixed", true) == std::vector<int>{1, 2, 3, 4}));
}

// C3D20 takes two lines per element; the second holds no element id.
CASE(an_element_over_two_lines_is_one_element)
{
    const InpSets s = readInpSets(fixture("cube-he20/cube.inp"));
    CHECK((ids(s, "EALL", false) == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}));
}

// Ends lists Fixed, Pulled and node 1 again: each id is kept once.
CASE(a_set_may_list_other_sets_and_continue_on_the_next_line)
{
    CHECK((ids(bar(), "Ends", true) == std::vector<int>{1, 2, 3, 4, 17, 18, 19, 20}));
    CHECK((ids(bar(), "Y0", true) == std::vector<int>{1, 2, 5, 6, 9, 10, 13, 14, 17, 18}));
}

// ccx matches names whatever their case: the deck's boundary on "x0" held
// the set X0 (the results equal bar-tension's).
CASE(names_match_whatever_their_case_and_keep_the_decks_spelling)
{
    const InpSet *s = bar().find("fixed", true);
    CHECK(s && s->name == "Fixed");
    CHECK(bar().find("BAR", false) && !bar().find("Bar", true));
}

CASE(the_files_read_are_named)
{
    const QStringList f = bar().files;
    CHECK(f.size() == 2 && f.value(0).endsWith("bar-sets/bar.inp")
          && f.value(1).endsWith("bar-sets/nodes.inp"));
}

CASE(a_set_naming_an_unknown_set_is_refused_with_its_line)
{
    const InpSets s = readInpSets(fixture("refused/unknown-set.inp"));
    CHECK(!s.ok());
    CHECK(s.error.contains("unknown-set.inp") && s.error.contains("line 8")
          && s.error.contains("Pushed"));
}

CASE(a_missing_include_is_refused_by_name)
{
    const InpSets s = readInpSets(fixture("refused/missing-include.inp"));
    CHECK(!s.ok());
    CHECK(s.error.contains("not-here.inp"));
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
