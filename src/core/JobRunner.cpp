#include "core/JobRunner.h"

#include <QDir>
#include <QJsonDocument>

namespace gf::core {

ProtocolLine readProtocolLine(const QByteArray &bytes)
{
    ProtocolLine p;
    p.text = QString::fromUtf8(bytes).trimmed();
    const QJsonDocument d = QJsonDocument::fromJson(bytes);
    if (!d.isObject())
        return p;
    const QJsonObject o = d.object();
    const QString type = o["type"].toString();
    if (type == "progress") {
        p.type = ProtocolLine::Type::Progress;
        p.fraction = o["fraction"].toDouble();
        p.text = o["text"].toString();
    } else if (type == "metric") {
        p.type = ProtocolLine::Type::Metric;
        p.name = o["name"].toString();
        p.value = o["value"].toDouble();
        p.text = o["text"].toString();
    } else if (type == "log") {
        p.type = ProtocolLine::Type::Log;
        p.level = o["level"].toString();
        p.text = o["text"].toString();
    } else if (type == "file") {
        p.type = ProtocolLine::Type::File;
        p.path = o["path"].toString();
        p.role = o["role"].toString();
        p.text.clear();
    }
    return p;
}

JobRunner::JobRunner(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &JobRunner::readOutput);
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        readOutput();
        if (!m_pending.trimmed().isEmpty())
            emit line(readProtocolLine(m_pending));
        m_pending.clear();
        m_kill.stop();
        const int exit = status == QProcess::NormalExit ? code : (m_stopping ? 3 : 1);
        emit finished(m_stopping && exit != 0 ? 3 : exit, exit == 0 ? m_results : QString(), m_stopping);
    });
    m_kill.setSingleShot(true);
    m_kill.setInterval(10000);
    connect(&m_kill, &QTimer::timeout, this, [this] {
        if (running())
            m_process.kill();
    });
}

JobRunner::~JobRunner()
{
    // A toolkit must not outlive what started it: asked, then made, to stop.
    if (running()) {
        m_process.terminate();
        if (!m_process.waitForFinished(10000))
            m_process.kill();
        m_process.waitForFinished(5000);
    }
}

bool JobRunner::start(const Toolkit &toolkit, const QString &jobFolder, QString *error)
{
    if (running()) {
        if (error)
            *error = "a job is already running";
        return false;
    }
    m_job = QDir(jobFolder).absolutePath();
    m_results.clear();
    m_pending.clear();
    m_stopping = false;
    const QStringList command = commandFor(toolkit, m_job);
    m_process.start(command.first(), command.mid(1));
    if (!m_process.waitForStarted(10000)) {
        if (error)
            *error = QString("%1 did not start: %2").arg(command.first(), m_process.errorString());
        return false;
    }
    if (error)
        error->clear();
    return true;
}

void JobRunner::stop()
{
    if (!running() || m_stopping)
        return;
    m_stopping = true;
    m_process.terminate();
    m_kill.start();
}

void JobRunner::readOutput()
{
    m_pending += m_process.readAllStandardOutput();
    qsizetype nl;
    while ((nl = m_pending.indexOf('\n')) >= 0) {
        const QByteArray bytes = m_pending.left(nl);
        m_pending.remove(0, nl + 1);
        if (bytes.trimmed().isEmpty())
            continue;
        const ProtocolLine p = readProtocolLine(bytes);
        if (p.type == ProtocolLine::Type::File && p.role.isEmpty())  // a deck, a log: not the results
            m_results = QDir(m_job).absoluteFilePath(p.path);
        emit line(p);
    }
}

} // namespace gf::core
