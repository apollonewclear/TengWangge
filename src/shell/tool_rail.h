#pragma once

#include "core/tool_info.h"

#include <QListWidget>
#include <QVector>

namespace tw {

// 左侧工具栏：每个工具一行（图标 + 名称 + Ctrl+N 提示）。
//
// - 当前工具：左侧琥珀色竖条 + 加亮底色，不依赖颜色以外的唯一线索（字重也变粗）。
// - 键盘焦点：整行描边，和选中态可同时出现。
// - 不可用工具：整行变灰并带“不可用”标签，仍可选中以查看原因。
// - 导航被锁定（setEnabled(false)）：整行变灰，当前位置仍可辨认。
// 行由 ToolRailDelegate 绘制，颜色取自 Theme，不依赖 QSS 选择器。
class ToolRail : public QListWidget
{
    Q_OBJECT

public:
    enum Role {
        IconKeyRole = Qt::UserRole + 1,
        AvailableRole,
        ShortcutRole,
        SummaryRole,
    };

    explicit ToolRail(QWidget *parent = nullptr);

    void setTools(const QVector<ToolInfo> &tools);
    void setAvailable(int index, bool available, const QString &reason = QString());
    bool isAvailable(int index) const;

    QSize sizeHint() const override;
};

} // namespace tw
