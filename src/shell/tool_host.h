#pragma once

#include "core/platform_services.h"
#include "shell/tool_registry.h"

#include <QObject>
#include <QPointer>
#include <QVector>

class QStackedWidget;
class QWidget;

namespace tw {

class ToolView;

// 驱动工具生命周期，并把每个工具的 ToolView 放进外层页面栈。
// 不认识主窗口：测试可以直接用假工具和一个 QStackedWidget 验证生命周期规则。
//
// 规则：
//  * 一个工具初始化失败，或页面无效，只让它自己变成“不可用”，不影响其他工具。
//  * 页面只创建一次，第一次激活时才创建；之后切换只改变外层栈的当前项。
//  * 不可用的工具也可以被选中，显示原因；它不会收到 activated()/deactivated()。
//  * shutdown() 幂等；工具持有的导航锁在 shutdown 和失败时被兜底释放。
class ToolHost : public QObject
{
    Q_OBJECT

public:
    enum class State { Pending, Ready, Failed };

    ToolHost(ToolRegistry *registry, PlatformServices *platform, QWidget *dialogParent,
             QStackedWidget *stack, QObject *parent = nullptr);
    ~ToolHost() override;

    void initializeAll();

    int count() const { return m_slots.size(); }
    ToolInfo info(int index) const;
    State state(int index) const;
    QString error(int index) const;
    int currentIndex() const { return m_current; }
    ToolView *view(int index) const;
    QWidget *currentWidget() const;

    // 切换到第 index 个工具；第一次切换时创建它的页面。
    bool activate(int index);

    void shutdown();

signals:
    void viewCreated(int index);
    void toolFailed(int index, const QString &error);
    void toolActivated(int index);

private:
    struct Slot {
        State state = State::Pending;
        QString error;
        bool initialized = false;
        bool active = false;
        bool pagesAttempted = false;
        QPointer<ToolView> view;
        QPointer<QWidget> unavailable;
    };

    bool createView(int index);
    void fail(int index, const QString &message);
    void notifyPageActivated(int index);
    QWidget *ensureUnavailableView(int index);

    ToolRegistry *m_registry;
    PlatformServices *m_platform;
    QWidget *m_dialogParent;
    QStackedWidget *m_stack;
    QVector<Slot> m_slots;
    int m_current = -1;
    bool m_shutDown = false;
};

} // namespace tw
