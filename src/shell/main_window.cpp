#include "shell/main_window.h"

#include "core/app_info.h"
#include "shell/icon_factory.h"
#include "shell/log_panel.h"
#include "shell/page_chrome.h"
#include "shell/quick_launcher.h"
#include "shell/status_strip.h"
#include "shell/theme.h"
#include "shell/tool_host.h"
#include "shell/tool_hub_view.h"
#include "shell/tool_rail.h"
#include "shell/tool_view.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSysInfo>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace tw {
namespace {

QWidget *firstFocusable(QWidget *root)
{
    if (!root)
        return nullptr;
    QWidget *w = root;
    for (int i = 0; i < 500; ++i) {
        w = w->nextInFocusChain();
        if (!w || w == root)
            break;
        if (root->isAncestorOf(w) && w->isEnabled() && w->isVisibleTo(root->window())
            && (w->focusPolicy() & Qt::TabFocus))
            return w;
    }
    return (root->focusPolicy() & Qt::TabFocus) ? root : nullptr;
}

} // namespace

MainWindow::MainWindow(std::unique_ptr<PlatformServices> platform, ToolRegistry registry,
                       QWidget *parent, const QString &initialToolId)
    : QMainWindow(parent)
    , m_platform(std::move(platform))
    , m_registry(std::move(registry))
{
    Q_ASSERT(m_platform);

    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(AppInfo::productName());
    setWindowIcon(IconFactory::appIcon());
    resize(1180, 740);
    setMinimumSize(900, 560);
    setStyleSheet(Theme::workbench().styleSheet());

    buildUi();

    m_host = new ToolHost(&m_registry, m_platform.get(), this, m_stack, this);
    connectServices();

    ToolServices self = m_platform->servicesFor(QString());
    self.logInfo(QStringLiteral("平台启动完成：%1 %2").arg(AppInfo::productName(), AppInfo::version()));
    self.logInfo(QStringLiteral("隔离数据目录：%1").arg(m_platform->paths().root()));

    m_host->initializeAll();

    QVector<ToolInfo> infos;
    for (int i = 0; i < m_host->count(); ++i)
        infos.append(m_host->info(i));
    m_rail->setTools(infos);
    m_hubView->setTools(infos);
    m_moreToolsBtn->setVisible(infos.size() > 6);
    for (int i = 0; i < m_host->count(); ++i) {
        if (m_host->state(i) == ToolHost::State::Failed) {
            m_rail->setAvailable(i, false, m_host->error(i));
            m_hubView->setAvailable(i, false, m_host->error(i));
        }
    }
    self.logInfo(QStringLiteral("加载全部工具模块完成（共 %1 项）").arg(infos.size()));

    installShortcuts();

    if (m_host->count() == 0) {
        auto *empty = new QWidget;
        empty->setObjectName(QStringLiteral("emptyView"));
        empty->setAttribute(Qt::WA_StyledBackground, true);
        auto *layout = new QVBoxLayout(empty);
        auto *label = new QLabel(QStringLiteral("尚未安装任何工具"));
        label->setObjectName(QStringLiteral("emptyTitle"));
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        m_stack->addWidget(empty);
        refreshChrome();
        return;
    }

    int first = -1;
    if (!initialToolId.isEmpty())
        first = m_registry.indexOf(initialToolId);
    if (first < 0) {
        first = 0;
        for (int i = 0; i < m_host->count(); ++i) {
            if (m_host->state(i) == ToolHost::State::Ready) {
                first = i;
                break;
            }
        }
    }
    selectTool(first);
    refreshNavigationLock();

    // 默认常驻紧凑消息日志区（~110px，1:1 对齐参考图底部常驻消息框）
    setLogPanelVisible(true);
    m_splitter->setSizes({600, 112});
}

MainWindow::~MainWindow()
{
    m_tornDown = true;
    m_platform->status()->disconnect(this);
    m_platform->gate()->disconnect(this);

    if (m_host)
        m_host->shutdown();
    delete takeCentralWidget();
    delete m_host;
    m_host = nullptr;
}

