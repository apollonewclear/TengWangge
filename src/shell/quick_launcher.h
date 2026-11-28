#pragma once

#include "core/tool_info.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QListWidget;

namespace tw {

// Ctrl+K 全局快速搜索弹窗：输入工具名称、编码、分类或简介，上下键选择、回车直达。
class QuickLauncherDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickLauncherDialog(QWidget *parent = nullptr);

    void setTools(const QVector<ToolInfo> &tools, const QVector<bool> &available);
    void setQuery(const QString &query);

    int matchedCount() const;
    int selectedToolIndex() const { return m_selectedIndex; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildList();
    void acceptCurrent();

    QVector<ToolInfo> m_tools;
    QVector<bool> m_available;
    int m_selectedIndex = -1;

    QLineEdit *m_query = nullptr;
    QListWidget *m_list = nullptr;
};

} // namespace tw
