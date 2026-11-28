#pragma once

#include <QString>

namespace tw {

// 工具与页面标识：小写字母开头，由小写字母、数字组成，可用 . 或 - 分段。
// 例如 "welcome"、"task-demo"、"deploy.history"。标识会进入目录名和
// 对象名，所以限制字符集，不允许路径分隔符和大小写差异。
bool isValidIdentifier(const QString &id);

struct PageInfo
{
    QString id;
    QString title;
    QString summary;
    QString iconKey;
};

struct ToolInfo
{
    QString id;
    QString title;
    QString summary;
    QString iconKey;
    QString category; // 业务分类（用于工具中心与 Ctrl+K 速搜分组；为空时归入“通用工具”）
    int order = 100;  // 越小越靠前；相同时保持注册顺序

    QString effectiveCategory() const
    {
        return category.trimmed().isEmpty() ? QStringLiteral("通用工具") : category.trimmed();
    }
};

} // namespace tw
