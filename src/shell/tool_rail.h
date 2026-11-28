#pragma once

#include "core/tool_info.h"

#include <QListWidget>
#include <QVector>

namespace tw {

// 顶部水平大图标工具带（Ribbon）：每个工具一个“上图（30x30 双色矢量）+ 下文”按钮，
// 1:1 对齐工业软件顶部模块切换条，释放左侧全部空间给业务工作区。
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
    QSize minimumSizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

} // namespace tw
