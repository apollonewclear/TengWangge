#include "core/log_file_sink.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace tw {
namespace {

constexpr QFileDevice::Permissions kPrivateFilePermissions =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner;

QString formatDiskLine(const LogEntry &entry)
{
    const QString source = entry.toolId.isEmpty() ? QStringLiteral("platform") : entry.toolId;
    return QStringLiteral("[%1][%2] [%3] %4\n")
        .arg(LogBuffer::levelName(entry.level),
             entry.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             source,
             entry.message);
}

} // namespace

LogFileSink::LogFileSink(const DataPaths &paths, LogBuffer *buffer, qint64 maxBytes, int maxBackups,
                         QObject *parent)
    : QObject(parent)
    , m_paths(paths)
    , m_maxBytes(std::max<qint64>(1024, maxBytes))
    , m_maxBackups(std::max(1, maxBackups))
{
    if (buffer) {
        connect(buffer, &LogBuffer::entryAppended, this, &LogFileSink::onEntryAppended);
    }
}

QString LogFileSink::currentLogFile() const
{
    return QDir(m_paths.logDir()).filePath(QStringLiteral("tengwangge.log"));
}

void LogFileSink::rotateIfNeeded(const QString &filePath) const
{
    const QFileInfo info(filePath);
    if (!info.exists() || info.size() < m_maxBytes)
        return;

    const QString oldest = QStringLiteral("%1.%2").arg(filePath).arg(m_maxBackups);
    QFile::remove(oldest);

    for (int i = m_maxBackups - 1; i >= 1; --i) {
        const QString src = QStringLiteral("%1.%2").arg(filePath).arg(i);
        const QString dst = QStringLiteral("%1.%2").arg(filePath).arg(i + 1);
        if (QFile::exists(src)) {
            QFile::remove(dst);
            QFile::rename(src, dst);
            QFile::setPermissions(dst, kPrivateFilePermissions);
        }
    }
    const QString firstBackup = QStringLiteral("%1.1").arg(filePath);
    QFile::remove(firstBackup);
    QFile::rename(filePath, firstBackup);
    QFile::setPermissions(firstBackup, kPrivateFilePermissions);
}

void LogFileSink::onEntryAppended(const LogEntry &entry)
{
    if (m_paths.ensureLogDir().isEmpty())
        return;

    const QString filePath = currentLogFile();
    rotateIfNeeded(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.setPermissions(kPrivateFilePermissions);
    file.write(formatDiskLine(entry).toUtf8());
    file.flush();
}

} // namespace tw
