// 外壳与内置工具的 offscreen UI 测试。
// 运行：QT_QPA_PLATFORM=offscreen ctest -R ui_tests
// 设置 TENGWANGGE_SCREENSHOT_DIR 可在关键状态保存截图，便于人工检查。

#include "core/app_info.h"
#include "core/platform_services.h"
#include "shell/elided_label.h"
#include "shell/icon_factory.h"
#include "shell/log_panel.h"
#include "shell/main_window.h"
#include "shell/status_strip.h"
#include "shell/theme.h"
#include "shell/tool_host.h"
#include "shell/tool_rail.h"
#include "shell/tool_registry.h"
#include "shell/tool_view.h"
#include "tools/builtin_tools.h"
#include "tools/task_demo/task_demo_tool.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include <memory>
#include <stdexcept>

using namespace tw;

namespace {

class FakeTool : public ITool
{
public:
    struct Spec
    {
        Spec(QString id_, QString title_, int order_ = 100, int pageCount_ = 2)
            : id(std::move(id_))
            , title(std::move(title_))
            , order(order_)
            , pageCount(pageCount_)
        {
        }

        QString id;
        QString title;
        int order = 100;
        int pageCount = 2;
        bool failInit = false;
        bool throwInit = false;
        QString pageProblem; // "empty" "dup" "null" "throw" "badid"
    };

    explicit FakeTool(Spec s)
        : spec(std::move(s))
    {
    }

    ToolInfo info() const override
    {
        ToolInfo i;
        i.id = spec.id;
        i.title = spec.title;
        i.summary = spec.title + QStringLiteral("摘要");
        i.iconKey = QStringLiteral("info");
        i.order = spec.order;
        return i;
    }

    bool initialize(const ToolContext &context, QString *error) override
    {
        ++initCalls;
        services = context.services;
        dialogParent = context.dialogParent;
        if (spec.throwInit)
            throw std::runtime_error("boom");
        if (spec.failInit) {
            *error = QStringLiteral("模拟的初始化失败");
            return false;
        }
        return true;
    }

    QVector<ToolPage> createPages() override
    {
        ++createCalls;
        if (spec.pageProblem == QLatin1String("throw"))
            throw std::runtime_error("页面构造异常");
        QVector<ToolPage> pages;
        if (spec.pageProblem == QLatin1String("empty"))
            return pages;
        for (int i = 0; i < spec.pageCount; ++i) {
            auto *w = new QWidget;
            w->setObjectName(QStringLiteral("page-%1-%2").arg(spec.id).arg(i));
            auto *layout = new QVBoxLayout(w);
            auto *edit = new QLineEdit;
            edit->setObjectName(QStringLiteral("edit-%1-%2").arg(spec.id).arg(i));
            layout->addWidget(edit);
            layout->addStretch(1);
            QString id = QStringLiteral("p%1").arg(i);
            if (spec.pageProblem == QLatin1String("dup"))
                id = QStringLiteral("same");
            if (spec.pageProblem == QLatin1String("badid") && i == 0)
                id = QStringLiteral("Bad Id");
            pages.append({PageInfo{id, QStringLiteral("页面%1").arg(i + 1), QString(), QString()}, w});
        }
        if (spec.pageProblem == QLatin1String("null"))
            pages.append({PageInfo{QStringLiteral("nullpage"), QStringLiteral("空"), QString(), QString()},
                          nullptr});
        if (spec.pageProblem == QLatin1String("dupwidget") && !pages.isEmpty())
            pages.append({PageInfo{QStringLiteral("dupwidget"), QStringLiteral("复用"), QString(), QString()},
                          pages.first().widget});
        return pages;
    }

    void activated() override { ++activatedCalls; }
    void deactivated() override { ++deactivatedCalls; }
    void shutdown() override
    {
        ++shutdownCalls;
        lock.release();
    }

    Spec spec;
    ToolServices services;
    NavigationLock lock;
    QPointer<QWidget> dialogParent;
    int initCalls = 0;
    int createCalls = 0;
    int activatedCalls = 0;
    int deactivatedCalls = 0;
    int shutdownCalls = 0;
};

struct Fixture
{
    QTemporaryDir dir;
    PlatformServices *platform = nullptr; // 由窗口持有
    std::unique_ptr<MainWindow> window;
    QVector<FakeTool *> tools;

    explicit Fixture(const QVector<FakeTool::Spec> &specs)
    {
        auto owned = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/data")));
        platform = owned.get();
        ToolRegistry registry;
        for (const FakeTool::Spec &spec : specs) {
            auto tool = std::make_unique<FakeTool>(spec);
            tools.append(tool.get());
            QString error;
            if (!registry.add(std::move(tool), &error))
                qFatal("registry.add failed: %s", qPrintable(error));
        }
        window = std::make_unique<MainWindow>(std::move(owned), std::move(registry));
    }

    void show()
    {
        window->show();
        QVERIFY2(QTest::qWaitForWindowExposed(window.get()), "window not exposed");
        window->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window.get()));
    }
};

