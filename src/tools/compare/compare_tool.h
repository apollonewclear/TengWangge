#pragma once

#include "shell/i_tool.h"

#include <QPointer>

namespace tw {

class FolderComparePage;
class IniComparePage;
class TextComparePage;
class ThreeWayMergePage;

// 文件对比工具（参考 Beyond Compare）：
// 集成文件夹比较、文本并排比较（可编辑 + 块复制）、INI/配置文件按节键核对与三方合并四个页面。
class CompareTool : public ITool
{
public:
    ToolInfo info() const override;
    bool initialize(const ToolContext &context, QString *error) override;
    QVector<ToolPage> createPages() override;
    void activated() override;
    void pageActivated(const QString &pageId) override;
    void shutdown() override;

    FolderComparePage *folderPage() const;
    TextComparePage *textPage() const;
    IniComparePage *iniPage() const;
    ThreeWayMergePage *mergePage() const;

private:
    ToolContext m_context;
    QPointer<FolderComparePage> m_folderPage;
    QPointer<TextComparePage> m_textPage;
    QPointer<IniComparePage> m_iniPage;
    QPointer<ThreeWayMergePage> m_mergePage;
};

} // namespace tw
