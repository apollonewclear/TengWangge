#include "tools/task_demo/task_demo_tool.h"

#include "shell/icon_factory.h"
#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace tw {

TaskDemoPage::TaskDemoPage(const ToolServices &services, int stepIntervalMs, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
    , m_runner(new TaskRunner(services, this))
    , m_stepIntervalMs(qMax(1, stepIntervalMs))
{
    const Theme &theme = Theme::workbench();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    layout->addWidget(ui::makeLabel(QStringLiteral("演示任务"), "pageTitle"));
    layout->addWidget(ui::makeLabel(
        QStringLiteral("基于平台标准后台运行器 TaskRunner 在独立工作线程执行。运行期间自动持有导航锁"
                       "（不可切换工具或页面），结束或取消时自动回收。"),
        "pageSubtitle"));

    // 顶部操作栏（保持 m_start 为本页首个 QPushButton，兼容自动化测试定位）
    QFrame *toolbar = ui::makeToolbar();
    auto *buttons = new QHBoxLayout(toolbar);
    buttons->setContentsMargins(10, 6, 10, 6);
    buttons->setSpacing(8);

    m_start = new QPushButton(QStringLiteral("开始任务"));
    ui::setRole(m_start, "primary");
    m_start->setIcon(IconFactory::icon(QStringLiteral("task"), theme.panel, theme.panel,
                                       theme.panel, theme.panel));
    m_start->setDefault(true);

    m_cancel = new QPushButton(QStringLiteral("取消"));
    ui::setRole(m_cancel, "danger");

    m_fail = new QCheckBox(QStringLiteral("在 60% 处模拟异常中断（验证错误态）"));

    buttons->addWidget(m_start);
    buttons->addWidget(m_cancel);
    buttons->addSpacing(12);
    buttons->addWidget(m_fail);
    buttons->addStretch(1);
    layout->addWidget(toolbar);

    QFrame *card = ui::makeCard();
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(10);

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(ui::makeLabel(QStringLiteral("当前阶段："), "fieldLabel"));
    m_state = ui::makeLabel(QStringLiteral("尚未运行"), "pill");
    m_state->setWordWrap(false);
    statusRow->addWidget(m_state);
    statusRow->addStretch(1);
    m_percent = ui::makeLabel(QStringLiteral("0%"), "code");
    m_percent->setWordWrap(false);
    statusRow->addWidget(m_percent);
    cardLayout->addLayout(statusRow);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_bar->setAccessibleName(QStringLiteral("任务进度"));
    cardLayout->addWidget(m_bar);

    cardLayout->addWidget(ui::makeLabel(
        QStringLiteral("排查提示：任务执行日志将实时写入底部“消息日志”面板与 logs/tengwangge.log（可按 Ctrl+L 展开检索或导出）。"),
        "hint"));

    layout->addWidget(card);
    layout->addStretch(1);

    connect(m_start, &QPushButton::clicked, this, &TaskDemoPage::start);
    connect(m_cancel, &QPushButton::clicked, this, &TaskDemoPage::cancel);
    connect(m_runner, &TaskRunner::progressChanged, this, [this](int) { updateUi(); });
    connect(m_runner, &TaskRunner::finished, this,
            [this](bool success, bool canceled, const QString &) {
                if (canceled) {
                    m_state->setText(QStringLiteral("已取消"));
                    ui::setRole(m_state, "pill");
                } else if (success) {
                    m_state->setText(QStringLiteral("已完成"));
                    ui::setRole(m_state, "pill");
                } else {
                    m_state->setText(QStringLiteral("执行失败"));
                    ui::setRole(m_state, "pillDanger");
                }
                updateUi();
                emit finished(success);
            });

    setTabOrder(m_start, m_cancel);
    setTabOrder(m_cancel, m_fail);
    updateUi();
}

bool TaskDemoPage::isRunning() const
{
    return m_runner->isRunning();
}

int TaskDemoPage::progress() const
{
    return m_runner->progress();
}

void TaskDemoPage::start()
{
    if (m_runner->isRunning())
        return;

    const bool failAt60 = m_fail->isChecked();
    const int stepMs = m_stepIntervalMs;
    ui::setRole(m_state, "pill");
    m_state->setText(QStringLiteral("运行中"));

    m_runner->start(QStringLiteral("演示任务运行中"),
                    [failAt60, stepMs](TaskControl &ctl, QString *error) -> bool {
                        for (int step = 1; step <= 10; ++step) {
                            if (!ctl.sleepMs(stepMs))
                                return false;
                            const int pct = step * 10;
                            ctl.reportProgress(pct);
                            if (failAt60 && pct >= 60) {
                                if (error)
                                    *error = QStringLiteral("演示任务失败：模拟的错误（进度 %1%）").arg(pct);
                                return false;
                            }
                        }
                        return true;
                    });
    updateUi();
}

void TaskDemoPage::cancel()
{
    m_runner->cancelAndWait();
}

void TaskDemoPage::abort()
{
    m_runner->abort();
    updateUi();
}

void TaskDemoPage::updateUi()
{
    const int pct = m_runner->progress();
    const bool running = m_runner->isRunning();
    m_bar->setValue(pct);
    m_percent->setText(QStringLiteral("%1%").arg(pct));
    if (running)
        m_state->setText(QStringLiteral("运行中"));
    m_start->setEnabled(!running);
    m_cancel->setEnabled(running);
    m_fail->setEnabled(!running);
}

// ---------------------------------------------------------------------------

TaskDemoTool::TaskDemoTool(int stepIntervalMs)
    : m_stepIntervalMs(stepIntervalMs)
{
}

ToolInfo TaskDemoTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("task-demo");
    info.title = QStringLiteral("任务演示");
    info.summary = QStringLiteral("演示长时间任务、状态栏与导航锁定");
    info.iconKey = QStringLiteral("task");
    info.order = 20;
    return info;
}

bool TaskDemoTool::initialize(const ToolContext &context, QString *)
{
    m_context = context;
    return true;
}

QVector<ToolPage> TaskDemoTool::createPages()
{
    auto *page = new TaskDemoPage(m_context.services, m_stepIntervalMs);
    m_page = page;

    auto *about = new QWidget;
    auto *layout = new QVBoxLayout(about);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addWidget(ui::makeLabel(QStringLiteral("关于本工具"), "pageTitle"));
    layout->addWidget(ui::makeLabel(
        QStringLiteral("这是一个不含业务功能的参考模块，用来验证新工具接入外壳时的标准模式："
                       "TaskRunner 后台执行器、消息日志、状态栏反馈、RAII 导航锁，以及单工具多页面组织。"),
        "pageSubtitle"));
    layout->addWidget(ui::makeLabel(
        QStringLiteral("操作规则：任务运行期间左侧工具栏与顶部页面条置灰锁定，仅本页“取消”按钮保持可用。"),
        "hint"));
    layout->addStretch(1);

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("run"), QStringLiteral("任务执行"),
                           QStringLiteral("运行演示任务"), QStringLiteral("task")},
                  page});
    pages.append({PageInfo{QStringLiteral("about"), QStringLiteral("模块说明"),
                           QStringLiteral("本工具演示的内容"), QStringLiteral("info")},
                  about});
    return pages;
}

void TaskDemoTool::shutdown()
{
    if (m_page)
        m_page->abort();
}

} // namespace tw
