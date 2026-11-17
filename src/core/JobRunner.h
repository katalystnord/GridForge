#pragma once

// Runs a toolkit on a job folder and says what it does (docs/specs/
// toolkits-v1.md, sections 4 and 5): the command started as its own
// process, its standard output read as it comes, one JSON object per line,
// each turned into a signal. Never blocks: the window and the command line
// both drive it from their event loops. stop() asks the toolkit to stop
// (SIGTERM), and kills it 10 s later if it has not.

#include "core/Toolkit.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace gf::core {

// One line of the protocol, as a toolkit wrote it.
struct ProtocolLine {
    enum class Type { Progress, Metric, Log, File, Other } type = Type::Other;
    double fraction = 0;    // progress
    QString text;           // progress, metric (where), log; other: the line itself
    QString name;           // metric
    double value = 0;       // metric
    QString level;          // log: "info", "warning", "error"
    QString path;           // file, relative to the job folder
    QString role;           // file: none for the results; "deck" for the toolkit's input deck
};

// A line of standard output read: a JSON object of a known type, or kept
// as text (the spec: anything else is the log).
ProtocolLine readProtocolLine(const QByteArray &line);

class JobRunner : public QObject
{
    Q_OBJECT
public:
    explicit JobRunner(QObject *parent = nullptr);
    ~JobRunner() override;

    // Starts `toolkit` on `jobFolder`. False (and the reason) if it cannot
    // start; a toolkit already running is not started again.
    bool start(const Toolkit &toolkit, const QString &jobFolder, QString *error);
    void stop();
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    QString jobFolder() const { return m_job; }

signals:
    void line(const gf::core::ProtocolLine &line);
    // Exit code (0 solved, 1 failed, 2 not started, 3 cancelled), and the
    // results file the toolkit named (absolute), if any.
    void finished(int exitCode, const QString &results, bool stopped);

private:
    void readOutput();
    QProcess m_process;
    QByteArray m_pending;
    QString m_job, m_results;
    bool m_stopping = false;
    QTimer m_kill;
};

} // namespace gf::core
