#include "gui/Markers.h"

#include "core/LoadChoice.h"
#include "core/StudyEdit.h"
#include "core/StudyOutline.h"
#include "mesh/Surface.h"

#include <cmath>

namespace gf::gui {

namespace {

using Vec = std::array<double, 3>;

double dot(const Vec &a, const Vec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

Vec axis(int a, double sign)
{
    Vec v{0, 0, 0};
    v[a] = sign;
    return v;
}

// A cone for a held direction: into the solid where the direction crosses
// the faces, else along it.
Marker held(int a, const Vec &centre, const Vec &normal, const QString &key)
{
    const double across = dot(axis(a, 1), normal);
    return {Marker::Shape::Cone, "support", key, centre, axis(a, across > 1e-9 ? -1 : 1), false};
}

// An arrow along `d`: out of the faces from their centre when it points
// out of them, else ending on them.
Marker arrow(const QString &role, const Vec &d, const Vec &centre, const Vec &normal, const QString &key)
{
    return {Marker::Shape::Arrow, role, key, centre, d, dot(d, normal) >= -1e-9};
}

} // namespace

std::vector<Marker> markersFor(const core::Study &study, int step, vtkPolyData *surface)
{
    std::vector<Marker> out;
    if (!surface || step < 0 || step >= int(study.setup.steps.size()))
        return out;
    // What holds in the step, as the Steps dock shows it: set there or kept.
    for (const core::StepTable::Row &row : core::stepTable(study).rows) {
        if (step >= int(row.cells.size()) || row.cells[step].key.isEmpty())
            continue;
        const QString key = row.cells[step].key;
        const QJsonObject entry = core::itemEntry(study.setupJson, key);
        const core::Group *g = study.setup.group(entry["group"].toString());
        if (!g || g->faces.empty())
            continue;  // gravity, or a group of solids: nothing to draw on a face
        const Vec centre = mesh::centreOf(surface, g->faces);
        const Vec normal = mesh::meanNormal(surface, g->faces);
        const core::LoadChoice c = core::choiceOf(entry, key.startsWith("support:"));
        switch (c.kind) {
        case core::LoadChoice::Kind::Fixed:
            for (int a = 0; a < 3; ++a)
                out.push_back(held(a, centre, normal, key));
            break;
        case core::LoadChoice::Kind::Symmetry: {
            // Held across the faces: along the axis they face.
            int a = 0;
            for (int k = 1; k < 3; ++k)
                if (std::abs(normal[k]) > std::abs(normal[a]))
                    a = k;
            out.push_back(held(a, centre, normal, key));
            break;
        }
        case core::LoadChoice::Kind::Axes:
        case core::LoadChoice::Kind::Move: {
            // Each direction as given: held a cone, moved an arrow.
            const QJsonObject d = entry["displacement"].toObject();
            for (int a = 0; a < 3; ++a) {
                const QString name(QChar('x' + a));
                if (!d.contains(name))
                    continue;
                const double v = d[name].toDouble();
                if (v == 0)
                    out.push_back(held(a, centre, normal, key));
                else
                    out.push_back(arrow("move", axis(a, v > 0 ? 1 : -1), centre, normal, key));
            }
            break;
        }
        case core::LoadChoice::Kind::Force: {
            const QJsonObject f = entry["force"].toObject();
            Vec v{f["x"].toDouble(), f["y"].toDouble(), f["z"].toDouble()};
            const double size = std::sqrt(dot(v, v));
            if (size == 0)
                break;
            for (double &x : v)
                x /= size;
            out.push_back(arrow("force", v, centre, normal, key));
            break;
        }
        case core::LoadChoice::Kind::Pressure: {
            // Positive pushes into the faces.
            const double s = c.magnitude >= 0 ? -1 : 1;
            out.push_back(arrow("pressure", {s * normal[0], s * normal[1], s * normal[2]}, centre, normal, key));
            break;
        }
        }
    }
    return out;
}

} // namespace gf::gui
