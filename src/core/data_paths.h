#pragma once

#include <QString>

namespace tw {

// 平台与各工具的隔离数据目录。
//
//   <root>/tools/<toolId>/   每个工具只拿到自己的目录
//   <root>/logs/             平台日志目录（按需创建）
//
// <root> 默认是 ~/.local/share/TengWangge，可用环境变量 TENGWANGGE_DATA_DIR
// 覆盖（测试和受控部署使用）。根目录不允许落在系统目录下。
class DataPaths
{
public:
    static const char *overrideVariable();

    explicit DataPaths(const QString &root);
    static DataPaths standard();

    QString root() const { return m_root; }
    QString logDir() const;
    QString ensureLogDir(QString *error = nullptr) const;

    // 返回已创建的工具目录（0700 权限）；失败时返回空串并写入 error。
    QString toolDataDir(const QString &toolId, QString *error = nullptr) const;

    // 非空表示该根目录不可用（空、根目录或系统目录）。
    static QString rootProblem(const QString &root);

private:
    QString m_root;
};

} // namespace tw
