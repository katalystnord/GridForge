// A study's set-up read from study.json (docs/specs/study-v1.md, section
// 4): the spec's own example read as written, then each fault refused,
// saying what and where.

#include "core/Setup.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

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

// The spec's example (section 4), with the material's values given in the
// study (section 4.8's test).
const char *example = R"({
  "units": "mm-N-s",
  "groups": {
    "Steel part": {"solids": [1]},
    "Back":   {"faces": [1]},
    "Side":   {"faces": [3]},
    "Bottom": {"faces": [5]},
    "Pulled": {"faces": [2]}
  },
  "materials": {
    "Steel": {"card": "inputs/Steel-Generic.FCMat",
              "values": {"YoungsModulus": 210000, "PoissonRatio": 0.3}}
  },
  "assign": {"Steel part": "Steel"},
  "supports": [
    {"group": "Back",   "symmetry": true},
    {"group": "Side",   "symmetry": true},
    {"group": "Bottom", "symmetry": true}
  ],
  "steps": [
    {"name": "Pull", "kind": "static",
     "loads": [{"group": "Pulled", "displacement": {"x": 0.01}}],
     "output": ["displacement", "reaction", "stress", "strain"]}
  ]
})";

QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }

// The example with one change: `path` (dot separated, [n] for arrays) set
// to `value` (JSON text), or removed when value is null.
QString errorWith(const QString &path, const char *value)
{
    QJsonObject root = json(example);
    std::function<QJsonValue(QJsonValue, QStringList)> set = [&](QJsonValue at, QStringList rest) -> QJsonValue {
        if (rest.isEmpty())
            return value ? QJsonDocument::fromJson(QByteArray("[") + value + "]").array().at(0) : QJsonValue();
        QString key = rest.takeFirst();
        int index = -1;
        if (key.contains('[')) {
            index = key.section('[', 1).chopped(1).toInt();
            key = key.section('[', 0, 0);
        }
        QJsonObject o = at.toObject();
        if (index >= 0) {
            QJsonArray a = o[key].toArray();
            a[index] = set(a[index], rest);
            o[key] = a;
        } else if (rest.isEmpty() && !value) {
            o.remove(key);
        } else {
            o[key] = set(o[key], rest);
        }
        return o;
    };
    root = set(root, path.split('.')).toObject();
    QString error;
    readSetup(root, &error);
    return error;
}

} // namespace

CASE(the_specs_example_is_read_as_written)
{
    QString error;
    const Setup s = readSetup(json(example), &error);
    CHECK(error.isEmpty());
    CHECK(s.units == Units::MillimetreNewtonSecond);
    CHECK(s.groups.size() == 5);
    const Group *part = s.group("Steel part"), *pulled = s.group("Pulled");
    CHECK(part && part->solids == std::vector<int>({1}) && part->faces.empty());
    CHECK(pulled && pulled->faces == std::vector<int>({2}));
    const Material *steel = s.material("Steel");
    CHECK(steel && steel->card == "inputs/Steel-Generic.FCMat" && steel->plastic.isEmpty());
    CHECK(steel && steel->values.at("YoungsModulus") == 210000 && steel->values.at("PoissonRatio") == 0.3);
    CHECK(s.assign.size() == 1 && s.assign.at("Steel part") == "Steel");
    CHECK(s.supports.size() == 3 && s.supports[2].group == "Bottom" && s.supports[2].symmetry);
    CHECK(s.steps.size() == 1);
    if (s.steps.empty())
        return;
    const AnalysisStep &pull = s.steps[0];
    CHECK(pull.name == "Pull" && pull.kind == "static" && !pull.nonlinearGeometry && !pull.replace);
    CHECK(pull.loads.size() == 1 && pull.loads[0].group == "Pulled");
    CHECK(pull.loads.size() == 1 && pull.loads[0].displacement.x == 0.01
          && !pull.loads[0].displacement.y && !pull.loads[0].displacement.z);
    CHECK(pull.output == QStringList({"displacement", "reaction", "stress", "strain"}));
}

CASE(si_units_are_read)
{
    QString error;
    QJsonObject o = json(example);
    o["units"] = "m-N-s";
    CHECK(readSetup(o, &error).units == Units::MetreNewtonSecond && error.isEmpty());
}

CASE(supports_and_step_details_are_read)
{
    QJsonObject o = json(example);
    o["supports"] = QJsonDocument::fromJson(R"([{"group": "Back", "fixed": true},
        {"group": "Side", "displacement": {"y": 0, "z": -0.5}}])").array();
    QJsonArray steps = o["steps"].toArray();
    QJsonObject step = steps[0].toObject();
    step["nonlinear geometry"] = false;
    step["replace"] = true;
    step["raw"] = QJsonArray{"*CONTROLS, PARAMETERS=TIME INCREMENTATION", "4, 8, 9, 200, 10, 4, 0, 10"};
    step["supports"] = QJsonDocument::fromJson(R"([{"group": "Bottom", "fixed": true}])").array();
    steps[0] = step;
    o["steps"] = steps;
    QString error;
    const Setup s = readSetup(o, &error);
    CHECK(error.isEmpty());
    CHECK(s.supports.size() == 2 && s.supports[0].fixed);
    CHECK(s.supports.size() == 2 && !s.supports[1].fixed && !s.supports[1].symmetry
          && !s.supports[1].displacement.x && s.supports[1].displacement.y == 0.0
          && s.supports[1].displacement.z == -0.5);
    CHECK(!s.steps.empty() && s.steps[0].nonlinearGeometry == false && s.steps[0].replace);
    CHECK(!s.steps.empty() && s.steps[0].raw.size() == 2);
    CHECK(!s.steps.empty() && s.steps[0].supports.size() == 1 && s.steps[0].supports[0].group == "Bottom");
}

