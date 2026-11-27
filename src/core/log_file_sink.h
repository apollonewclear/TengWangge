#pragma once

#include "core/data_paths.h"
#include "core/log_buffer.h"

#include <QObject>
#include <QString>

namespace tw {

// 平台日志本地滚动落盘器：订阅 LogBuffer::entryAppended，将每条日志追加写入
// <root>/logs/tengwangge.log（权限 0600），超过 maxBytes 时自动按 .1 .. .N 滚动。
class LogFileSink : public QObject
{
    Q_OBJECT

public:
    explicit LogFileSink(const DataPaths &paths, LogBuffer *buffer,
                         qint64 maxBytes = 2 * 1024 * 1024, int maxBackups = 3,
                         QObject *parent = nullptr);

    QString currentLogFile() const;
    qint64 maxBytes() const { return m_maxBytes; }
    int maxBackups() const { return m_maxBackups; }

private:
    void onEntryAppended(const LogEntry &entry);
    void rotateIfNeeded(const QString &filePath) const;

    DataPaths m_paths;
    qint64 m_maxBytes;
    int m_maxBackups;
};

} // namespace tw