QVector<FakeTool::Spec> threeTools()
{
    return {{QStringLiteral("alpha"), QStringLiteral("甲工具")},
            {QStringLiteral("beta"), QStringLiteral("乙工具")},
            {QStringLiteral("gamma"), QStringLiteral("丙工具"), 100, 1}};
}

void shot(QWidget *w, const QString &name)
{
    const QString dir = qEnvironmentVariable("TENGWANGGE_SCREENSHOT_DIR");
    if (dir.isEmpty())
        return;
    QApplication::processEvents();
    w->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"), "PNG");
}

} // namespace

class UiTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<tw::LogEntry>("tw::LogEntry"); }

    // ---- 注册表 ----
    void registry_ordersByOrderThenInsertion();
    void registry_rejectsBadTools();

    // ---- ToolHost 生命周期 ----
    void host_pagesAreLazyAndCreatedOnce();
    void host_lifecycleCallsAreBalanced();
    void host_failedInitOnlyAffectsThatTool();
    void host_throwingInitIsContained();
    void host_invalidPagesMarkToolUnavailable();
    void host_failedPagesStillShutDownInitializedTool();
    void host_shutdownIsIdempotentAndReleasesLocks();

    // ---- 主窗口：两层导航 ----
    void window_selectsFirstToolAndSyncsRail();
    void window_initialToolIdSkipsDefaultToolCreation();
    void window_railClickAndKeyboardSwitchTools();
    void window_pageSelectionIsPerTool();
    void window_breadcrumbAndTitleFollowSelection();
    void window_unavailableToolShowsReasonAndIsSelectable();
    void window_emptyRegistryDoesNotCrash();

    // ---- 导航锁 ----
    void lock_blocksToolAndPageNavigation();
    void lock_isRefCountedAcrossTools();
    void lock_blocksShortcutsAndRestoresFocus();
    void lock_closeGuardIsConsulted();
    void lock_destroyingWindowWithLockedToolIsSafe();

    // ---- 状态栏 / 日志 ----
    void status_isPerToolAndFollowsSelection();
    void status_infoReturnsToIdle();
    void status_longTextDoesNotWidenWindow();
    void log_filtersByCurrentToolAndShowAll();
    void log_clearAndToggle();

    // ---- 键盘 ----
    void keyboard_toolAndPageShortcuts();
    void keyboard_f6CyclesFocusZones();

    // ---- 主题 ----
    void theme_stylesheetIsFullyExpanded();
    void theme_iconsRenderForAllKnownKeys();
    void theme_elidedLabelNeverForcesWidth();

    // ---- 内置工具 ----
    void builtin_toolsRegisterAndCreatePages();
    void builtin_taskDemoLocksAndReportsStatus();
    void builtin_taskDemoFailureAndCancel();
    void builtin_taskDemoShutdownMidRunReleasesLock();
};

// ---------------------------------------------------------------------------

void UiTests::registry_ordersByOrderThenInsertion()
{
    ToolRegistry registry;
    QVERIFY(registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"c", "C", 50})));
    QVERIFY(registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"a", "A", 10})));
    QVERIFY(registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"b", "B", 50})));
    QCOMPARE(registry.count(), 3);
    QCOMPARE(registry.infoAt(0).id, QStringLiteral("a"));
    QCOMPARE(registry.infoAt(1).id, QStringLiteral("c"));
    QCOMPARE(registry.infoAt(2).id, QStringLiteral("b"));
    QCOMPARE(registry.indexOf(QStringLiteral("b")), 2);
    QCOMPARE(registry.indexOf(QStringLiteral("zzz")), -1);
    QVERIFY(registry.find(QStringLiteral("a")) != nullptr);
    QVERIFY(registry.at(7) == nullptr);
}

void UiTests::registry_rejectsBadTools()
{
    ToolRegistry registry;
    QString error;
    QVERIFY(!registry.add(nullptr, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"Bad Id", "标题"}), &error));
    QVERIFY(!registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"ok", "  "}), &error));
    QVERIFY(registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"ok", "标题"}), &error));
    error.clear();
    QVERIFY(!registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"ok", "另一个"}), &error));
    QVERIFY(error.contains(QStringLiteral("ok")));
    QCOMPARE(registry.count(), 1);
}

// ---------------------------------------------------------------------------

void UiTests::host_pagesAreLazyAndCreatedOnce()
{
    Fixture f(threeTools());
    // 窗口构造后只激活了第一个工具。
    QCOMPARE(f.tools[0]->createCalls, 1);
    QCOMPARE(f.tools[1]->createCalls, 0);
    QCOMPARE(f.tools[2]->createCalls, 0);
    QCOMPARE(f.tools[1]->initCalls, 1); // initialize 是急切的

    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.tools[1]->createCalls, 1);
    ToolView *beta = f.window->currentView();
    QVERIFY(beta);

    QVERIFY(f.window->selectTool(0));
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.tools[0]->createCalls, 1);
    QCOMPARE(f.tools[1]->createCalls, 1);
    QCOMPARE(f.window->currentView(), beta); // 同一个视图，没有重建
}

