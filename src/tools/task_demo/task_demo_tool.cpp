#include "tools/task_demo/task_demo_tool.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

namespace tw {
namespace {

QLabel *makeLabel(const QString &text, const char *role)
{
    auto *label = new QLabel(text);
    label->setProperty("role", QString::fromLatin1(role));
    label->setWordWrap(true);
    return label;
}

void setPillRole(QLabel *label, const char *role)
{
    if (label->property("role").toString() == QLatin1String(role))
        return;
    label->setProperty("role", QString::fromLatin1(role));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

} // namespace

TaskDemoPage::TaskDemoPage(const ToolServices &services, int stepIntervalMs, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
{
    const Theme &theme = Theme::workbench();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    layout->addWidget(makeLabel(QStringLiteral("演示任务"), "pageTitle"));
    layout->addWidget(makeLabel(
        QStringLiteral("模拟一次长时间批处理任务。运行期间自动持有导航锁（不可切换工具或页面），"
                       "状态栏实时提示锁定原因，结束或取消后自动恢复。"),
        "pageSubtitle"));

    // 顶部操作栏（保持 m_start 为本页首个 QPushButton，兼容自动化测试定位）
    auto *toolbar = new QFrame;
    toolbar->setProperty("role", QStringLiteral("toolbar"));
    auto *buttons = new QHBoxLayout(toolbar);
    buttons->setContentsMargins(10, 6, 10, 6);
    buttons->setSpacing(8);

    m_start = new QPushButton(QStringLiteral("开始任务"));
    m_start->setProperty("role", QStringLiteral("primary"));
    m_start->setIcon(IconFactory::icon(QStringLiteral("task"), theme.panel, theme.panel,
                                       theme.panel, theme.panel));
    m_start->setDefault(true);

    m_cancel = new QPushButton(QStringLiteral("取消"));
    m_cancel->setProperty("role", QStringLiteral("danger"));

    m_fail = new QCheckBox(QStringLiteral("在 60% 处模拟异常中断（验证错误态）"));

    buttons->addWidget(m_start);
    buttons->addWidget(m_cancel);
    buttons->addSpacing(12);
    buttons->addWidget(m_fail);
    buttons->addStretch(1);
    layout->addWidget(toolbar);

    // 任务执行状态面板
    auto *card = new QFrame;
    card->setProperty("role", QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(10);

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(makeLabel(QStringLiteral("当前阶段："), "fieldLabel"));
    m_state = makeLabel(QStringLiteral("尚未运行"), "pill");
    m_state->setWordWrap(false);
    statusRow->addWidget(m_state);
    statusRow->addStretch(1);
    m_percent = makeLabel(QStringLiteral("0%"), "code");
    m_percent->setWordWrap(false);
    statusRow->addWidget(m_percent);
    cardLayout->addLayout(statusRow);

    // 进度条本身不叠黑字，百分比由右侧独立高亮标签展示，彻底解决 60% 处蓝底黑字对比度过低问题
    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_bar->setAccessibleName(QStringLiteral("任务进度"));
    cardLayout->addWidget(m_bar);

    cardLayout->addWidget(makeLabel(
        QStringLiteral("排查提示：任务执行日志将实时写入底部“消息日志”面板（可按 Ctrl+L 展开核对）。"),
        "hint"));

    layout->addWidget(card);
    layout->addStretch(1);

    m_timer = new QTimer(this);
    m_timer->setInterval(qMax(1, stepIntervalMs));
    connect(m_timer, &QTimer::timeout, this, &TaskDemoPage::tick);
    connect(m_start, &QPushButton::clicked, this, &TaskDemoPage::start);
    connect(m_cancel, &QPushButton::clicked, this, &TaskDemoPage::cancel);

    setTabOrder(m_start, m_cancel);
    setTabOrder(m_cancel, m_fail);
    updateUi();
}

void TaskDemoPage::start()
{
    if (m_running)
        return;
    m_running = true;
    m_progress = 0;
    setPillRole(m_state, "pill");
    m_lock = m_services.lockNavigation(QStringLiteral("演示任务运行中"));
    m_services.logInfo(QStringLiteral("演示任务开始"));
    m_services.showStatus(StatusState::Busy, QStringLiteral("演示任务运行中 0%"));
    m_timer->start();
    updateUi();
}

void TaskDemoPage::cancel()
{
    if (!m_running)
        return;
    m_timer->stop();
    m_running = false;
    m_lock.release();
    m_services.logWarning(QStringLiteral("演示任务已取消（进度 %1%）").arg(m_progress));
    m_services.showStatus(StatusState::Info, QStringLiteral("演示任务已取消"));
    m_state->setText(QStringLiteral("已取消"));
    setPillRole(m_state, "pill");
    updateUi();
    emit finished(false);
}

void TaskDemoPage::tick()
{
    m_progress = qMin(100, m_progress + 10);
    if (m_fail->isChecked() && m_progress >= 60) {
        end(false, QStringLiteral("演示任务失败：模拟的错误（进度 %1%）").arg(m_progress));
        return;
    }
    if (m_progress >= 100) {
        end(true, QStringLiteral("演示任务完成"));
        return;
    }
    m_services.showStatus(StatusState::Busy, QStringLiteral("演示任务运行中 %1%").arg(m_progress));
    updateUi();
}

void TaskDemoPage::end(bool success, const QString &message)
{
    m_timer->stop();
    m_running = false;
    m_lock.release();
    if (success) {
        m_services.logSuccess(message);
        m_services.showStatus(StatusState::Success, message);
        m_state->setText(QStringLiteral("已完成"));
        setPillRole(m_state, "pill");
    } else {
        m_services.logError(message);
        m_services.showStatus(StatusState::Error, message);
        m_state->setText(QStringLiteral("执行失败"));
        setPillRole(m_state, "pillDanger");
    }
    updateUi();
    emit finished(success);
}

void TaskDemoPage::abort()
{
    m_timer->stop();
    m_running = false;
    m_lock.release();
}

void TaskDemoPage::updateUi()
{
    m_bar->setValue(m_progress);
    m_percent->setText(QStringLiteral("%1%").arg(m_progress));
    if (m_running)
        m_state->setText(QStringLiteral("运行中"));
    m_start->setEnabled(!m_running);
    m_cancel->setEnabled(m_running);
    m_fail->setEnabled(!m_running);
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
    layout->addWidget(makeLabel(QStringLiteral("关于本工具"), "pageTitle"));
    layout->addWidget(makeLabel(
        QStringLiteral("这是一个不含业务功能的参考模块，用来验证新工具接入外壳时的标准模式："
                       "消息日志、状态栏反馈、RAII 导航锁，以及单工具多页面组织。"),
        "pageSubtitle"));
    layout->addWidget(makeLabel(
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
