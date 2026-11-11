// SPDX-License-Identifier: GPL-3.0-or-later
#include "Esptool.hpp"

#include <QRegularExpression>
#include <QStandardPaths>

namespace Esptool
{

namespace
{
QString hex(quint32 v)
{
    return QStringLiteral("0x%1").arg(v, 0, 16);
}

QStringList common(const QString& port, int baud)
{
    return { QStringLiteral("--chip"),
        QStringLiteral("esp32"),
        QStringLiteral("--port"),
        port,
        QStringLiteral("--baud"),
        QString::number(baud) };
}
} // namespace

QStringList chipInfoArgs(const QString& port, int baud)
{
    return common(port, baud) << QStringLiteral("flash-id");
}

QStringList backupArgs(const QString& port, int baud, const QString& outFile, quint64 flashSize)
{
    return common(port, baud) << QStringLiteral("read-flash") << hex(0) << hex(quint32(flashSize)) << outFile;
}

QStringList flashArgs(const QString& port, int baud, const QList<QPair<quint32, QString>>& images)
{
    QStringList a = common(port, baud);
    a << QStringLiteral("write-flash") << QStringLiteral("--flash-mode") << QStringLiteral("keep")
      << QStringLiteral("--flash-freq") << QStringLiteral("keep") << QStringLiteral("--flash-size")
      << QStringLiteral("keep");
    for (const auto& img : images)
    {
        a << hex(img.first) << img.second;
    }

    return a;
}

QStringList restoreArgs(const QString& port, int baud, const QString& backupFile)
{
    return common(port, baud) << QStringLiteral("write-flash") << hex(0) << backupFile;
}

QString stripAnsi(const QString& text)
{
    static const QRegularExpression ansi(QStringLiteral("\\x1B\\[[0-9;?]*[A-Za-z]"));
    QString s = text;
    s.remove(ansi);
    return s;
}

std::optional<double> parseProgress(const QString& line)
{
    static const QRegularExpression pct(
        QStringLiteral("(?:Writing at|Reading from|Read|Wrote)[^%]*?(\\d{1,3}(?:\\.\\d+)?)\\s*%"));
    const auto m = pct.match(stripAnsi(line));
    if (!m.hasMatch())
    {
        return std::nullopt;
    }

    const double v = m.captured(1).toDouble();
    if (v < 0 || v > 100)
    {
        return std::nullopt;
    }

    return v;
}

ChipInfo parseChipInfo(const QString& output)
{
    ChipInfo info;
    static const QRegularExpression chipV5(QStringLiteral("Chip type:\\s*(.+)"));
    static const QRegularExpression chipV4(QStringLiteral("Chip is (.+)"));
    static const QRegularExpression mac(QStringLiteral("MAC:\\s*([0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5})"));
    static const QRegularExpression flash(QStringLiteral("Detected flash size:\\s*(\\S+)"));
    const QString text = stripAnsi(output);
    auto m = chipV5.match(text);
    if (!m.hasMatch())
    {
        m = chipV4.match(text);
    }

    if (m.hasMatch())
    {
        info.chip = m.captured(1).trimmed();
    }

    m = mac.match(text);
    if (m.hasMatch())
    {
        info.mac = m.captured(1).toLower();
    }

    m = flash.match(text);
    if (m.hasMatch())
    {
        info.flashSize = m.captured(1).trimmed();
    }

    return info;
}

QString parseFatalError(const QString& output)
{
    const QString text = stripAnsi(output);
    const QString marker = QStringLiteral("A fatal error occurred:");
    const qsizetype i = text.lastIndexOf(marker);
    if (i < 0)
    {
        return QString();
    }

    return text.mid(i + marker.size()).section(QLatin1Char('\n'), 0, 0).trimmed();
}

QPair<QString, QStringList> command()
{
    const QByteArray env = qgetenv("TUBECLOCK_ESPTOOL");
    if (!env.isEmpty())
    {
        QStringList parts = QString::fromLocal8Bit(env).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString program = parts.takeFirst();
        return { program, parts };
    }

    const QString exe = QStandardPaths::findExecutable(QStringLiteral("esptool"));
    if (!exe.isEmpty())
    {
        return { exe, {} };
    }

    return { QStringLiteral("python3"), { QStringLiteral("-m"), QStringLiteral("esptool") } };
}

} // namespace Esptool

EsptoolRunner::EsptoolRunner(QObject* parent)
  : QObject(parent)
{
}

EsptoolRunner::~EsptoolRunner()
{
    if (process_ != nullptr && process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(2000);
    }
}

bool EsptoolRunner::isRunning() const
{
    return process_ != nullptr && process_->state() != QProcess::NotRunning;
}

void EsptoolRunner::start(const QStringList& args)
{
    if (isRunning())
    {
        return;
    }

    output_.clear();
    pending_.clear();
    delete process_;
    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    process_->setProcessEnvironment(env);
    connect(process_, &QProcess::readyRead, this, &EsptoolRunner::onReadyRead);
    connect(process_, &QProcess::finished, this, &EsptoolRunner::onFinished);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
        {
            Q_EMIT finished(false, QStringLiteral("esptool could not be started. Is it installed?"));
        }
    });
    const auto cmd = Esptool::command();
    process_->start(cmd.first, cmd.second + args);
}

void EsptoolRunner::cancel()
{
    if (isRunning())
    {
        process_->kill();
    }
}

void EsptoolRunner::onReadyRead()
{
    const QString chunk = QString::fromLocal8Bit(process_->readAll());
    output_ += chunk;
    pending_ += chunk;
    // esptool updates progress with '\r', normal output with '\n'.
    static const QRegularExpression sep(QStringLiteral("[\\r\\n]"));
    qsizetype idx;
    while ((idx = pending_.indexOf(sep)) >= 0)
    {
        const QString line = Esptool::stripAnsi(pending_.left(idx)).trimmed();
        pending_.remove(0, idx + 1);
        if (line.isEmpty())
        {
            continue;
        }

        if (auto p = Esptool::parseProgress(line))
        {
            Q_EMIT progress(*p);
        }
        else
        {
            Q_EMIT lineReceived(line);
        }
    }
}

void EsptoolRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    onReadyRead();
    if (status == QProcess::CrashExit)
    {
        Q_EMIT finished(false, QStringLiteral("esptool was stopped."));
        return;
    }

    if (exitCode != 0)
    {
        QString err = Esptool::parseFatalError(output_);
        if (err.isEmpty())
        {
            err = QStringLiteral("esptool failed (exit code %1).").arg(exitCode);
        }

        Q_EMIT finished(false, err);
        return;
    }

    Q_EMIT finished(true, QString());
}
