#include "tools/task_demo/task_demo_tool.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace tw {
namespace {

struct StepDef
{
    QString code;
    QString name;
    QString stage;
    int targetPct;
};

QVector<StepDef> demoSteps()
{
    return {
        {QStringLiteral("step.01"), QStringLiteral("初始化运行上下文与隔离目录核验"), QStringLiteral("准备阶段"), 10},
        {QStringLiteral("step.02"), QStringLiteral("加载任务参数配置表"), QStringLiteral("准备阶段"), 20},
        {QStringLiteral("step.03"), QStringLiteral("建立输入模型拓扑索引"), QStringLiteral("校验阶段"), 30},
        {QStringLiteral("step.04"), QStringLiteral("扫描待处理对象集合"), QStringLiteral("校验阶段"), 40},
        {QStringLiteral("step.05"), QStringLiteral("执行批处理第 1 轮分量计算"), QStringLiteral("执行阶段"), 50},
        {QStringLiteral("step.06"), QStringLiteral("执行批处理第 2 轮一致性校验"), QStringLiteral("执行阶段"), 60},
        {QStringLiteral("step.07"), QStringLiteral("生成中间快照与差异对比"), QStringLiteral("固化阶段"), 70},
        {QStringLiteral("step.08"), QStringLiteral("固化目标数据记录"), QStringLiteral("固化阶段"), 80},
        {QStringLiteral("step.09"), QStringLiteral("刷新状态摘要与审计日志"), QStringLiteral("收尾阶段"), 90},
        {QStringLiteral("step.10"), QStringLiteral("释放导航锁并归档运行报告"), QStringLiteral("收尾阶段"), 100},
    };
}

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

QWidget *makeAboutPage()
{
    const Theme &theme = Theme::workbench();
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 6, 8, 4);
    layout->setSpacing(5);

    layout->addWidget(ui::makeLabel(QStringLiteral("长任务参考模块接入规范（不含真实业务数据）"), "paneTitle"));

    auto *table = new QTableWidget;
    table->setAccessibleName(QStringLiteral("模块机制说明表"));
    ui::configureDataTable(table,
                           {QStringLiteral("序号"),
                            QStringLiteral("机制编码"),
                            QStringLiteral("能力名称"),
                            QStringLiteral("对应核心类"),
                            QStringLiteral("接入约束与行为说明")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 52);
    table->setColumnWidth(1, 130);
    table->setColumnWidth(2, 145);
    table->setColumnWidth(3, 150);

    const QList<QStringList> rows = {
        {QStringLiteral("task.runner"), QStringLiteral("独立工作线程执行"), QStringLiteral("tw::TaskRunner"),
         QStringLiteral("后台闭包运行在独立 QThread，支持 QWaitCondition 零延迟唤醒取消与异常兜底")},
        {QStringLiteral("nav.lock"), QStringLiteral("RAII 引用计数导航锁"), QStringLiteral("tw::NavigationLock"),
         QStringLiteral("任务启动自动置灰顶部模块栏与页签条，结束/失败/取消/析构自动解锁")},
        {QStringLiteral("status.board"), QStringLiteral("显式状态机反馈"), QStringLiteral("tw::StatusBoard"),
         QStringLiteral("按 Idle / Info / Busy / Success / Error 显式上报状态栏，不靠字符串猜测")},
        {QStringLiteral("log.sink"), QStringLiteral("工具隔离日志追踪"), QStringLiteral("tw::LogBuffer"),
         QStringLiteral("实时推送到底部消息日志框并自动落盘至 logs/tengwangge.log")},
        {QStringLiteral("tool.shutdown"), QStringLiteral("关闭期安全收敛"), QStringLiteral("ITool::shutdown()"),
         QStringLiteral("主窗口关闭时优先调用 abort() 终止线程并回收导航锁，杜绝悬垂引用")},
    };
    table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        table->setRowHeight(i, 26);
        table->setItem(i, 0, makeCell(QString::number(i + 1), Qt::AlignCenter));
        QTableWidgetItem *codeCell = makeCell(rows.at(i).at(0));
        codeCell->setForeground(QBrush(theme.primary));
        table->setItem(i, 1, codeCell);
        table->setItem(i, 2, makeCell(rows.at(i).at(1)));
        table->setItem(i, 3, makeCell(rows.at(i).at(2)));
        table->setItem(i, 4, makeCell(rows.at(i).at(3)));
    }
    table->selectRow(0);
    layout->addWidget(table, 1);
    return page;
}

} // namespace