void UiTests::host_lifecycleCallsAreBalanced()
{
    Fixture f(threeTools());
    QCOMPARE(f.tools[0]->activatedCalls, 1);
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.tools[0]->deactivatedCalls, 1);
    QCOMPARE(f.tools[1]->activatedCalls, 1);
    QVERIFY(f.window->selectTool(1)); // 重复选择不重复通知
    QCOMPARE(f.tools[1]->activatedCalls, 1);
    QVERIFY(f.window->selectTool(0));
    QCOMPARE(f.tools[0]->activatedCalls, 2);
    QCOMPARE(f.tools[1]->deactivatedCalls, 1);
}

void UiTests::host_failedInitOnlyAffectsThatTool()
{
    QVector<FakeTool::Spec> specs = threeTools();
    specs[0].failInit = true;
    Fixture f(specs);

    ToolHost *host = f.window->toolHost();
    QCOMPARE(host->state(0), ToolHost::State::Failed);
    QCOMPARE(host->state(1), ToolHost::State::Ready);
    QVERIFY(host->error(0).contains(QStringLiteral("模拟的初始化失败")));
    QVERIFY(!f.window->toolRail()->isAvailable(0));
    QVERIFY(f.window->toolRail()->isAvailable(1));

    // 窗口跳过不可用的工具，选中第一个可用工具。
    QCOMPARE(f.window->currentToolIndex(), 1);
    QCOMPARE(f.tools[0]->createCalls, 0);
    QCOMPARE(f.tools[0]->activatedCalls, 0);
    QVERIFY(f.platform->log()->entries(QString(), true).size() >= 1);
}

void UiTests::host_throwingInitIsContained()
{
    QVector<FakeTool::Spec> specs = threeTools();
    specs[1].throwInit = true;
    Fixture f(specs);
    QCOMPARE(f.window->toolHost()->state(1), ToolHost::State::Failed);
    QVERIFY(f.window->toolHost()->error(1).contains(QStringLiteral("boom")));
    QCOMPARE(f.window->toolHost()->state(0), ToolHost::State::Ready);
    QCOMPARE(f.window->toolHost()->state(2), ToolHost::State::Ready);
}

void UiTests::host_invalidPagesMarkToolUnavailable()
{
    for (const QString &problem : {QStringLiteral("empty"), QStringLiteral("dup"), QStringLiteral("null"),
                                   QStringLiteral("throw"), QStringLiteral("badid"),
                                   QStringLiteral("dupwidget")}) {
        QVector<FakeTool::Spec> specs = threeTools();
        specs[1].pageProblem = problem;
        Fixture f(specs);
        QVERIFY2(f.window->selectTool(1), qPrintable(problem));
        QCOMPARE(f.window->toolHost()->state(1), ToolHost::State::Failed);
        QVERIFY2(f.window->currentView() == nullptr, qPrintable(problem));
        QVERIFY(!f.window->toolRail()->isAvailable(1));
        QCOMPARE(f.tools[1]->activatedCalls, 0);
        // 其他工具照常工作
        QVERIFY(f.window->selectTool(2));
        QVERIFY(f.window->currentView() != nullptr);
    }
}

void UiTests::host_failedPagesStillShutDownInitializedTool()
{
    QVector<FakeTool::Spec> specs = threeTools();
    specs[1].pageProblem = QStringLiteral("empty");
    Fixture f(specs);
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.window->toolHost()->state(1), ToolHost::State::Failed);
    f.window->toolHost()->shutdown();
    QCOMPARE(f.tools[1]->initCalls, 1);
    QCOMPARE(f.tools[1]->shutdownCalls, 1);
}

void UiTests::host_shutdownIsIdempotentAndReleasesLocks()
{
    Fixture f(threeTools());
    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QStringLiteral("测试"));
    QVERIFY(f.window->navigationLocked());
    f.window->toolHost()->shutdown();
    f.window->toolHost()->shutdown();
    QCOMPARE(f.tools[0]->shutdownCalls, 1);
    QCOMPARE(f.tools[1]->shutdownCalls, 1);
    QVERIFY(!f.window->navigationLocked());
}

// ---------------------------------------------------------------------------

void UiTests::window_selectsFirstToolAndSyncsRail()
{
    Fixture f(threeTools());
    QCOMPARE(f.window->toolCount(), 3);
    QCOMPARE(f.window->currentToolIndex(), 0);
    QCOMPARE(f.window->currentToolId(), QStringLiteral("alpha"));
    QCOMPARE(f.window->toolRail()->currentRow(), 0);
    QCOMPARE(f.window->toolRail()->count(), 3);
    QVERIFY(f.window->selectToolById(QStringLiteral("gamma")));
    QCOMPARE(f.window->toolRail()->currentRow(), 2);
    QVERIFY(!f.window->selectToolById(QStringLiteral("nope")));
    QVERIFY(!f.window->selectTool(9));
    QVERIFY(!f.window->selectTool(-1));
    QCOMPARE(f.window->currentToolIndex(), 2);
}