void MainWindow::buildUi()
{
    const Theme &theme = Theme::workbench();

    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("centralArea"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 68px 顶部合一 Ribbon 大图标工具带（左：工具中心 + 可滚模块带 + 溢出菜单；右：Ctrl+K 速搜 + 日志/帮助/退出）----
    auto *top = new QFrame;
    top->setObjectName(QStringLiteral("topBar"));
    top->setFixedHeight(68);
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(10, 4, 12, 4);
    topLayout->setSpacing(6);

    auto makeRibbonAction = [&](const QString &text, const QString &iconKey,
                                const QString &tip, bool checkable) {
        auto *btn = new QToolButton;
        btn->setObjectName(QStringLiteral("topActionBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIcon(IconFactory::icon(iconKey, theme.primary, theme.primary,
                                       theme.disabledText, theme.accent));
        btn->setIconSize(QSize(28, 28));
        btn->setFixedSize(74, 58);
        btn->setText(text);
        btn->setCheckable(checkable);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setToolTip(tip);
        return btn;
    };

    m_hubBtn = makeRibbonAction(QStringLiteral("工具中心"), QStringLiteral("hub"),
                                QStringLiteral("按分类浏览与搜索全部工具模块 (Ctrl+0)"), true);
    auto *divider = new QFrame;
    divider->setObjectName(QStringLiteral("topDivider"));
    divider->setFixedHeight(40);

    m_rail = new ToolRail;

    m_moreToolsBtn = new QToolButton;
    m_moreToolsBtn->setObjectName(QStringLiteral("moreToolsBtn"));
    m_moreToolsBtn->setText(QStringLiteral("»"));
    m_moreToolsBtn->setToolTip(QStringLiteral("按业务分类查看全部工具模块"));
    m_moreToolsBtn->setFixedSize(26, 44);
    m_moreToolsBtn->setFocusPolicy(Qt::NoFocus);
    m_moreToolsBtn->hide();

    // 保留无障碍/状态元数据标签（隐藏显示，兼容路径查询）
    m_crumbTool = new QLabel(top);
    m_crumbTool->setObjectName(QStringLiteral("breadcrumbTool"));
    m_crumbTool->hide();
    m_crumbSep = new QLabel(QStringLiteral("›"), top);
    m_crumbSep->setObjectName(QStringLiteral("breadcrumbSep"));
    m_crumbSep->hide();
    m_crumbPage = new QLabel(top);
    m_crumbPage->setObjectName(QStringLiteral("breadcrumbPage"));
    m_crumbPage->hide();

    m_quickSearchBtn = new QPushButton(QStringLiteral(" 速搜  Ctrl+K"));
    m_quickSearchBtn->setObjectName(QStringLiteral("quickSearchBtn"));
    m_quickSearchBtn->setIcon(IconFactory::icon(QStringLiteral("search"), theme.primary, theme.primary,
                                                theme.disabledText, theme.accent));
    m_quickSearchBtn->setIconSize(QSize(14, 14));
    m_quickSearchBtn->setFixedWidth(122);
    m_quickSearchBtn->setFocusPolicy(Qt::NoFocus);
    m_quickSearchBtn->setToolTip(QStringLiteral("快速搜索并切换任意工具模块 (Ctrl+K)"));

    m_topLogBtn = makeRibbonAction(QStringLiteral("消息日志"), QStringLiteral("log"),
                                   QStringLiteral("显示/隐藏底部消息日志 (Ctrl+L)"), true);
    m_topHelpBtn = makeRibbonAction(QStringLiteral("操作帮助"), QStringLiteral("help"),
                                    QStringLiteral("查看键盘快捷键与操作说明 (F1)"), false);
    m_topExitBtn = makeRibbonAction(QStringLiteral("退出"), QStringLiteral("exit"),
                                    QStringLiteral("安全退出程序"), false);

    topLayout->addWidget(m_hubBtn, 0, Qt::AlignVCenter);
    topLayout->addWidget(divider, 0, Qt::AlignVCenter);
    topLayout->addWidget(m_rail, 1, Qt::AlignVCenter);
    topLayout->addWidget(m_moreToolsBtn, 0, Qt::AlignVCenter);
    topLayout->addSpacing(4);
    topLayout->addWidget(m_quickSearchBtn, 0, Qt::AlignVCenter);
    topLayout->addWidget(m_topLogBtn, 0, Qt::AlignVCenter);
    topLayout->addWidget(m_topHelpBtn, 0, Qt::AlignVCenter);
    topLayout->addWidget(m_topExitBtn, 0, Qt::AlignVCenter);
    root->addWidget(top);

    // ---- 全宽主工作区（工具栈 + 工具中心页）+ 五圆点拖拽条 + 底部常驻白底消息日志框 ----
    m_bodyStack = new QStackedWidget;
    m_bodyStack->setObjectName(QStringLiteral("bodyStack"));
    m_stack = new QStackedWidget;
    m_stack->setObjectName(QStringLiteral("toolStack"));
    m_hubView = new ToolHubView;
    m_bodyStack->addWidget(m_stack);
    m_bodyStack->addWidget(m_hubView);
    m_bodyStack->setCurrentWidget(m_stack);

    m_log = new LogPanel(m_platform->log());
    m_log->hide();

    m_splitter = ui::makeSplitter(Qt::Vertical);
    m_splitter->addWidget(m_bodyStack);
    m_splitter->addWidget(m_log);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    root->addWidget(m_splitter, 1);

    setCentralWidget(central);

    // ---- 26px 底部浅灰状态栏 ----
    m_status = new StatusStrip;
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setFixedHeight(26);
    statusBar()->addWidget(m_status, 1);
}

void MainWindow::connectServices()
{
    connect(m_rail, &QListWidget::currentRowChanged, this, &MainWindow::onRailRowChanged);
    connect(m_host, &ToolHost::viewCreated, this, &MainWindow::onViewCreated);
    connect(m_host, &ToolHost::toolFailed, this, [this](int index, const QString &error) {
        m_rail->setAvailable(index, false, error);
        m_hubView->setAvailable(index, false, error);
    });
    connect(m_hubBtn, &QToolButton::clicked, this, [this](bool checked) {
        showToolHub(checked);
    });
    connect(m_hubView, &ToolHubView::toolActivated, this, [this](int index) {
        selectTool(index);
    });
    connect(m_moreToolsBtn, &QToolButton::clicked, this, &MainWindow::showMoreToolsMenu);
    connect(m_quickSearchBtn, &QPushButton::clicked, this, &MainWindow::openQuickLauncher);
    connect(m_platform->status(), &StatusBoard::changed, this, [this](const QString &toolId) {
        if (toolId == currentToolId())
            refreshStatus();
    });
    connect(m_platform->gate(), &NavigationGate::changed, this, &MainWindow::refreshNavigationLock);
    connect(m_platform.get(), &PlatformServices::navigationRequested, this,
            [this](const QString &fromToolId, const QString &targetToolId, const QString &targetPageId) {
                if (navigationLocked()) {
                    m_platform->servicesFor(fromToolId).logWarning(
                        QStringLiteral("导航已锁定，已忽略跳转请求：%1")
                            .arg(targetToolId.isEmpty() ? targetPageId : targetToolId));
                    return;
                }
                if (!targetToolId.isEmpty() && !selectToolById(targetToolId)) {
                    m_platform->servicesFor(fromToolId).logWarning(
                        QStringLiteral("请求跳转的目标工具不存在：%1").arg(targetToolId));
                    return;
                }
                if (!targetPageId.isEmpty() && !selectPageById(targetPageId)) {
                    m_platform->servicesFor(fromToolId).logWarning(
                        QStringLiteral("请求跳转的目标页面不存在：%1").arg(targetPageId));
                }
            });
    connect(m_status->logToggle(), &QPushButton::toggled, this,
            [this](bool on) { setLogPanelVisible(on); });
    connect(m_topLogBtn, &QToolButton::toggled, this,
            [this](bool on) { setLogPanelVisible(on); });
    connect(m_topHelpBtn, &QToolButton::clicked, this, &MainWindow::showShortcutHelp);
    connect(m_topExitBtn, &QToolButton::clicked, this, &QWidget::close);
    connect(m_log, &LogPanel::closeRequested, this, [this] {
        setLogPanelVisible(false);
        if (m_rail->isEnabled())
            m_rail->setFocus(Qt::OtherFocusReason);
    });
    connect(m_log, &LogPanel::exported, this, [this](const QString &filePath) {
        m_platform->servicesFor(currentToolId()).showStatus(
            StatusState::Success, QStringLiteral("日志已导出：%1").arg(filePath));
    });
    connect(m_log, &LogPanel::exportFailed, this, [this](const QString &err) {
        m_platform->servicesFor(currentToolId()).showStatus(
            StatusState::Error, QStringLiteral("日志导出失败：%1").arg(err));
    });
    m_log->setTitleResolver([this](const QString &toolId) {
        const int index = m_registry.indexOf(toolId);
        return index >= 0 ? m_registry.infoAt(index).title : toolId;
    });
}

void MainWindow::installShortcuts()
{
    auto add = [this](const QKeySequence &keys, std::function<void()> action) {
        auto *shortcut = new QShortcut(keys, this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, std::move(action));
    };

    for (int i = 0; i < 9; ++i)
        add(QKeySequence(Qt::CTRL + Qt::Key_1 + i), [this, i] { selectTool(i); });
    add(QKeySequence(Qt::CTRL + Qt::Key_0), [this] { showToolHub(!isToolHubVisible()); });
    add(QKeySequence(Qt::CTRL + Qt::Key_K), [this] { openQuickLauncher(); });
    add(QKeySequence(Qt::ALT + Qt::Key_Up), [this] { stepTool(-1); });
    add(QKeySequence(Qt::ALT + Qt::Key_Down), [this] { stepTool(+1); });
    add(QKeySequence(Qt::CTRL + Qt::Key_PageUp), [this] { stepPage(-1); });
    add(QKeySequence(Qt::CTRL + Qt::Key_PageDown), [this] { stepPage(+1); });
    add(QKeySequence(Qt::CTRL + Qt::Key_L), [this] { setLogPanelVisible(!isLogPanelVisible()); });
    add(QKeySequence(Qt::Key_F6), [this] { cycleFocus(); });
    add(QKeySequence(Qt::Key_F1), [this] { showShortcutHelp(); });
}

QString MainWindow::shortcutHelpText()
{
    return QStringLiteral(
        "<table cellspacing='6' cellpadding='2'>"
        "<tr><td><b>Ctrl+0</b></td><td>打开 / 返回工具中心（按分类浏览全部模块）</td></tr>"
        "<tr><td><b>Ctrl+K</b></td><td>全局快速搜索并直达任意工具模块</td></tr>"
        "<tr><td><b>Ctrl+1 … Ctrl+9</b></td><td>切换到第 N 个工具模块</td></tr>"
        "<tr><td><b>Alt+↑ / Alt+↓</b></td><td>上一个 / 下一个工具模块</td></tr>"
        "<tr><td><b>Ctrl+PageUp / PageDown</b></td><td>当前工具的上一页 / 下一页</td></tr>"
        "<tr><td><b>F6</b></td><td>在顶部工具带、页面条、工作区、日志框之间移动焦点</td></tr>"
        "<tr><td><b>Ctrl+L</b></td><td>显示 / 隐藏底部消息日志</td></tr>"
        "<tr><td><b>← → / ↑ ↓</b></td><td>工具带或页面条内逐项移动</td></tr>"
        "<tr><td><b>右键消息日志框</b></td><td>打开日志筛选栏、导出或清空日志</td></tr>"
        "<tr><td><b>F1</b></td><td>显示本帮助</td></tr>"
        "</table>"
        "<p style='margin-top:8px;color:#596370;'>长时间任务进行期间导航被锁定，切换类快捷键暂时无效。</p>");
}

void MainWindow::showShortcutHelp()
{
    QMessageBox box(QMessageBox::Information, QStringLiteral("操作帮助"),
                    shortcutHelpText(), QMessageBox::Ok, this);
    box.setTextFormat(Qt::RichText);
    box.exec();
}

int MainWindow::toolCount() const
{
    return m_host ? m_host->count() : 0;
}

int MainWindow::currentToolIndex() const
{
    return m_host ? m_host->currentIndex() : -1;
}

QString MainWindow::currentToolId() const
{
    const int index = currentToolIndex();
    return index >= 0 ? m_host->info(index).id : QString();
}

ToolView *MainWindow::currentView() const
{
    return m_host ? m_host->view(currentToolIndex()) : nullptr;
}

bool MainWindow::navigationLocked() const
{
    return m_platform->gate()->isLocked();
}

bool MainWindow::selectTool(int index)
{
    if (!m_host || index < 0 || index >= m_host->count() || navigationLocked())
        return false;
    showToolHub(false);
    if (index != m_host->currentIndex())
        m_host->activate(index);
    syncRailSelection();
    refreshChrome();
    return true;
}

bool MainWindow::selectToolById(const QString &toolId)
{
    const int index = m_registry.indexOf(toolId);
    return index >= 0 && selectTool(index);
}

bool MainWindow::stepTool(int delta)
{
    const int target = currentToolIndex() + delta;
    if (delta == 0 || target < 0 || target >= toolCount())
        return false;
    return selectTool(target);
}

bool MainWindow::selectPage(int index)
{
    ToolView *view = currentView();
    if (!view || navigationLocked())
        return false;
    return view->setCurrentPage(index);
}

bool MainWindow::selectPageById(const QString &pageId)
{
    ToolView *view = currentView();
    if (!view)
        return false;
    return selectPage(view->indexOfPage(pageId));
}

bool MainWindow::stepPage(int delta)
{
    ToolView *view = currentView();
    if (!view || navigationLocked())
        return false;
    return view->stepPage(delta);
}

void MainWindow::onRailRowChanged(int row)
{
    if (m_syncingRail || row < 0)
        return;
    if (!selectTool(row))
        syncRailSelection();
}

void MainWindow::syncRailSelection()
{
    const int index = currentToolIndex();
    if (index < 0)
        return;
    if (m_rail->currentRow() != index) {
        m_syncingRail = true;
        m_rail->setCurrentRow(index);
        m_syncingRail = false;
    }
    if (QListWidgetItem *it = m_rail->item(index))
        m_rail->scrollToItem(it, QAbstractItemView::EnsureVisible);
}

void MainWindow::onViewCreated(int index)
{
    ToolView *view = m_host->view(index);
    if (!view)
        return;
    const QString toolId = m_host->info(index).id;
    const QString savedPage = m_savedToolPages.value(toolId);
    if (!savedPage.isEmpty()) {
        const int pageIdx = view->indexOfPage(savedPage);
        if (pageIdx >= 0)
            view->setCurrentPage(pageIdx);
    }
    connect(view, &ToolView::currentPageChanged, this, [this, view, toolId] {
        const int p = view->currentPage();
        if (p >= 0)
            m_savedToolPages.insert(toolId, view->pageInfo(p).id);
        if (view == currentView())
            refreshChrome();
    });
    view->setNavigationEnabled(!navigationLocked());
}

void MainWindow::refreshChrome()
{
    const int index = currentToolIndex();
    const QString host = QSysInfo::machineHostName();
    const QString hostSuffix = host.isEmpty() ? QString() : QStringLiteral("@%1").arg(host);

    if (index < 0) {
        m_crumbTool->setText(QStringLiteral("无工具"));
        m_crumbPage->clear();
        setWindowTitle(AppInfo::productName() + hostSuffix);
        refreshStatus();
        return;
    }

    const ToolInfo info = m_host->info(index);
    QString pageTitle;
    if (ToolView *view = currentView())
        pageTitle = view->pageInfo(view->currentPage()).title;

    m_crumbTool->setText(info.title);
    m_crumbPage->setText(pageTitle);

    // 标题栏贴合参考图风格：滕王阁@主机名 — 工具名 · 页面名
    setWindowTitle((pageTitle.isEmpty() || pageTitle == info.title)
                       ? QStringLiteral("%1%2 — %3").arg(AppInfo::productName(), hostSuffix, info.title)
                       : QStringLiteral("%1%2 — %3 · %4")
                             .arg(AppInfo::productName(), hostSuffix, info.title, pageTitle));

    m_log->setCurrentTool(info.id, info.title);
    refreshStatus();
}

void MainWindow::refreshStatus()
{
    const int index = currentToolIndex();
    if (index >= 0 && m_host->state(index) == ToolHost::State::Failed) {
        m_status->setStatus({StatusState::Error,
                             QStringLiteral("“%1”不可用：%2")
                                 .arg(m_host->info(index).title, m_host->error(index))});
        return;
    }
    m_status->setStatus(m_platform->status()->current(currentToolId()));
}

void MainWindow::refreshNavigationLock()
{
    if (m_tornDown)
        return;
    NavigationGate *gate = m_platform->gate();
    const bool locked = gate->isLocked();

    if (locked && m_rail->isEnabled()) {
        QWidget *focused = QApplication::focusWidget();
        const bool inNavigation = focused
            && (focused == m_rail
                || (currentView() && focused == currentView()->tabBar()));
        m_focusBeforeLock = inNavigation ? focused : nullptr;
    }

    m_rail->setEnabled(!locked);
    if (m_hubBtn)
        m_hubBtn->setEnabled(!locked);
    if (m_moreToolsBtn)
        m_moreToolsBtn->setEnabled(!locked);
    if (m_quickSearchBtn)
        m_quickSearchBtn->setEnabled(!locked);
    for (int i = 0; i < toolCount(); ++i) {
        if (ToolView *view = m_host->view(i))
            view->setNavigationEnabled(!locked);
    }
    m_status->setLocks(gate->activeHolds());

    if (!locked && m_focusBeforeLock) {
        m_focusBeforeLock->setFocus(Qt::OtherFocusReason);
        m_focusBeforeLock.clear();
    }
}

void MainWindow::cycleFocus()
{
    QVector<QWidget *> zones;
    auto addZone = [this, &zones](QWidget *w) {
        if (w && w->isEnabled() && w->isVisibleTo(this))
            zones.append(w);
    };

    addZone(m_rail);
    if (ToolView *view = currentView()) {
        addZone(view->tabBar());
        addZone(firstFocusable(view->pageWidget(view->currentPage())));
    } else {
        addZone(firstFocusable(m_stack->currentWidget()));
    }
    if (m_log->isVisibleTo(this))
        addZone(m_log->view());
    addZone(m_status->logToggle());

    if (zones.isEmpty())
        return;

    ToolView *view = currentView();
    QWidget *pageRoot = view ? view->pageWidget(view->currentPage()) : m_stack->currentWidget();
    QWidget *focused = QApplication::focusWidget();
    int current = -1;
    for (int i = 0; i < zones.size() && focused; ++i) {
        QWidget *zone = zones.at(i);
        const bool isPageZone = pageRoot && pageRoot->isAncestorOf(zone);
        const bool inside = focused == zone
            || (isPageZone ? pageRoot->isAncestorOf(focused) || focused == pageRoot
                           : zone->isAncestorOf(focused));
        if (inside) {
            current = i;
            break;
        }
    }
    zones.at((current + 1) % zones.size())->setFocus(Qt::TabFocusReason);
}

void MainWindow::setLogPanelVisible(bool visible)
{
    if (isLogPanelVisible() != visible) {
        if (visible) {
            m_log->show();
            const int total = m_splitter->height();
            if (total > 0) {
                const int fallback = qBound(96, total * 18 / 100, 128);
                const int logHeight = m_preferredLogHeight > 0
                    ? qBound(80, m_preferredLogHeight, qMax(100, total - 200))
                    : fallback;
                m_splitter->setSizes({qMax(200, total - logHeight), logHeight});
            }
        } else {
            const QList<int> sizes = m_splitter->sizes();
            if (sizes.size() >= 2 && sizes.at(1) >= 70)
                m_preferredLogHeight = sizes.at(1);
            m_log->hide();
        }
    }
    QPushButton *toggle = m_status->logToggle();
    if (toggle->isChecked() != visible) {
        const bool blocked = toggle->blockSignals(true);
        toggle->setChecked(visible);
        toggle->blockSignals(blocked);
    }
    if (m_topLogBtn && m_topLogBtn->isChecked() != visible) {
        const bool blocked = m_topLogBtn->blockSignals(true);
        m_topLogBtn->setChecked(visible);
        m_topLogBtn->blockSignals(blocked);
    }
}

bool MainWindow::isLogPanelVisible() const
{
    return !m_log->isHidden();
}

void MainWindow::showToolHub(bool visible)
{
    if (visible && navigationLocked()) {
        if (m_hubBtn)
            m_hubBtn->setChecked(false);
        return;
    }
    if (!m_bodyStack || !m_hubView)
        return;
    m_bodyStack->setCurrentWidget(visible ? static_cast<QWidget *>(m_hubView)
                                          : static_cast<QWidget *>(m_stack));
    if (m_hubBtn) {
        const QSignalBlocker blocker(m_hubBtn);
        m_hubBtn->setChecked(visible);
    }
    if (visible)
        m_hubView->focusSearch();
}

bool MainWindow::isToolHubVisible() const
{
    return m_bodyStack && m_bodyStack->currentWidget() == m_hubView;
}

void MainWindow::openQuickLauncher()
{
    if (navigationLocked() || !m_host || m_host->count() == 0)
        return;
    QVector<ToolInfo> infos;
    QVector<bool> avail;
    infos.reserve(m_host->count());
    avail.reserve(m_host->count());
    for (int i = 0; i < m_host->count(); ++i) {
        infos.append(m_host->info(i));
        avail.append(m_host->state(i) != ToolHost::State::Failed);
    }
    QuickLauncherDialog dlg(this);
    dlg.setTools(infos, avail);
    if (dlg.exec() == QDialog::Accepted && dlg.selectedToolIndex() >= 0)
        selectTool(dlg.selectedToolIndex());
}

void MainWindow::showMoreToolsMenu()
{
    if (navigationLocked() || !m_host || m_host->count() == 0)
        return;
    const Theme &theme = Theme::workbench();
    QMenu menu(this);
    QMap<QString, QMenu *> catMenus;
    QStringList catOrder;
    for (int i = 0; i < m_host->count(); ++i) {
        const QString cat = m_host->info(i).effectiveCategory();
        if (!catOrder.contains(cat))
            catOrder.append(cat);
    }
    for (const QString &cat : catOrder)
        catMenus.insert(cat, menu.addMenu(cat));

    for (int i = 0; i < m_host->count(); ++i) {
        const ToolInfo info = m_host->info(i);
        const bool ok = (m_host->state(i) != ToolHost::State::Failed);
        QMenu *sub = catMenus.value(info.effectiveCategory());
        QAction *act = sub->addAction(
            ok ? IconFactory::icon(info.iconKey, theme.primary, theme.primary, theme.disabledText, theme.accent)
               : IconFactory::icon(info.iconKey, theme.disabledText, theme.disabledText, theme.disabledText, theme.disabledText),
            ok ? info.title : QStringLiteral("%1（不可用）").arg(info.title));
        act->setCheckable(true);
        act->setChecked(i == currentToolIndex() && !isToolHubVisible());
        connect(act, &QAction::triggered, this, [this, i]() { selectTool(i); });
    }
    menu.addSeparator();
    QAction *openHub = menu.addAction(QStringLiteral("打开工具中心 (Ctrl+0)"));
    connect(openHub, &QAction::triggered, this, [this]() { showToolHub(true); });
    menu.exec(m_moreToolsBtn->mapToGlobal(QPoint(0, m_moreToolsBtn->height())));
}

SessionState MainWindow::captureSessionState() const
{
    SessionState state;
    state.windowSize = size();
    state.logVisible = isLogPanelVisible();
    if (isLogPanelVisible()) {
        const QList<int> sizes = m_splitter->sizes();
        if (sizes.size() >= 2 && sizes.at(1) > 0)
            state.logHeight = sizes.at(1);
    } else if (m_preferredLogHeight > 0) {
        state.logHeight = m_preferredLogHeight;
    }

    const int cur = currentToolIndex();
    if (cur >= 0 && m_host->state(cur) == ToolHost::State::Ready)
        state.currentToolId = m_host->info(cur).id;

    state.toolPages = m_savedToolPages;
    for (int i = 0; i < toolCount(); ++i) {
        if (const ToolView *view = m_host->view(i)) {
            const int pageIdx = view->currentPage();
            if (pageIdx >= 0)
                state.toolPages.insert(m_host->info(i).id, view->pageInfo(pageIdx).id);
        }
    }
    return state;
}

void MainWindow::applySessionState(const SessionState &state, bool restoreGeometry, bool restoreTool)
{
    for (auto it = state.toolPages.constBegin(); it != state.toolPages.constEnd(); ++it)
        m_savedToolPages.insert(it.key(), it.value());

    if (restoreGeometry && state.windowSize.isValid())
        resize(state.windowSize);

    if (state.logHeight > 0)
        m_preferredLogHeight = state.logHeight;

    if (restoreTool && !state.currentToolId.isEmpty())
        selectToolById(state.currentToolId);

    if (currentView()) {
        const QString savedPage = m_savedToolPages.value(currentToolId());
        if (!savedPage.isEmpty())
            selectPageById(savedPage);
    }

    setLogPanelVisible(state.logVisible);
}

bool MainWindow::loadSessionState(bool restoreGeometry, bool restoreTool)
{
    const ShellConfig config(m_platform->paths());
    if (!config.exists())
        return false;
    applySessionState(config.load(), restoreGeometry, restoreTool);
    return true;
}

bool MainWindow::saveSessionState(QString *error) const
{
    const ShellConfig config(m_platform->paths());
    return config.save(captureSessionState(), error);
}

void MainWindow::setCloseGuard(std::function<bool(const QString &)> guard)
{
    m_closeGuard = std::move(guard);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (navigationLocked()) {
        const QString reason = m_platform->gate()->reason();
        bool allow = false;
        if (m_closeGuard) {
            allow = m_closeGuard(reason);
        } else {
            const QVector<NavigationGate::HoldInfo> holds = m_platform->gate()->activeHolds();
            QString detailText;
            if (holds.size() <= 1) {
                detailText = QStringLiteral("当前有任务正在进行（%1）。\n现在退出可能使任务中断，确定退出吗？")
                                 .arg(reason);
            } else {
                QStringList items;
                for (const NavigationGate::HoldInfo &h : holds)
                    items.append(QStringLiteral("• %1").arg(h.reason));
                detailText = QStringLiteral("当前有 %1 项任务正在进行：\n%2\n\n现在退出可能使任务中断，确定退出吗？")
                                 .arg(holds.size())
                                 .arg(items.join(QLatin1Char('\n')));
            }
            const auto answer = QMessageBox::question(
                this, QStringLiteral("任务仍在进行"), detailText,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            allow = answer == QMessageBox::Yes;
        }
        if (!allow) {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

} // namespace tw