CASE(each_fault_is_refused_saying_what_and_where)
{
    CHECK(errorWith("units", "\"in-lbf-s\"") == "units: \"in-lbf-s\" is not one of mm-N-s, m-N-s");
    CHECK(errorWith("units", nullptr) == "units: missing; mm-N-s or m-N-s");
    CHECK(errorWith("groups.Back", "{}") == "groups.Back: names no faces or solids");
    CHECK(errorWith("groups.Back", "{\"faces\": [0]}") == "groups.Back: face 0; faces are numbered from 1");
    CHECK(errorWith("assign.Steel part", "\"Brass\"") == "assign: no material \"Brass\"");
    CHECK(errorWith("assign.Pulled", "\"Steel\"") == "assign: group \"Pulled\" holds no solids");
    CHECK(errorWith("assign.Ghost", "\"Steel\"") == "assign: no group \"Ghost\"");
    CHECK(errorWith("supports[0].group", "\"Top\"") == "supports[0]: no group \"Top\"");
    CHECK(errorWith("supports[1].symmetry", nullptr)
          == "supports[1]: says neither fixed, symmetry nor a displacement");
    CHECK(errorWith("steps[0].kind", "\"modal\"") == "steps[0]: kind \"modal\" is not written yet; static is");
    CHECK(errorWith("steps[0].loads[0].group", "\"Top\"") == "steps[0].loads[0]: no group \"Top\"");
    CHECK(errorWith("steps[0].loads[0].displacement", nullptr)
          == "steps[0].loads[0]: says none of displacement, force, pressure or gravity");
    CHECK(errorWith("steps[0].loads[0].pressure", "5")
          == "steps[0].loads[0]: says more than one of displacement, force, pressure or gravity");
    CHECK(errorWith("steps[0].loads[0]", "{\"group\": \"Steel part\", \"pressure\": 5}")
          == "steps[0].loads[0]: a pressure acts on faces; group \"Steel part\" has none");
    CHECK(errorWith("steps[0].loads[0]", "{\"group\": \"Pulled\", \"gravity\": {\"z\": -9810}}")
          == "steps[0].loads[0]: gravity acts on solids; group \"Pulled\" has none");
    CHECK(errorWith("steps[0].loads[0]", "{\"pressure\": 5}") == "steps[0].loads[0]: no group");
    CHECK(errorWith("steps[0].loads[0].displacement", "{\"w\": 1}")
          == "steps[0].loads[0].displacement: \"w\" is not x, y or z");
    CHECK(errorWith("materials.Steel.values", "{\"YoungsModulus\": \"stiff\"}")
          == "materials.Steel.values.YoungsModulus: not a number");
    CHECK(errorWith("steps", "[]") == "steps: none; a study needs at least one");
}

CASE(forces_pressure_and_gravity_are_read)
{
    QJsonObject o = json(example);
    QJsonArray steps = o["steps"].toArray();
    QJsonObject st = steps[0].toObject();
    st["loads"] = QJsonDocument::fromJson(R"([
        {"group": "Pulled", "force": {"x": 126000}},
        {"group": "Pulled", "pressure": -210},
        {"gravity": {"z": -9810}},
        {"group": "Steel part", "gravity": {"y": 1}}])").array();
    steps[0] = st;
    o["steps"] = steps;
    QString error;
    const Setup s = readSetup(o, &error);
    CHECK(error.isEmpty());
    if (s.steps.empty() || s.steps[0].loads.size() != 4)
        return;
    const auto &l = s.steps[0].loads;
    CHECK(l[0].kind == Load::Kind::Force && l[0].group == "Pulled" && l[0].vector.x == 126000.0
          && !l[0].vector.y);
    CHECK(l[1].kind == Load::Kind::Pressure && l[1].pressure == -210);
    CHECK(l[2].kind == Load::Kind::Gravity && l[2].group.isEmpty() && l[2].vector.z == -9810.0);
    CHECK(l[3].kind == Load::Kind::Gravity && l[3].group == "Steel part");
}

CASE(a_solid_given_two_materials_is_refused)
{
    QJsonObject o = json(example);
    QJsonObject groups = o["groups"].toObject();
    groups["Also steel"] = QJsonObject{{"solids", QJsonArray{1}}};
    o["groups"] = groups;
    QJsonObject assign = o["assign"].toObject();
    assign["Also steel"] = "Steel";
    o["assign"] = assign;
    QString error;
    readSetup(o, &error);
    CHECK(error == "assign: solid 1 is in \"Also steel\" and \"Steel part\", both given a material");
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