void UiTests::window_initialToolIdSkipsDefaultToolCreation()
{
    QTemporaryDir dir;
    auto owned = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/data")));
    ToolRegistry registry;
    QVector<FakeTool *> tools;
    for (const FakeTool::Spec &spec : threeTools()) {
        auto tool = std::make_unique<FakeTool>(spec);
        tools.append(tool.get());
        QVERIFY(registry.add(std::move(tool)));
    }
    MainWindow window(std::move(owned), std::move(registry), nullptr, QStringLiteral("beta"));
    QCOMPARE(window.currentToolIndex(), 1);
    QCOMPARE(window.currentToolId(), QStringLiteral("beta"));
    QCOMPARE(tools[0]->createCalls, 0);
    QCOMPARE(tools[0]->activatedCalls, 0);
    QCOMPARE(tools[1]->createCalls, 1);
    QCOMPARE(tools[1]->activatedCalls, 1);
}

void UiTests::window_railClickAndKeyboardSwitchTools()
{
    Fixture f(threeTools());
    f.show();
    ToolRail *rail = f.window->toolRail();
    rail->setFocus();
    QTest::keyClick(rail, Qt::Key_Down);
    QCOMPARE(f.window->currentToolIndex(), 1);
    QTest::keyClick(rail, Qt::Key_Down);
    QCOMPARE(f.window->currentToolIndex(), 2);
    QTest::keyClick(rail, Qt::Key_Down); // 到底不循环
    QCOMPARE(f.window->currentToolIndex(), 2);
    QTest::keyClick(rail, Qt::Key_Up);
    QCOMPARE(f.window->currentToolIndex(), 1);

    const QRect r = rail->visualItemRect(rail->item(0));
    QTest::mouseClick(rail->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
    QCOMPARE(f.window->currentToolIndex(), 0);
    QCOMPARE(f.window->toolStack()->currentWidget(), f.window->currentView());
    shot(f.window.get(), QStringLiteral("ui-rail-focus"));
}

void UiTests::window_pageSelectionIsPerTool()
{
    Fixture f(threeTools());
    QVERIFY(f.window->selectPage(1));
    QCOMPARE(f.window->currentView()->currentPage(), 1);
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.window->currentView()->currentPage(), 0);
    QVERIFY(f.window->selectPageById(QStringLiteral("p1")));
    QVERIFY(!f.window->selectPageById(QStringLiteral("none")));
    QVERIFY(f.window->selectTool(0));
    QCOMPARE(f.window->currentView()->currentPage(), 1);
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.window->currentView()->currentPage(), 1);

    QVERIFY(!f.window->selectPage(5));
    QVERIFY(!f.window->stepPage(+1)); // 已经是最后一页
    QVERIFY(f.window->stepPage(-1));
    QVERIFY(!f.window->stepPage(-1)); // 第一页
    QCOMPARE(f.window->currentView()->currentPage(), 0);
}

void UiTests::window_breadcrumbAndTitleFollowSelection()
{
    Fixture f(threeTools());
    auto *tool = f.window->findChild<QLabel *>(QStringLiteral("breadcrumbTool"));
    auto *page = f.window->findChild<QLabel *>(QStringLiteral("breadcrumbPage"));
    QVERIFY(tool && page);
    QCOMPARE(tool->text(), QStringLiteral("甲工具"));
    QCOMPARE(page->text(), QStringLiteral("页面1"));
    QVERIFY(f.window->windowTitle().contains(QStringLiteral("甲工具")));

    f.window->selectPage(1);
    QCOMPARE(page->text(), QStringLiteral("页面2"));
    QVERIFY(f.window->windowTitle().contains(QStringLiteral("页面2")));

    f.window->selectTool(1);
    QCOMPARE(tool->text(), QStringLiteral("乙工具"));
    QCOMPARE(page->text(), QStringLiteral("页面1"));
}

void UiTests::window_unavailableToolShowsReasonAndIsSelectable()
{
    QVector<FakeTool::Spec> specs = threeTools();
    specs[1].failInit = true;
    Fixture f(specs);
    f.show();
    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.window->currentToolIndex(), 1);
    QCOMPARE(f.window->currentView(), static_cast<ToolView *>(nullptr));
    auto *view = f.window->toolStack()->currentWidget();
    QVERIFY(view);
    QCOMPARE(view->objectName(), QStringLiteral("unavailableView"));
    auto *reason = view->findChild<QLabel *>(QStringLiteral("unavailableReason"));
    QVERIFY(reason && reason->text().contains(QStringLiteral("模拟的初始化失败")));
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Error);
    QCOMPARE(f.tools[1]->activatedCalls, 0);
    shot(f.window.get(), QStringLiteral("ui-unavailable"));

    // 回到可用工具后状态栏恢复
    QVERIFY(f.window->selectTool(0));
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Idle);
}

void UiTests::window_emptyRegistryDoesNotCrash()
{
    QTemporaryDir dir;
    auto platform = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/d")));
    MainWindow window(std::move(platform), ToolRegistry());
    window.show();
    QCOMPARE(window.toolCount(), 0);
    QCOMPARE(window.currentToolIndex(), -1);
    QVERIFY(!window.selectTool(0));
    QVERIFY(!window.stepTool(1));
    QVERIFY(!window.stepPage(1));
    window.cycleFocus();
    shot(&window, QStringLiteral("ui-empty"));
}

