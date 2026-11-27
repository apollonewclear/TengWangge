#pragma once

#include "shell/i_tool.h"

namespace tw {

// 内置示例工具：平台概览与键盘操作说明。
// 它同时是“最小工具”的参考实现：多页面、只读、不持有任何锁。
class WelcomeTool : public ITool
{
public:
    ToolInfo info() const override;
    bool initialize(const ToolContext &context, QString *error) override;
    QVector<ToolPage> createPages() override;
    void activated() override;
    void pageActivated(const QString &pageId) override;

private:
    ToolContext m_context;
};

} // namespace tw
