#include "core/data_paths.h"

#include "core/app_info.h"
#include "core/tool_info.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace tw {
namespace {

constexpr QFileDevice::Permissions kPrivateDirPermissions =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;

QString absoluteClean(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};
    return QDir::cleanPath(QDir::current().absoluteFilePath(path.trimmed()));
}

bool isSystemPrefix(const QString &cleanPath)
{
    static const QStringList systemRoots{
        QStringLiteral("/bin"),         QStringLiteral("/boot"),
        QStringLiteral("/dev"),         QStringLiteral("/etc"),
        QStringLiteral("/lib"),         QStringLiteral("/lib32"),
        QStringLiteral("/lib64"),       QStringLiteral("/private/etc"),
        QStringLiteral("/proc"),        QStringLiteral("/root"),
        QStringLiteral("/run"),         QStringLiteral("/sbin"),
        QStringLiteral("/sys"),         QStringLiteral("/usr"),
    };
    for (const QString &system : systemRoots) {
        if (cleanPath == system || cleanPath.startsWith(system + QLatin1Char('/')))
            return true;
    }
    return false;
}

// 向上查找最近已存在（或本身为符号链接）的祖先路径并解析真实目标，防止软链接绕过系统目录检查。
QString resolveRealPath(const QString &cleanPath)
{
    if (cleanPath.isEmpty() || cleanPath == QLatin1String("/"))
        return cleanPath;

    QString probe = cleanPath;
    QStringList tail;
    while (!probe.isEmpty() && probe != QLatin1String("/")) {
        const QFileInfo info(probe);
        if (info.exists() || info.isSymLink())
            break;
        tail.prepend(info.fileName());
        const QString parent = info.path();
        if (parent == probe)
            break;
        probe = parent;
    }

    QString base = probe;
    const QFileInfo probeInfo(probe);
    if (probeInfo.exists()) {
        const QString canonical = probeInfo.canonicalFilePath();
        if (!canonical.isEmpty())
            base = canonical;
    } else if (probeInfo.isSymLink()) {
        const QString target = absoluteClean(probeInfo.symLinkTarget());
        if (!target.isEmpty())
            base = target;
    }

    for (const QString &segment : tail)
        base = QDir(base).filePath(segment);
    return QDir::cleanPath(base);
}

bool ensurePrivateDirectory(const QString &path, QString *error)
{
    const QFileInfo existing(path);
    if (existing.exists() && !existing.isDir()) {
        if (error)
            *error = QStringLiteral("数据路径不是目录：%1").arg(path);
        return false;
    }
    if (!QDir(path).exists() && !QDir().mkpath(path)) {
        if (error)
            *error = QStringLiteral("无法创建数据目录：%1").arg(path);
        return false;
    }
    QFile::setPermissions(path, kPrivateDirPermissions);
    return true;
}

} // namespace

const char *DataPaths::overrideVariable()
{
    return "TENGWANGGE_DATA_DIR";
}

DataPaths::DataPaths(const QString &root)
    : m_root(absoluteClean(root))
{
}

DataPaths DataPaths::standard()
{
    const QString configured = qEnvironmentVariable(overrideVariable());
    if (!configured.trimmed().isEmpty())
        return DataPaths(configured);

    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::home().filePath(QStringLiteral(".local/share"));
    return DataPaths(QDir(base).filePath(AppInfo::dataDirName()));
}

QString DataPaths::logDir() const
{
    return QDir(m_root).filePath(QStringLiteral("logs"));
}

QString DataPaths::ensureLogDir(QString *error) const
{
    const QString problem = rootProblem(m_root);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return {};
    }
    if (!ensurePrivateDirectory(m_root, error))
        return {};
    const QString dir = logDir();
    if (!ensurePrivateDirectory(dir, error))
        return {};
    return dir;
}

QString DataPaths::configDir() const
{
    return QDir(m_root).filePath(QStringLiteral("config"));
}

QString DataPaths::ensureConfigDir(QString *error) const
{
    const QString problem = rootProblem(m_root);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return {};
    }
    if (!ensurePrivateDirectory(m_root, error))
        return {};
    const QString dir = configDir();
    if (!ensurePrivateDirectory(dir, error))
        return {};
    return dir;
}

QString DataPaths::shellConfigFile(QString *error) const
{
    const QString dir = ensureConfigDir(error);
    if (dir.isEmpty())
        return {};
    return QDir(dir).filePath(QStringLiteral("shell.ini"));
}

QString DataPaths::rootProblem(const QString &root)
{
    const QString clean = absoluteClean(root);
    if (clean.isEmpty())
        return QStringLiteral("数据根目录为空");
    if (clean == QLatin1String("/"))
        return QStringLiteral("数据根目录不能是文件系统根目录");
    if (isSystemPrefix(clean))
        return QStringLiteral("数据根目录位于系统目录下，已拒绝：%1").arg(clean);

    const QString resolved = resolveRealPath(clean);
    if (resolved == QLatin1String("/"))
        return QStringLiteral("数据根目录软链接指向文件系统根目录，已拒绝：%1").arg(clean);
    if (isSystemPrefix(resolved))
        return QStringLiteral("数据根目录软链接指向系统目录（%1），已拒绝：%2").arg(resolved, clean);
    return {};
}

QString DataPaths::toolDataDir(const QString &toolId, QString *error) const
{
    if (!isValidIdentifier(toolId)) {
        if (error)
            *error = QStringLiteral("工具标识无效：%1").arg(toolId);
        return {};
    }
    const QString problem = rootProblem(m_root);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return {};
    }

    if (!ensurePrivateDirectory(m_root, error))
        return {};
    const QString toolsRoot = QDir(m_root).filePath(QStringLiteral("tools"));
    if (!ensurePrivateDirectory(toolsRoot, error))
        return {};
    const QString path = QDir(toolsRoot).filePath(toolId);
    if (!ensurePrivateDirectory(path, error))
        return {};
    return path;
}

QString DataPaths::toolSettingsFile(const QString &toolId, QString *error) const
{
    const QString dir = toolDataDir(toolId, error);
    if (dir.isEmpty())
        return {};
    return QDir(dir).filePath(QStringLiteral("settings.ini"));
}

} // namespace tw