// ---------------------------------------------------------------------------

void UiTests::lock_blocksToolAndPageNavigation()
{
    Fixture f(threeTools());
    f.show();
    QVERIFY(f.window->toolRail()->isEnabled());
    QVERIFY(f.window->currentView()->tabBar()->isEnabled());

    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QStringLiteral("正在部署"));
    QVERIFY(f.window->navigationLocked());
    QVERIFY(!f.window->toolRail()->isEnabled());
    QVERIFY(!f.window->currentView()->tabBar()->isEnabled());
    QVERIFY(f.window->statusStrip()->isLockShown());
    QVERIFY(f.window->statusStrip()->lockText().contains(QStringLiteral("正在部署")));
    QApplication::processEvents();
    auto *chip = f.window->statusStrip()->findChild<QLabel *>(QStringLiteral("statusLockChip"));
    QVERIFY(chip);
    QVERIFY(chip->isVisible());
    QVERIFY(chip->width() > 40);

    QVERIFY(!f.window->selectTool(1));
    QVERIFY(!f.window->stepTool(1));
    QVERIFY(!f.window->selectPage(1));
    QVERIFY(!f.window->stepPage(1));
    QCOMPARE(f.window->currentToolIndex(), 0);
    QCOMPARE(f.window->toolRail()->currentRow(), 0); // 选中位置仍然可辨认
    shot(f.window.get(), QStringLiteral("ui-locked"));

    f.tools[0]->lock.release();
    QVERIFY(!f.window->navigationLocked());
    QVERIFY(f.window->toolRail()->isEnabled());
    QVERIFY(f.window->currentView()->tabBar()->isEnabled());
    QVERIFY(!f.window->statusStrip()->isLockShown());
    QVERIFY(f.window->selectTool(1));
    QVERIFY(f.window->currentView()->tabBar()->isEnabled()); // 新视图同样恢复
}

void UiTests::lock_isRefCountedAcrossTools()
{
    Fixture f(threeTools());
    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QStringLiteral("任务一"));
    f.tools[1]->lock = f.tools[1]->services.lockNavigation(QStringLiteral("任务二"));
    QCOMPARE(f.platform->gate()->holdCount(), 2);
    f.tools[1]->lock.release();
    QVERIFY(f.window->navigationLocked());
    QVERIFY(!f.window->selectTool(2));
    QVERIFY(f.window->statusStrip()->lockText().contains(QStringLiteral("任务一")));
    f.tools[0]->lock.release();
    QVERIFY(f.window->selectTool(2));
}

void UiTests::lock_blocksShortcutsAndRestoresFocus()
{
    Fixture f(threeTools());
    f.show();
    f.window->toolRail()->setFocus();
    QTRY_VERIFY(f.window->toolRail()->hasFocus());

    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QStringLiteral("忙"));
    QTest::keyClick(f.window.get(), Qt::Key_2, Qt::ControlModifier);
    QTest::keyClick(f.window.get(), Qt::Key_Down, Qt::AltModifier);
    QTest::keyClick(f.window.get(), Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(f.window->currentToolIndex(), 0);
    QCOMPARE(f.window->currentView()->currentPage(), 0);

    f.tools[0]->lock.release();
    QVERIFY(f.window->toolRail()->isEnabled());
    QVERIFY(f.window->toolRail()->hasFocus()); // 焦点回到锁定前的位置
    QTest::keyClick(f.window.get(), Qt::Key_2, Qt::ControlModifier);
    QCOMPARE(f.window->currentToolIndex(), 1);
}

void UiTests::lock_closeGuardIsConsulted()
{
    Fixture f(threeTools());
    f.show();
    QString seenReason;
    bool allow = false;
    f.window->setCloseGuard([&](const QString &reason) {
        seenReason = reason;
        return allow;
    });

    // 没有锁时不询问
    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QStringLiteral("长任务"));
    QVERIFY(!f.window->close());
    QCOMPARE(seenReason, QStringLiteral("长任务"));
    QVERIFY(f.window->isVisible());

    allow = true;
    QVERIFY(f.window->close());
    QVERIFY(!f.window->isVisible());
}

void UiTests::lock_destroyingWindowWithLockedToolIsSafe()
{
    auto fixture = std::make_unique<Fixture>(threeTools());
    fixture->tools[0]->lock = fixture->tools[0]->services.lockNavigation(QStringLiteral("x"));
    fixture->tools[1]->lock = fixture->tools[1]->services.lockNavigation(QStringLiteral("y"));
    fixture->window.reset(); // 不应崩溃，也不应访问已销毁的平台服务
    QVERIFY(true);
}

// ---------------------------------------------------------------------------