TaskDemoPage::TaskDemoPage(const ToolServices &services, int stepIntervalMs, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
    , m_runner(new TaskRunner(services, this))
    , m_stepIntervalMs(qMax(1, stepIntervalMs))
{
    const Theme &theme = Theme::workbench();
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 6, 8, 4);
    rootLayout->setSpacing(0);

    QSplitter *splitter = ui::makeSplitter(Qt::Horizontal, this);

    // 左侧：任务方案树（无按钮，确保右侧 m_start 为页面首个 QPushButton）
    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(ui::makeLabel(QStringLiteral("演示任务方案"), "paneTitle"));

    m_planTree = new QTreeWidget;
    m_planTree->setHeaderHidden(true);
    m_planTree->setIndentation(16);
    m_planTree->setUniformRowHeights(true);
    m_planTree->setAccessibleName(QStringLiteral("任务方案树"));

    auto *group = new QTreeWidgetItem(m_planTree, QStringList{QStringLiteral("批处理校验方案")});
    auto *normalPlan = new QTreeWidgetItem(group, QStringList{QStringLiteral("标准全量十步执行")});
    normalPlan->setData(0, Qt::UserRole, false);
    auto *failPlan = new QTreeWidgetItem(group, QStringList{QStringLiteral("60% 异常中断演练")});
    failPlan->setData(0, Qt::UserRole, true);
    m_planTree->expandAll();
    m_planTree->setCurrentItem(normalPlan);

    leftLayout->addWidget(m_planTree, 1);
    leftPane->setMinimumWidth(185);
    leftPane->setMaximumWidth(250);

    // 右侧：顶部动作条（保持 m_start 为首个 QPushButton 兼容自动化测试）+ 进度条 + 10 步状态表
    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(5);

    rightLayout->addWidget(ui::makeLabel(QStringLiteral("任务控制与步骤执行监视"), "paneTitle"));

    auto *actionBar = new QHBoxLayout;
    actionBar->setContentsMargins(0, 0, 0, 0);
    actionBar->setSpacing(8);

    m_start = ui::makeActionButton(QStringLiteral("开始任务"), QStringLiteral("update"));
    m_start->setDefault(true);
    m_cancel = ui::makeActionButton(QStringLiteral("取消"), QStringLiteral("delete"));
    m_fail = new QCheckBox(QStringLiteral("在 60% 处模拟异常中断"));

    m_state = ui::makeLabel(QStringLiteral("尚未运行"), "pill");
    m_state->setWordWrap(false);
    m_percent = ui::makeLabel(QStringLiteral("0%"), "code");
    m_percent->setWordWrap(false);

    actionBar->addWidget(m_start);
    actionBar->addWidget(m_cancel);
    actionBar->addSpacing(6);
    actionBar->addWidget(m_fail);
    actionBar->addStretch(1);
    actionBar->addWidget(ui::makeLabel(QStringLiteral("状态:"), "fieldLabel"));
    actionBar->addWidget(m_state);
    actionBar->addWidget(m_percent);
    rightLayout->addLayout(actionBar);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setFixedHeight(8);
    m_bar->setTextVisible(false);
    m_bar->setAccessibleName(QStringLiteral("任务进度"));
    rightLayout->addWidget(m_bar);

    m_stepTable = new QTableWidget;
    m_stepTable->setAccessibleName(QStringLiteral("任务步骤明细表"));
    ui::configureDataTable(m_stepTable,
                           {QStringLiteral("序号"),
                            QStringLiteral("步骤编码"),
                            QStringLiteral("步骤名称"),
                            QStringLiteral("所属阶段"),
                            QStringLiteral("目标进度"),
                            QStringLiteral("当前状态")});
    m_stepTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_stepTable->setColumnWidth(0, 52);
    m_stepTable->setColumnWidth(1, 105);
    m_stepTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_stepTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_stepTable->setColumnWidth(3, 100);
    m_stepTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_stepTable->setColumnWidth(4, 84);
    m_stepTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_stepTable->setColumnWidth(5, 96);

    const QVector<StepDef> steps = demoSteps();
    m_stepTable->setRowCount(steps.size());
    for (int i = 0; i < steps.size(); ++i) {
        m_stepTable->setRowHeight(i, 26);
        m_stepTable->setItem(i, 0, makeCell(QString::number(i + 1), Qt::AlignCenter));
        QTableWidgetItem *codeCell = makeCell(steps.at(i).code);
        codeCell->setForeground(QBrush(theme.primary));
        m_stepTable->setItem(i, 1, codeCell);
        m_stepTable->setItem(i, 2, makeCell(steps.at(i).name));
        m_stepTable->setItem(i, 3, makeCell(steps.at(i).stage, Qt::AlignCenter));
        m_stepTable->setItem(i, 4, makeCell(QStringLiteral("%1%").arg(steps.at(i).targetPct), Qt::AlignCenter));
        m_stepTable->setItem(i, 5, makeCell(QStringLiteral("待执行"), Qt::AlignCenter));
    }
    m_stepTable->selectRow(0);
    rightLayout->addWidget(m_stepTable, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({205, 925});
    rootLayout->addWidget(splitter, 1);

    connect(m_planTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (!current || m_runner->isRunning() || !current->data(0, Qt::UserRole).isValid())
                    return;
                m_fail->setChecked(current->data(0, Qt::UserRole).toBool());
            });
    connect(m_fail, &QCheckBox::toggled, this,
            [this, normalPlan, failPlan](bool checked) {
                QSignalBlocker blocker(m_planTree);
                m_planTree->setCurrentItem(checked ? failPlan : normalPlan);
            });

    connect(m_start, &QPushButton::clicked, this, &TaskDemoPage::start);
    connect(m_cancel, &QPushButton::clicked, this, &TaskDemoPage::cancel);
    connect(m_runner, &TaskRunner::progressChanged, this, [this](int pct) {
        m_lastPct = pct;
        updateUi();
    });
    connect(m_runner, &TaskRunner::finished, this,
            [this](bool success, bool canceled, const QString &) {
                m_lastPct = m_runner->progress();
                if (canceled) {
                    m_outcome = RunOutcome::Canceled;
                    m_state->setText(QStringLiteral("已取消"));
                    ui::setRole(m_state, "pill");
                } else if (success) {
                    m_outcome = RunOutcome::Succeeded;
                    m_state->setText(QStringLiteral("已完成"));
                    ui::setRole(m_state, "pill");
                } else {
                    m_outcome = RunOutcome::Failed;
                    m_state->setText(QStringLiteral("执行失败"));
                    ui::setRole(m_state, "pillDanger");
                }
                updateUi();
                emit finished(success);
            });

    setTabOrder(m_planTree, m_start);
    setTabOrder(m_start, m_cancel);
    setTabOrder(m_cancel, m_fail);
    setTabOrder(m_fail, m_stepTable);
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
    m_outcome = RunOutcome::Running;
    m_lastPct = 0;
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
    if (m_outcome == RunOutcome::Running)
        m_outcome = RunOutcome::Canceled;
    updateUi();
}

