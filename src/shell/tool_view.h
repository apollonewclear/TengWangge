#pragma once

#include "core/tool_info.h"

#include <QVector>
#include <QWidget>

class QEvent;
class QFrame;
class QStackedWidget;
class QTabBar;

namespace tw {

// 一个工具自己的工作区：上面是页面条，下面是该工具的页面栈。
// 每个工具一份，外壳只在“工具”这一层切换它们，所以每个工具的当前页、
// 页面内容和控件状态天然保留，不需要在切换时重建。
class ToolView : public QWidget
{
    Q_OBJECT

public:
    explicit ToolView(const ToolInfo &info, QWidget *parent = nullptr);

    // 接管 widget 的所有权。
    void addPage(const PageInfo &info, QWidget *widget);

    ToolInfo toolInfo() const { return m_info; }
    int pageCount() const { return m_pages.size(); }
    int currentPage() const;
    PageInfo pageInfo(int index) const;
    QWidget *pageWidget(int index) const;
    int indexOfPage(const QString &pageId) const;

    bool setCurrentPage(int index);
    bool stepPage(int delta); // 到头不循环

    QTabBar *tabBar() const { return m_tabs; }
    QStackedWidget *stack() const { return m_stack; }

    // 导航被锁定时置灰页面条；页面内容本身不受影响。
    void setNavigationEnabled(bool enabled);

signals:
    void currentPageChanged(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refreshTabIcons();

    ToolInfo m_info;
    QVector<PageInfo> m_pages;
    QVector<QWidget *> m_pageWidgets;
    QFrame *m_strip = nullptr;
    QTabBar *m_tabs = nullptr;
    QStackedWidget *m_stack = nullptr;
};

} // namespace tw