void UiTests::status_isPerToolAndFollowsSelection()
{
    Fixture f(threeTools());
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Idle);

    f.tools[0]->services.showStatus(StatusState::Busy, QStringLiteral("甲在忙"));
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Busy);
    QCOMPARE(f.window->statusStrip()->statusText(), QStringLiteral("甲在忙"));

    // 其他工具写状态不影响当前显示
    f.tools[1]->services.showStatus(StatusState::Error, QStringLiteral("乙出错"));
    QCOMPARE(f.window->statusStrip()->statusText(), QStringLiteral("甲在忙"));

    QVERIFY(f.window->selectTool(1));
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Error);
    QCOMPARE(f.window->statusStrip()->statusText(), QStringLiteral("乙出错"));

    QVERIFY(f.window->selectTool(0));
    QCOMPARE(f.window->statusStrip()->statusText(), QStringLiteral("甲在忙"));

    f.tools[0]->services.clearStatus();
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Idle);
    QCOMPARE(f.window->statusStrip()->statusText(), StatusBoard::idleText());
}

void UiTests::status_infoReturnsToIdle()
{
    Fixture f(threeTools());
    f.tools[0]->services.showStatus(StatusState::Success, QStringLiteral("完成了"), 30);
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Success);
    QTRY_COMPARE_WITH_TIMEOUT(f.window->statusStrip()->status().state, StatusState::Idle, 2000);

    // 进行中状态不会自己消失
    f.tools[0]->services.showStatus(StatusState::Busy, QStringLiteral("还在跑"));
    QTest::qWait(60);
    QCOMPARE(f.window->statusStrip()->status().state, StatusState::Busy);
}

void UiTests::status_longTextDoesNotWidenWindow()
{
    Fixture f(threeTools());
    f.show();
    const int before = f.window->minimumSizeHint().width();
    f.tools[0]->services.showStatus(StatusState::Error, QString(600, QChar(0x9519)));
    f.tools[0]->lock = f.tools[0]->services.lockNavigation(QString(300, QChar(0x9501)));
    QCOMPARE(f.window->minimumSizeHint().width(), before);
    QVERIFY(f.window->width() <= qMax(f.window->minimumWidth(), 1180));
    shot(f.window.get(), QStringLiteral("ui-long-status"));
}

void UiTests::log_filtersByCurrentToolAndShowAll()
{
    Fixture f(threeTools());
    LogPanel *panel = f.window->logPanel();
    f.tools[0]->services.logInfo(QStringLiteral("来自甲"));
    f.tools[1]->services.logWarning(QStringLiteral("来自乙"));

    QVERIFY(panel->plainText().contains(QStringLiteral("来自甲")));
    QVERIFY(!panel->plainText().contains(QStringLiteral("来自乙")));
    QVERIFY(panel->plainText().contains(QStringLiteral("启动"))); // 平台条目始终可见

    QVERIFY(f.window->selectTool(1));
    QVERIFY(panel->plainText().contains(QStringLiteral("来自乙")));
    QVERIFY(!panel->plainText().contains(QStringLiteral("来自甲")));

    panel->setShowsAllTools(true);
    QVERIFY(panel->plainText().contains(QStringLiteral("来自甲")));
    QVERIFY(panel->plainText().contains(QStringLiteral("来自乙")));
    QVERIFY(panel->plainText().contains(QStringLiteral("甲工具")));
    QVERIFY(panel->plainText().contains(QStringLiteral("乙工具")));

    f.tools[2]->services.logError(QStringLiteral("来自丙"));
    QVERIFY(panel->plainText().contains(QStringLiteral("来自丙")));
}

void UiTests::log_clearAndToggle()
{
    Fixture f(threeTools());
    f.show();
    QVERIFY(!f.window->isLogPanelVisible());
    QPushButton *toggle = f.window->statusStrip()->logToggle();
    QVERIFY(!toggle->isChecked());

    toggle->click();
    QVERIFY(f.window->isLogPanelVisible());
    QVERIFY(toggle->isChecked());

    f.window->setLogPanelVisible(false);
    QVERIFY(!f.window->isLogPanelVisible());
    QVERIFY(!toggle->isChecked());

    f.window->setLogPanelVisible(true);
    f.tools[0]->services.logInfo(QStringLiteral("一条日志"));
    QVERIFY(f.window->logPanel()->plainText().contains(QStringLiteral("一条日志")));
    auto *clear = f.window->logPanel()->findChild<QPushButton *>(QStringLiteral("logClear"));
    QVERIFY(clear);
    clear->click();
    QVERIFY(f.window->logPanel()->plainText().isEmpty());
    QCOMPARE(f.platform->log()->size(), 0);

    auto *close = f.window->logPanel()->findChild<QPushButton *>(QStringLiteral("logClose"));
    QVERIFY(close);
    close->click();
    QVERIFY(!f.window->isLogPanelVisible());
    QVERIFY(!toggle->isChecked());
}

// ---------------------------------------------------------------------------