void TaskDemoPage::refreshStepTable()
{
    if (!m_stepTable)
        return;
    const Theme &theme = Theme::workbench();
    const int doneSteps = qBound(0, m_lastPct / 10, m_stepTable->rowCount());
    int activeRow = 0;

    for (int i = 0; i < m_stepTable->rowCount(); ++i) {
        QTableWidgetItem *cell = m_stepTable->item(i, 5);
        if (!cell)
            continue;
        if (m_outcome == RunOutcome::Idle) {
            cell->setText(QStringLiteral("待执行"));
            cell->setForeground(QBrush(theme.textMuted));
        } else if (m_outcome == RunOutcome::Succeeded) {
            cell->setText(QStringLiteral("已完成"));
            cell->setForeground(QBrush(theme.statusSuccess));
            activeRow = m_stepTable->rowCount() - 1;
        } else if (m_outcome == RunOutcome::Running) {
            if (i < doneSteps) {
                cell->setText(QStringLiteral("已完成"));
                cell->setForeground(QBrush(theme.statusSuccess));
            } else if (i == doneSteps) {
                cell->setText(QStringLiteral("执行中..."));
                cell->setForeground(QBrush(theme.primary));
                activeRow = i;
            } else {
                cell->setText(QStringLiteral("待执行"));
                cell->setForeground(QBrush(theme.textMuted));
            }
        } else if (m_outcome == RunOutcome::Failed) {
            const int failIdx = qMax(0, doneSteps - 1);
            if (i < failIdx) {
                cell->setText(QStringLiteral("已完成"));
                cell->setForeground(QBrush(theme.statusSuccess));
            } else if (i == failIdx) {
                cell->setText(QStringLiteral("异常中断"));
                cell->setForeground(QBrush(theme.danger));
                activeRow = i;
            } else {
                cell->setText(QStringLiteral("未执行"));
                cell->setForeground(QBrush(theme.textMuted));
            }
        } else if (m_outcome == RunOutcome::Canceled) {
            if (i < doneSteps) {
                cell->setText(QStringLiteral("已完成"));
                cell->setForeground(QBrush(theme.statusSuccess));
            } else if (i == doneSteps) {
                cell->setText(QStringLiteral("已取消"));
                cell->setForeground(QBrush(theme.statusBusy));
                activeRow = i;
            } else {
                cell->setText(QStringLiteral("未执行"));
                cell->setForeground(QBrush(theme.textMuted));
            }
        }
    }
    if (activeRow >= 0 && activeRow < m_stepTable->rowCount())
        m_stepTable->selectRow(activeRow);
}

