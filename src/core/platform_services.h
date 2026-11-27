#pragma once

#include "core/data_paths.h"
#include "core/log_buffer.h"
#include "core/log_file_sink.h"
#include "core/navigation_gate.h"
#include "core/status_board.h"

#include <QObject>
#include <QPointer>
#include <QString>

namespace tw {

class ToolServices;

// 平台级共享状态：日志、各工具状态、导航锁、数据目录。
// 不依赖任何 Widgets，可以在没有图形环境的测试里直接构造。
class PlatformServices : public QObject
{
    Q_OBJECT

public:
    explicit PlatformServices(const DataPaths &paths, QObject *parent = nullptr);

    LogBuffer *log() { return &m_log; }
    LogFileSink *fileSink() { return &m_fileSink; }
    StatusBoard *status() { return &m_status; }
    NavigationGate *gate() { return &m_gate; }
    const DataPaths &paths() const { return m_paths; }

    // 给某个工具的带作用域句柄；toolId 为空表示平台自身。
    ToolServices servicesFor(const QString &toolId);

private:
    DataPaths m_paths;
    LogBuffer m_log;
    LogFileSink m_fileSink;
    StatusBoard m_status;
    NavigationGate m_gate;
};

// 工具拿到的服务句柄。所有写入都自动带上工具标识，工具无法冒充别的工具，
// 也拿不到别的工具的数据目录。句柄可以按值复制，平台销毁后调用是空操作。
class ToolServices
{
public:
    ToolServices() = default;
    ToolServices(PlatformServices *platform, const QString &toolId);

    bool valid() const { return !m_platform.isNull(); }
    QString toolId() const { return m_toolId; }

    void log(LogLevel level, const QString &message) const;
    void logInfo(const QString &message) const { log(LogLevel::Info, message); }
    void logSuccess(const QString &message) const { log(LogLevel::Success, message); }
    void logWarning(const QString &message) const { log(LogLevel::Warning, message); }
    void logError(const QString &message) const { log(LogLevel::Error, message); }

    void showStatus(StatusState state, const QString &text,
                    int timeoutMs = StatusBoard::UseDefaultTimeout) const;
    void clearStatus() const;

    // 持有期间工具和页面导航被锁定；释放（或析构）后恢复。
    NavigationLock lockNavigation(const QString &reason) const;

    // 本工具独占的数据目录与配置文件路径，按需创建。
    QString dataDir(QString *error = nullptr) const;
    QString settingsFile(QString *error = nullptr) const;

private:
    QPointer<PlatformServices> m_platform;
    QString m_toolId;
};

} // namespace tw