void UiTests::keyboard_toolAndPageShortcuts()
{
    Fixture f(threeTools());
    f.show();
    QTest::keyClick(f.window.get(), Qt::Key_3, Qt::ControlModifier);
    QCOMPARE(f.window->currentToolIndex(), 2);
    QTest::keyClick(f.window.get(), Qt::Key_9, Qt::ControlModifier); // 不存在的第 9 个：忽略
    QCOMPARE(f.window->currentToolIndex(), 2);
    QTest::keyClick(f.window.get(), Qt::Key_Up, Qt::AltModifier);
    QCOMPARE(f.window->currentToolIndex(), 1);
    QTest::keyClick(f.window.get(), Qt::Key_Up, Qt::AltModifier);
    QCOMPARE(f.window->currentToolIndex(), 0);
    QTest::keyClick(f.window.get(), Qt::Key_Up, Qt::AltModifier); // 到头不循环
    QCOMPARE(f.window->currentToolIndex(), 0);

    QTest::keyClick(f.window.get(), Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(f.window->currentView()->currentPage(), 1);
    QTest::keyClick(f.window.get(), Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(f.window->currentView()->currentPage(), 1);
    QTest::keyClick(f.window.get(), Qt::Key_PageUp, Qt::ControlModifier);
    QCOMPARE(f.window->currentView()->currentPage(), 0);

    QTest::keyClick(f.window.get(), Qt::Key_L, Qt::ControlModifier);
    QVERIFY(f.window->isLogPanelVisible());
    QTest::keyClick(f.window.get(), Qt::Key_L, Qt::ControlModifier);
    QVERIFY(!f.window->isLogPanelVisible());
}

void UiTests::keyboard_f6CyclesFocusZones()
{
    Fixture f(threeTools());
    f.show();
    f.window->setLogPanelVisible(true);
    f.window->toolRail()->setFocus();

    ToolView *view = f.window->currentView();
    QWidget *pageEdit = view->pageWidget(0)->findChild<QLineEdit *>();
    QVERIFY(pageEdit);

    f.window->cycleFocus();
    QCOMPARE(QApplication::focusWidget(), view->tabBar());
    shot(f.window.get(), QStringLiteral("ui-tabs-focus"));
    f.window->cycleFocus();
    QCOMPARE(QApplication::focusWidget(), pageEdit);
    f.window->cycleFocus();
    QCOMPARE(QApplication::focusWidget(), f.window->logPanel()->view());
    f.window->cycleFocus();
    QCOMPARE(QApplication::focusWidget(), f.window->statusStrip()->logToggle());
    f.window->cycleFocus();
    QCOMPARE(QApplication::focusWidget(), f.window->toolRail());

    // 页面条上的方向键切页
    f.window->cycleFocus();
    QTest::keyClick(view->tabBar(), Qt::Key_Right);
    QCOMPARE(view->currentPage(), 1);
    QTest::keyClick(view->tabBar(), Qt::Key_Left);
    QCOMPARE(view->currentPage(), 0);
}

// ---------------------------------------------------------------------------

void UiTests::theme_stylesheetIsFullyExpanded()
{
    const QString qss = Theme::workbench().styleSheet();
    QVERIFY2(!qss.isEmpty(), "theme.qss resource missing from static library");
    QVERIFY2(!qss.contains(QLatin1Char('@')), "unexpanded @token@ left in stylesheet");
    QVERIFY(qss.contains(QStringLiteral("#topBar")));
    QVERIFY(qss.contains(QStringLiteral("#pageTabs")));
    QVERIFY(qss.contains(QStringLiteral("#statusStrip")));

    const QMap<QString, QString> tokens{{QStringLiteral("a"), QStringLiteral("#123456")}};
    QCOMPARE(Theme::expand(QStringLiteral("x:@a@;y:@a@"), tokens), QStringLiteral("x:#123456;y:#123456"));
    QCOMPARE(Theme::expand(QStringLiteral("@missing@"), tokens), QStringLiteral("@missing@"));
}

void UiTests::theme_iconsRenderForAllKnownKeys()
{
    for (const QString &key : IconFactory::knownKeys() + QStringList{QString(), QStringLiteral("no-such")}) {
        const QPixmap pixmap = IconFactory::pixmap(key, 16, Qt::black);
        QVERIFY2(!pixmap.isNull(), qPrintable(key));
        const QImage image = pixmap.toImage();
        bool painted = false;
        for (int y = 0; y < image.height() && !painted; ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 0) {
                    painted = true;
                    break;
                }
        QVERIFY2(painted, qPrintable(key));
    }
}

void UiTests::theme_elidedLabelNeverForcesWidth()
{
    ElidedLabel label;
    label.setFullText(QString(500, QChar(0x5b57)));
    QCOMPARE(label.minimumSizeHint().width(), 0);
    QCOMPARE(label.sizeHint().width(), 0);
    label.resize(120, 24);
    label.show();
    QVERIFY(label.text().endsWith(QChar(0x2026)));
    QVERIFY(label.text().size() < 500);
    QCOMPARE(label.toolTip().size(), 500);
    label.setFullText(QStringLiteral("短"));
    QCOMPARE(label.text(), QStringLiteral("短"));
}

// ---------------------------------------------------------------------------

void UiTests::builtin_toolsRegisterAndCreatePages()
{
    QTemporaryDir dir;
    ToolRegistry registry;
    QString error;
    QVERIFY2(registerBuiltinTools(registry, &error), qPrintable(error));
    QVERIFY(registry.count() >= 2);

    auto platform = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/d")));
    PlatformServices *platformPtr = platform.get();
    MainWindow window(std::move(platform), std::move(registry));
    window.resize(1180, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    for (int i = 0; i < window.toolCount(); ++i) {
        QCOMPARE(window.toolHost()->state(i), ToolHost::State::Ready);
        QVERIFY(window.selectTool(i));
        QVERIFY(window.currentView() != nullptr);
        QVERIFY(window.currentView()->pageCount() >= 1);
        const QString id = window.currentToolId();
        shot(&window, QStringLiteral("builtin-%1").arg(id));
    }
    // 每个工具的数据只落在隔离目录之下
    const QString root = platformPtr->paths().root();
    QVERIFY(root.startsWith(dir.path()));
    window.selectToolById(QStringLiteral("welcome"));
    window.selectPageById(QStringLiteral("keys"));
    shot(&window, QStringLiteral("builtin-welcome-keys"));
}

void UiTests::builtin_taskDemoLocksAndReportsStatus()
{
    QTemporaryDir dir;
    ToolRegistry registry;
    auto tool = std::make_unique<TaskDemoTool>(2);
    TaskDemoTool *demo = tool.get();
    QVERIFY(registry.add(std::make_unique<FakeTool>(FakeTool::Spec{"other", "其他"})));
    QVERIFY(registry.add(std::move(tool)));

    auto platform = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/d")));
    PlatformServices *platformPtr = platform.get();
    MainWindow window(std::move(platform), std::move(registry));
    window.show();
    QVERIFY(window.selectToolById(QStringLiteral("task-demo")));

    TaskDemoPage *page = demo->taskPage();
    QVERIFY(page);
    QVERIFY(!page->isRunning());
    QVERIFY(!page->cancelButton()->isEnabled());

    QSignalSpy finished(page, &TaskDemoPage::finished);
    page->startButton()->click();
    QVERIFY(page->isRunning());
    QVERIFY(window.navigationLocked());
    QVERIFY(!window.selectToolById(QStringLiteral("other")));
    QVERIFY(!page->startButton()->isEnabled());
    QCOMPARE(window.statusStrip()->status().state, StatusState::Busy);
    QVERIFY(window.statusStrip()->lockText().contains(QStringLiteral("演示任务")));

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
    QVERIFY(finished.first().first().toBool());
    QVERIFY(!window.navigationLocked());
    QCOMPARE(window.statusStrip()->status().state, StatusState::Success);
    QCOMPARE(page->progress(), 100);
    QVERIFY(window.selectToolById(QStringLiteral("other")));

    bool sawStart = false;
    bool sawDone = false;
    for (const LogEntry &entry : platformPtr->log()->entries(QStringLiteral("task-demo"))) {
        sawStart = sawStart || entry.message.contains(QStringLiteral("开始"));
        sawDone = sawDone || (entry.level == LogLevel::Success);
    }
    QVERIFY(sawStart && sawDone);
}

void UiTests::builtin_taskDemoFailureAndCancel()
{
    QTemporaryDir dir;
    ToolRegistry registry;
    auto tool = std::make_unique<TaskDemoTool>(2);
    TaskDemoTool *demo = tool.get();
    QVERIFY(registry.add(std::move(tool)));
    auto platform = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/d")));
    MainWindow window(std::move(platform), std::move(registry));
    window.show();

    TaskDemoPage *page = demo->taskPage();
    QVERIFY(page);

    // 失败：锁释放、状态为错误、日志有错误级别
    QSignalSpy finished(page, &TaskDemoPage::finished);
    page->failCheck()->setChecked(true);
    page->startButton()->click();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
    QVERIFY(!finished.first().first().toBool());
    QVERIFY(!window.navigationLocked());
    QCOMPARE(window.statusStrip()->status().state, StatusState::Error);
    shot(&window, QStringLiteral("ui-task-error"));

    // 取消
    page->failCheck()->setChecked(false);
    page->startButton()->click();
    QVERIFY(window.navigationLocked());
    shot(&window, QStringLiteral("ui-task-busy"));
    page->cancelButton()->click();
    QVERIFY(!window.navigationLocked());
    QVERIFY(!page->isRunning());
    QCOMPARE(finished.count(), 2);
    QVERIFY(!finished.last().first().toBool());
}

void UiTests::builtin_taskDemoShutdownMidRunReleasesLock()
{
    QTemporaryDir dir;
    ToolRegistry registry;
    registry.add(std::make_unique<TaskDemoTool>(500));
    auto platform = std::make_unique<PlatformServices>(DataPaths(dir.path() + QStringLiteral("/d")));
    PlatformServices *platformPtr = platform.get();
    auto window = std::make_unique<MainWindow>(std::move(platform), std::move(registry));
    window->toolStack()->currentWidget()->findChild<QPushButton *>()->click();
    QVERIFY(window->navigationLocked());
    QPointer<NavigationGate> gate = platformPtr->gate();
    window->toolHost()->shutdown();
    QVERIFY(!gate->isLocked());
    window.reset();
}

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