void TaskDemoPage::updateUi()
{
    const int pct = m_runner->progress();
    const bool running = m_runner->isRunning();
    m_lastPct = pct;
    m_bar->setValue(pct);
    m_percent->setText(QStringLiteral("%1%").arg(pct));
    if (running) {
        m_outcome = RunOutcome::Running;
        m_state->setText(QStringLiteral("运行中"));
    }
    m_start->setEnabled(!running);
    m_cancel->setEnabled(running);
    m_fail->setEnabled(!running);
    if (m_planTree)
        m_planTree->setEnabled(!running);
    refreshStepTable();
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
    info.summary = QStringLiteral("演示长时间任务、步骤监视与导航锁定");
    info.iconKey = QStringLiteral("task");
    info.category = QStringLiteral("任务与运维");
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

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("run"), QStringLiteral("任务执行"),
                           QStringLiteral("运行演示任务与步骤监视"), QStringLiteral("task")},
                  page});
    pages.append({PageInfo{QStringLiteral("about"), QStringLiteral("模块说明"),
                           QStringLiteral("本工具演示的平台规范"), QStringLiteral("info")},
                  makeAboutPage()});
    return pages;
}

void TaskDemoTool::shutdown()
{
    if (m_page)
        m_page->abort();
}

} // namespace tw
