// FreeCAD material cards read into a study's units (study spec, 4.3),
// against cards with known values (fixtures/materials/README.md): one of
// FreeCAD 1.1.4's own, one TenSense wrote, two by hand.

#include "core/MaterialCard.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

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

QString card(const char *file) { return QString(GF_FIXTURES) + "/materials/" + file; }

bool near(double a, double b) { return std::abs(a - b) <= 1e-12 * std::abs(b); }

double value(const MaterialCard &c, const char *key)
{
    const auto it = c.values.find(key);
    return it == c.values.end() ? NAN : it->second.value;
}

} // namespace

CASE(freecads_own_card_is_read_in_millimetres)
{
    const MaterialCard c = readMaterialCard(card("Steel-Generic.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(c.ok());
    CHECK(c.name == "Steel-Generic");
    CHECK(near(value(c, "YoungsModulus"), 200000));
    CHECK(near(value(c, "PoissonRatio"), 0.3));
    CHECK(near(value(c, "Density"), 7.9e-9));  // 7900 kg/m^3 in t/mm^3
    CHECK(c.values.count("YoungsModulus") && c.values.at("YoungsModulus").asWritten == "200000 MPa");
    CHECK(c.values.size() == 3);  // its thermal and other models are not read
}

CASE(freecads_own_card_is_read_in_si)
{
    const MaterialCard c = readMaterialCard(card("Steel-Generic.FCMat"), Units::MetreNewtonSecond);
    CHECK(near(value(c, "YoungsModulus"), 2e11));
    CHECK(near(value(c, "Density"), 7900));
}

CASE(a_card_tensense_wrote_is_read_from_the_other_elastic_model)
{
    const MaterialCard c = readMaterialCard(card("tensense-ro.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(c.ok());
    CHECK(near(value(c, "YoungsModulus"), 199900));  // 199.9 GPa
    CHECK(!c.values.count("PoissonRatio") && !c.values.count("Density"));
}

CASE(other_units_are_converted)
{
    const MaterialCard mm = readMaterialCard(card("hand-units.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(near(value(mm, "YoungsModulus"), 70000));
    CHECK(near(value(mm, "PoissonRatio"), 0.33));
    CHECK(near(value(mm, "Density"), 2.7e-9));
    const MaterialCard si = readMaterialCard(card("hand-units.FCMat"), Units::MetreNewtonSecond);
    CHECK(near(value(si, "YoungsModulus"), 7e10));
    CHECK(near(value(si, "Density"), 2700));
}

CASE(a_unit_not_known_is_refused_not_guessed)
{
    const MaterialCard c = readMaterialCard(card("odd-unit.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(c.error == "odd-unit.FCMat: YoungsModulus \"29000000 psi\": psi is not a unit GridForge converts "
                     "(Pa, kPa, MPa, GPa, N/mm^2, N/m^2)");
}

CASE(a_poissons_ratio_with_a_unit_is_refused)
{
    const MaterialCard c = readMaterialCard(card("poisson-unit.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(c.error == "poisson-unit.FCMat: PoissonRatio \"0.3 MPa\": Poisson's ratio has no unit");
}

CASE(a_file_that_is_not_yaml_is_refused)
{
    const MaterialCard c = readMaterialCard(card("broken.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(c.error.startsWith("broken.FCMat: not a material card: "));
    const MaterialCard missing = readMaterialCard(card("no-such.FCMat"), Units::MillimetreNewtonSecond);
    CHECK(missing.error == "cannot read " + card("no-such.FCMat"));
}

CASE(the_study_fills_from_cards_and_says_where_each_value_came_from)
{
    QString error;
    Setup s = readSetup(QJsonDocument::fromJson(R"({
        "units": "mm-N-s",
        "groups": {"Part": {"solids": [1]}},
        "materials": {"Steel": {"card": "materials/Steel-Generic.FCMat",
                                "values": {"YoungsModulus": 210000}}},
        "assign": {"Part": "Steel"},
        "steps": [{"name": "S", "kind": "static"}]})").object(), &error);
    CHECK(error.isEmpty());
    CHECK(fillFromCards(s, GF_FIXTURES, &error) && error.isEmpty());
    const Material *m = s.material("Steel");
    CHECK(m && m->values.at("YoungsModulus") == 210000);  // the study's own value wins
    CHECK(m && near(m->values.at("PoissonRatio"), 0.3) && near(m->values.at("Density"), 7.9e-9));
    CHECK(m && m->source.at("YoungsModulus") == "entered in the study");
    CHECK(m && m->source.at("PoissonRatio") == "0.3 in Steel-Generic.FCMat");
    CHECK(m && m->source.at("Density") == "7900 kg/m^3 in Steel-Generic.FCMat");
}

CASE(a_card_that_cannot_be_read_stops_the_study)
{
    QString error;
    Setup s = readSetup(QJsonDocument::fromJson(R"({
        "units": "mm-N-s",
        "groups": {"Part": {"solids": [1]}},
        "materials": {"Odd": {"card": "materials/odd-unit.FCMat"}},
        "assign": {"Part": "Odd"},
        "steps": [{"name": "S", "kind": "static"}]})").object(), &error);
    CHECK(!fillFromCards(s, GF_FIXTURES, &error));
    CHECK(error.startsWith("material Odd: odd-unit.FCMat: YoungsModulus"));
}

int main()
{
    for (auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
