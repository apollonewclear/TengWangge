#pragma once

#include "core/platform_services.h"
#include "shell/tool_registry.h"

#include <QMainWindow>
#include <QPointer>

#include <functional>
#include <memory>

class QFrame;
class QLabel;
class QSplitter;
class QStackedWidget;
class QToolButton;

namespace tw {

class LogPanel;
class StatusStrip;
class ToolHost;
class ToolRail;
class ToolView;

// 外壳主窗口：两层导航。
//   第一层  左侧工具栏（ToolRail）选择“用哪个工具”；
//   第二层  工具自己的页面条（ToolView 内的 QTabBar）选择“工具里的哪一页”。
// 顶栏显示“工具 › 页面”，底部是状态栏和可收起的日志面板。
//
// MainWindow 拥有 PlatformServices 和 ToolRegistry，并保证销毁顺序：
// 先让工具 shutdown、销毁所有页面，再销毁工具对象，最后销毁平台服务。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(std::unique_ptr<PlatformServices> platform, ToolRegistry registry,
               QWidget *parent = nullptr, const QString &initialToolId = QString());
    ~MainWindow() override;

    // ---- 导航 ----
    int toolCount() const;
    int currentToolIndex() const;
    QString currentToolId() const;
    ToolView *currentView() const;
    bool navigationLocked() const;

    // 导航被锁定时返回 false。
    bool selectTool(int index);
    bool selectToolById(const QString &toolId);
    bool stepTool(int delta);                 // 到头不循环
    bool selectPage(int index);               // 当前工具内
    bool selectPageById(const QString &pageId);
    bool stepPage(int delta);
    void cycleFocus();                        // F6：工具栏 → 页面条 → 页面 → 日志

    void setLogPanelVisible(bool visible);
    bool isLogPanelVisible() const;

    // 有任务持有导航锁时关闭窗口会先调用它；返回 false 取消关闭。
    // 默认弹出确认框。测试里替换成固定返回值。
    void setCloseGuard(std::function<bool(const QString &reason)> guard);

    // ---- 部件与服务（供测试和少量宿主代码使用）----
    PlatformServices *platform() const { return m_platform.get(); }
    ToolHost *toolHost() const { return m_host; }
    ToolRail *toolRail() const { return m_rail; }
    StatusStrip *statusStrip() const { return m_status; }
    LogPanel *logPanel() const { return m_log; }
    QStackedWidget *toolStack() const { return m_stack; }

    static QString shortcutHelpText();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void installShortcuts();
    void connectServices();
    void onRailRowChanged(int row);
    void onViewCreated(int index);
    void refreshChrome();
    void refreshStatus();
    void refreshNavigationLock();
    void syncRailSelection();
    void showShortcutHelp();

    // 声明顺序即销毁顺序的反序：平台最后销毁。
    std::unique_ptr<PlatformServices> m_platform;
    ToolRegistry m_registry;

    ToolHost *m_host = nullptr;
    ToolRail *m_rail = nullptr;
    QStackedWidget *m_stack = nullptr;
    QSplitter *m_splitter = nullptr;
    LogPanel *m_log = nullptr;
    StatusStrip *m_status = nullptr;
    QLabel *m_crumbTool = nullptr;
    QLabel *m_crumbSep = nullptr;
    QLabel *m_crumbPage = nullptr;
    QToolButton *m_topLogBtn = nullptr;
    QToolButton *m_topHelpBtn = nullptr;

    QPointer<QWidget> m_focusBeforeLock;
    std::function<bool(const QString &)> m_closeGuard;
    bool m_syncingRail = false;
    bool m_tornDown = false;
};

} // namespace tw
