#include "core/shell_config.h"

#include "core/tool_info.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace tw {
namespace {

constexpr QFileDevice::Permissions kPrivateFilePermissions =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner;

QString rawShellIniPath(const DataPaths &paths)
{
    return QDir(paths.configDir()).filePath(QStringLiteral("shell.ini"));
}

} // namespace

ShellConfig::ShellConfig(const DataPaths &paths)
    : m_paths(paths)
{
}

bool ShellConfig::exists() const
{
    if (!DataPaths::rootProblem(m_paths.root()).isEmpty())
        return false;
    return QFileInfo::exists(rawShellIniPath(m_paths));
}

SessionState ShellConfig::load() const
{
    SessionState state;
    if (!exists())
        return state;

    QSettings settings(rawShellIniPath(m_paths), QSettings::IniFormat);
    const int width = settings.value(QStringLiteral("window/width"), 0).toInt();
    const int height = settings.value(QStringLiteral("window/height"), 0).toInt();
    if (width >= 400 && width <= 8000 && height >= 300 && height <= 8000)
        state.windowSize = QSize(width, height);

    state.logVisible = settings.value(QStringLiteral("log/visible"), false).toBool();
    const int logHeight = settings.value(QStringLiteral("log/height"), 0).toInt();
    if (logHeight >= 80 && logHeight <= 1200)
        state.logHeight = logHeight;

    const QString toolId = settings.value(QStringLiteral("navigation/currentTool")).toString();
    if (isValidIdentifier(toolId))
        state.currentToolId = toolId;

    settings.beginGroup(QStringLiteral("toolPages"));
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        const QString pageId = settings.value(key).toString();
        if (isValidIdentifier(key) && isValidIdentifier(pageId))
            state.toolPages.insert(key, pageId);
    }
    settings.endGroup();

    return state;
}

bool ShellConfig::save(const SessionState &state, QString *error) const
{
    const QString path = m_paths.shellConfigFile(error);
    if (path.isEmpty())
        return false;

    QSettings settings(path, QSettings::IniFormat);
    settings.clear();
    if (state.windowSize.isValid()) {
        settings.setValue(QStringLiteral("window/width"), state.windowSize.width());
        settings.setValue(QStringLiteral("window/height"), state.windowSize.height());
    }
    settings.setValue(QStringLiteral("log/visible"), state.logVisible);
    if (state.logHeight > 0)
        settings.setValue(QStringLiteral("log/height"), state.logHeight);
    if (isValidIdentifier(state.currentToolId))
        settings.setValue(QStringLiteral("navigation/currentTool"), state.currentToolId);

    settings.beginGroup(QStringLiteral("toolPages"));
    for (auto it = state.toolPages.constBegin(); it != state.toolPages.constEnd(); ++it) {
        if (isValidIdentifier(it.key()) && isValidIdentifier(it.value()))
            settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = QStringLiteral("无法写入会话配置：%1").arg(path);
        return false;
    }
    QFile::setPermissions(path, kPrivateFilePermissions);
    return true;
}

} // namespace tw
