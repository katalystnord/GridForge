#include "calculix/JobProgress.h"

#include <QList>

namespace gf::calculix {

namespace {

// The complete lines read so far (the last one, if not ended, is left),
// and where they end.
QList<QByteArray> completeLines(const QByteArray &data, qsizetype *consumed)
{
    const qsizetype end = data.lastIndexOf('\n') + 1;
    *consumed = end;
    QList<QByteArray> lines = data.left(end).split('\n');
    if (!lines.isEmpty())
        lines.removeLast();  // the empty piece after the last line end
    for (QByteArray &l : lines)
        if (l.endsWith('\r'))
            l.chop(1);
    return lines;
}

bool whole(const QByteArray &t, int *out)
{
    bool ok = false;
    *out = t.toInt(&ok);
    return ok;
}

bool real(const QByteArray &t, double *out)
{
    bool ok = false;
    *out = t.toDouble(&ok);
    return ok;
}

// The summary line, then `headers` column-header lines, then rows of
// `columns` fields each, handed to `row`; false and *error if not so.
template <typename Row>
bool parse(const QByteArray &data, const QList<QByteArray> &summaries, int headers, int columns,
           const char *what, qsizetype *consumed, std::vector<Row> *rows, QString *error,
           bool (*row)(const QList<QByteArray> &, Row *))
{
    const QList<QByteArray> lines = completeLines(data, consumed);
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QByteArray t = lines[i].trimmed();
        const int lineNo = int(i) + 1;
        if (i == 0) {
            if (!summaries.contains(t)) {
                *error = QString("line 1: expected \"%1\", found \"%2\"")
                             .arg(QString::fromLatin1(summaries.first()), QString::fromLatin1(t.left(40)));
                return false;
            }
            continue;
        }
        if (i <= headers || t.isEmpty())
            continue;  // the column headers, as ccx writes them
        const QList<QByteArray> fields = t.simplified().split(' ');
        Row r;
        if (fields.size() != columns || !row(fields, &r)) {
            *error = QString("line %1: not a %2 row: \"%3\"")
                         .arg(lineNo)
                         .arg(what, QString::fromLatin1(t.left(80)));
            return false;
        }
        rows->push_back(r);
    }
    return true;
}

bool staRow(const QList<QByteArray> &f, StaRow *r)
{
    QByteArray attempt = f[2];
    r->converged = !attempt.endsWith('U');
    if (!r->converged)
        attempt.chop(1);
    return whole(f[0], &r->step) && whole(f[1], &r->increment) && whole(attempt, &r->attempt)
           && whole(f[3], &r->iterations) && real(f[4], &r->totalTime)
           && real(f[5], &r->stepTime) && real(f[6], &r->incrementTime);
}

bool cvgRow(const QList<QByteArray> &f, CvgRow *r)
{
    return whole(f[0], &r->step) && whole(f[1], &r->increment) && whole(f[2], &r->attempt)
           && whole(f[3], &r->iteration) && whole(f[4], &r->contactElements)
           && real(f[5], &r->forceResidual) && real(f[6], &r->displacementCorrection)
           && real(f[7], &r->fluxResidual) && real(f[8], &r->temperatureCorrection);
}

} // namespace

StaFile parseSta(const QByteArray &data)
{
    StaFile s;
    parse<StaRow>(data, {"SUMMARY OF JOB INFORMATION"}, 1, 7, ".sta", &s.consumed, &s.rows,
                  &s.error, staRow);
    return s;
}

CvgFile parseCvg(const QByteArray &data)
{
    CvgFile c;
    // ccx 2.23 spells it with a zero; the letter O is accepted too.
    parse<CvgRow>(data,
                  {"SUMMARY OF C0NVERGENCE INFORMATION", "SUMMARY OF CONVERGENCE INFORMATION"},
                  3, 9, ".cvg", &c.consumed, &c.rows, &c.error, cvgRow);
    return c;
}

} // namespace gf::calculix
