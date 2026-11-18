#include "mesh/GroupsFile.h"

#include "mesh/Surface.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <set>

namespace gf::mesh {

namespace {

std::array<double, 3> point(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    return {a.at(0).toDouble(), a.at(1).toDouble(), a.at(2).toDouble()};
}

QString numbers(const std::vector<int> &n)
{
    QStringList s;
    for (int i : n)
        s << QString::number(i);
    if (s.size() < 2)
        return s.join("");
    return s.mid(0, s.size() - 1).join(", ") + " and " + s.last();
}

} // namespace

std::vector<int> facesMatching(const gf::core::FaceSignature &signature,
                               const std::vector<gf::core::FaceSignature> &faces)
{
    std::vector<int> out;
    for (size_t i = 0; i < faces.size(); ++i)
        if (gf::core::sameFace(signature, faces[i]))
            out.push_back(static_cast<int>(i + 1));
    return out;
}

GroupsFile readGroupsFile(const QString &path, const QString &geometry, const gf::core::ResultsSeries &mesh,
                          gf::core::Units units)
{
    GroupsFile out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = "cannot read " + path;
        return out;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o["format"].toString() != "gridforge-groups") {
        out.error = "not a GridForge group file (its format is not gridforge-groups)";
        return out;
    }
    if (o["geometry"].toString() != geometry) {
        out.error = QString("the groups are for %1, not %2").arg(o["geometry"].toString(), geometry);
        return out;
    }
    const QString length = units == gf::core::Units::MetreNewtonSecond ? "m" : "mm";
    if (o["units"].toString() != length) {
        out.error = QString("the groups are in %1, the study in %2").arg(o["units"].toString(), length);
        return out;
    }

    // GridForge's own faces and solids, as its mesher measured them.
    std::vector<gf::core::FaceSignature> faces;
    while (const auto s = faceSignature(mesh, static_cast<int>(faces.size()) + 1))
        faces.push_back(*s);
    std::vector<gf::core::SolidSignature> solids;
    while (const auto s = solidSignature(mesh, static_cast<int>(solids.size()) + 1))
        solids.push_back(*s);

    const QJsonObject groups = o["groups"].toObject();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QJsonObject g = it.value().toObject();
        gf::core::Group group{it.key(), {}, {}, {}};
        QString why;
        std::set<int> faceSet, solidSet;
        for (const QJsonValue &v : g["faces"].toArray()) {
            const QJsonObject e = v.toObject();
            const gf::core::FaceSignature s{e["kind"].toString(), e["area"].toDouble(), point(e["centre"])};
            const std::vector<int> found = facesMatching(s, faces);
            const QString said = QString("FreeCAD's %1 (%2)").arg(e["freecad"].toString(), gf::core::describeFace(s, length));
            if (found.empty())
                why = QString("%1 is not a face of %2").arg(said, geometry);
            else if (found.size() > 1)
                why = QString("%1 matches faces %2 of %3").arg(said, numbers(found), geometry);
            if (!why.isEmpty())
                break;
            faceSet.insert(found.front());
            group.signatures[found.front()] = faces[found.front() - 1];
        }
        for (const QJsonValue &v : g["solids"].toArray()) {
            if (!why.isEmpty())
                break;
            const QJsonObject e = v.toObject();
            const gf::core::SolidSignature s{e["volume"].toDouble(), point(e["centre"])};
            std::vector<int> found;
            for (size_t i = 0; i < solids.size(); ++i)
                if (gf::core::sameSolid(s, solids[i]))
                    found.push_back(static_cast<int>(i + 1));
            const QString said = QString("FreeCAD's %1 (%2 %3%4 at (%5, %6, %7))")
                                     .arg(e["freecad"].toString(), QString::number(s.volume, 'g', 6), length, "3")
                                     .arg(s.centre[0], 0, 'g', 6)
                                     .arg(s.centre[1], 0, 'g', 6)
                                     .arg(s.centre[2], 0, 'g', 6);
            if (found.empty())
                why = QString("%1 is not a solid of %2").arg(said, geometry);
            else if (found.size() > 1)
                why = QString("%1 matches solids %2 of %3").arg(said, numbers(found), geometry);
            else
                solidSet.insert(found.front());
        }
        if (!why.isEmpty()) {
            out.problems << QString("group \"%1\" not made: %2; pick the group's faces in GridForge").arg(it.key(), why);
            continue;
        }
        group.faces.assign(faceSet.begin(), faceSet.end());
        group.solids.assign(solidSet.begin(), solidSet.end());
        if (group.faces.empty() && group.solids.empty()) {
            out.problems << QString("group \"%1\" not made: it names no faces or solids").arg(it.key());
            continue;
        }
        out.groups.push_back(group);
    }
    return out;
}

} // namespace gf::mesh
