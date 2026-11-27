#pragma once

#include "core/data_paths.h"

#include <QHash>
#include <QSize>
#include <QString>

namespace tw {

// 外壳会话状态（存储在隔离数据目录 <root>/config/shell.ini，不污染系统 ~/.config）。
struct SessionState
{
    QSize windowSize;
    bool logVisible = false;
    int logHeight = 0;
    QString currentToolId;
    QHash<QString, QString> toolPages; // toolId -> pageId
};

class ShellConfig
{
public:
    explicit ShellConfig(const DataPaths &paths);

    bool exists() const;
    SessionState load() const;
    bool save(const SessionState &state, QString *error = nullptr) const;

private:
    DataPaths m_paths;
};

} // namespace tw
