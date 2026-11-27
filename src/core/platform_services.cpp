#include "core/platform_services.h"

#include <QMetaObject>
#include <QThread>

namespace tw {

PlatformServices::PlatformServices(const DataPaths &paths, QObject *parent)
    : QObject(parent)
    , m_paths(paths)
    , m_log(2000)
    , m_fileSink(m_paths, &m_log)
{
}

ToolServices PlatformServices::servicesFor(const QString &toolId)
{
    return ToolServices(this, toolId);
}

void PlatformServices::requestNavigation(const QString &fromToolId, const QString &targetToolId,
                                         const QString &targetPageId)
{
    emit navigationRequested(fromToolId, targetToolId, targetPageId);
}

ToolServices::ToolServices(PlatformServices *platform, const QString &toolId)
    : m_platform(platform)
    , m_toolId(toolId)
{
}

void ToolServices::log(LogLevel level, const QString &message) const
{
    PlatformServices *platform = m_platform.data();
    if (!platform)
        return;
    if (QThread::currentThread() == platform->thread()) {
        platform->log()->append(level, m_toolId, message);
        return;
    }
    const QString toolId = m_toolId;
    QMetaObject::invokeMethod(
        platform,
        [platform, level, toolId, message]() {
            platform->log()->append(level, toolId, message);
        },
        Qt::QueuedConnection);
}

void ToolServices::showStatus(StatusState state, const QString &text, int timeoutMs) const
{
    PlatformServices *platform = m_platform.data();
    if (!platform)
        return;
    if (QThread::currentThread() == platform->thread()) {
        platform->status()->set(m_toolId, state, text, timeoutMs);
        return;
    }
    const QString toolId = m_toolId;
    QMetaObject::invokeMethod(
        platform,
        [platform, state, toolId, text, timeoutMs]() {
            platform->status()->set(toolId, state, text, timeoutMs);
        },
        Qt::QueuedConnection);
}

void ToolServices::clearStatus() const
{
    PlatformServices *platform = m_platform.data();
    if (!platform)
        return;
    if (QThread::currentThread() == platform->thread()) {
        platform->status()->clear(m_toolId);
        return;
    }
    const QString toolId = m_toolId;
    QMetaObject::invokeMethod(
        platform,
        [platform, toolId]() {
            platform->status()->clear(toolId);
        },
        Qt::QueuedConnection);
}

NavigationLock ToolServices::lockNavigation(const QString &reason) const
{
    if (!m_platform)
        return NavigationLock();
    Q_ASSERT_X(QThread::currentThread() == m_platform->thread(),
               "ToolServices::lockNavigation",
               "NavigationLock must be acquired on the platform (GUI) thread");
    return NavigationLock(m_platform->gate(), m_toolId, reason);
}

QString ToolServices::dataDir(QString *error) const
{
    if (!m_platform) {
        if (error)
            *error = QStringLiteral("平台服务已不可用");
        return {};
    }
    return m_platform->paths().toolDataDir(m_toolId, error);
}

QString ToolServices::settingsFile(QString *error) const
{
    if (!m_platform) {
        if (error)
            *error = QStringLiteral("平台服务已不可用");
        return {};
    }
    return m_platform->paths().toolSettingsFile(m_toolId, error);
}

void ToolServices::requestNavigation(const QString &targetToolId, const QString &targetPageId) const
{
    PlatformServices *platform = m_platform.data();
    if (!platform)
        return;
    if (QThread::currentThread() == platform->thread()) {
        platform->requestNavigation(m_toolId, targetToolId, targetPageId);
        return;
    }
    const QString fromToolId = m_toolId;
    QMetaObject::invokeMethod(
        platform,
        [platform, fromToolId, targetToolId, targetPageId]() {
            platform->requestNavigation(fromToolId, targetToolId, targetPageId);
        },
        Qt::QueuedConnection);
}

} // namespace tw
