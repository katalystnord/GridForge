#include "core/MaterialCard.h"

#include <QDir>
#include <QFileInfo>

#include <yaml-cpp/yaml.h>

#include <set>

namespace gf::core {

namespace {

const std::set<std::string> elasticModels = {
    "f6f9e48c-b116-4e82-ad7f-3659a9219c50",  // IsotropicLinearElastic
    "7b561d1d-fb9b-44f6-9da9-56a4f74d7536",  // LinearElastic
};
const std::string densityModel = "454661e5-265b-4320-8e6f-fcf6223ac3af";

// Each unit's factor to MPa (pressures) or to kg/m^3 (densities).
const std::map<QString, double> pressure = {{"Pa", 1e-6}, {"kPa", 1e-3}, {"MPa", 1},
                                            {"GPa", 1e3}, {"N/mm^2", 1}, {"N/m^2", 1e-6}};
const std::map<QString, double> density = {{"kg/m^3", 1},    {"g/cm^3", 1e3}, {"t/m^3", 1e3},
                                           {"kg/mm^3", 1e9}, {"g/mm^3", 1e6}, {"t/mm^3", 1e12}};

struct Fault {
    QString text;
};

// "200000 MPa" in the study's units; `kind` is the property's name.
double convert(const QString &text, const QString &property, Units units)
{
    const QString trimmed = text.trimmed();
    const int space = trimmed.indexOf(' ');
    const QString number = space < 0 ? trimmed : trimmed.left(space);
    const QString unit = space < 0 ? QString() : trimmed.mid(space + 1).trimmed();
    bool ok = false;
    const double value = number.toDouble(&ok);
    const QString where = QString("%1 \"%2\"").arg(property, text);
    if (!ok)
        throw Fault{where + ": not a number"};
    if (property == "PoissonRatio") {
        if (!unit.isEmpty())
            throw Fault{where + ": Poisson's ratio has no unit"};
        return value;
    }
    const bool isPressure = property == "YoungsModulus";
    const auto &table = isPressure ? pressure : density;
    const auto it = table.find(unit);
    if (it == table.end())
        throw Fault{QString("%1: %2 is not a unit GridForge converts (%3)")
                        .arg(where, unit.isEmpty() ? QString("no unit") : unit,
                             isPressure ? "Pa, kPa, MPa, GPa, N/mm^2, N/m^2"
                                        : "kg/m^3, g/cm^3, t/m^3, kg/mm^3, g/mm^3, t/mm^3")};
    const double base = value * it->second;  // MPa, or kg/m^3
    if (units == Units::MillimetreNewtonSecond)
        return isPressure ? base : base * 1e-12;  // t/mm^3
    return isPressure ? base * 1e6 : base;        // Pa, kg/m^3
}

} // namespace

MaterialCard readMaterialCard(const QString &path, Units units)
{
    MaterialCard card;
    const QString file = QFileInfo(path).fileName();
    if (!QFileInfo(path).isFile()) {
        card.error = "cannot read " + path;
        return card;
    }
    try {
        const YAML::Node root = YAML::LoadFile(path.toStdString());
        if (!root.IsMap() || !root["Models"].IsMap())
            throw Fault{"no Models"};
        if (root["General"] && root["General"]["Name"])
            card.name = QString::fromStdString(root["General"]["Name"].as<std::string>());
        for (const auto &entry : root["Models"]) {
            const YAML::Node model = entry.second;
            if (!model.IsMap() || !model["UUID"])
                continue;
            const std::string uuid = model["UUID"].as<std::string>();
            QStringList properties;
            if (elasticModels.count(uuid))
                properties = {"YoungsModulus", "PoissonRatio"};
            else if (uuid == densityModel)
                properties = {"Density"};
            for (const QString &property : properties) {
                const YAML::Node v = model[property.toStdString()];
                if (!v)
                    continue;
                const QString text = QString::fromStdString(v.as<std::string>());
                card.values[property] = {convert(text, property, units), text};
            }
        }
    } catch (const Fault &f) {
        card.values.clear();
        card.error = QString("%1: %2").arg(file, f.text);
    } catch (const YAML::Exception &e) {
        card.values.clear();
        card.error = QString("%1: not a material card: %2").arg(file, QString::fromStdString(e.what()));
    }
    return card;
}

bool fillFromCards(Setup &setup, const QString &studyDir, QString *error)
{
    for (Material &m : setup.materials) {
        for (const auto &[key, value] : m.values)
            m.source.emplace(key, "entered in the study");  // a job's set-up already says
        if (m.card.isEmpty())
            continue;
        const MaterialCard card = readMaterialCard(QDir(studyDir).filePath(m.card), setup.units);
        if (!card.ok()) {
            if (error)
                *error = QString("material %1: %2").arg(m.name, card.error);
            return false;
        }
        for (const auto &[key, value] : card.values)
            if (!m.values.count(key)) {
                m.values[key] = value.value;
                m.source[key] = QString("%1 in %2").arg(value.asWritten, QFileInfo(m.card).fileName());
            }
    }
    if (error)
        error->clear();
    return true;
}

} // namespace gf::core
