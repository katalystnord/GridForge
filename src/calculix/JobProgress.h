#pragma once

// A running ccx job's progress, from the two files ccx appends to as it
// goes: <job>.sta, a row per increment attempt, and <job>.cvg, a row per
// iteration. Pure functions over the bytes read so far: the watcher reads
// the files, these parse them. A last line not yet ended is not an error,
// it is left for the next read (`consumed` says where the complete lines
// end). Written from the files ccx 2.23 writes (fixture
// cantilever-cutbacks); the ccx manual does not specify them.

#include <QByteArray>
#include <QString>

#include <vector>

namespace gf::calculix {

// "     1          1     2U    4  0.000000E+00  0.000000E+00  0.250000E+00"
struct StaRow {
    int step = 0, increment = 0, attempt = 0;
    bool converged = true;  // false: ccx marked the attempt with "U" and cut back
    int iterations = 0;
    double totalTime = 0, stepTime = 0, incrementTime = 0;
};

// "     1     1     1     1        0  0.9104E+03  0.1000E+03  0.0000E+00  0.0000E+00"
struct CvgRow {
    int step = 0, increment = 0, attempt = 0, iteration = 0;
    int contactElements = 0;
    double forceResidual = 0;           // %
    double displacementCorrection = 0;  // %
    double fluxResidual = 0;            // %
    double temperatureCorrection = 0;   // %
};

struct StaFile {
    std::vector<StaRow> rows;
    qsizetype consumed = 0;  // bytes up to the end of the last complete line
    QString error;           // empty when read; otherwise what, on which line
    bool ok() const { return error.isEmpty(); }
};

struct CvgFile {
    std::vector<CvgRow> rows;
    qsizetype consumed = 0;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

StaFile parseSta(const QByteArray &data);
CvgFile parseCvg(const QByteArray &data);

} // namespace gf::calculix
