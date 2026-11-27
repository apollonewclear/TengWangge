#pragma once

#include "core/platform_services.h"
#include "core/task_runner.h"
#include "shell/i_tool.h"

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTreeWidget;

namespace tw {

// “任务执行”页：基于平台标准后台运行器 TaskRunner 演示真实工作线程执行、
// 可中断休眠、步骤级表格状态刷新、RAII 导航锁定以及错误/取消收敛。
class TaskDemoPage : public QWidget
{
    Q_OBJECT

public:
    TaskDemoPage(const ToolServices &services, int stepIntervalMs, QWidget *parent = nullptr);

    bool isRunning() const;
    int progress() const;
    QPushButton *startButton() const { return m_start; }
    QPushButton *cancelButton() const { return m_cancel; }
    QCheckBox *failCheck() const { return m_fail; }

    // 页面销毁或工具关闭时调用：立即唤醒后台线程并释放锁。
    void abort();

signals:
    void finished(bool success);

private:
    enum class RunOutcome
    {
        Idle,
        Running,
        Succeeded,
        Failed,
        Canceled
    };

    void start();
    void cancel();
    void updateUi();
    void refreshStepTable();

    ToolServices m_services;
    TaskRunner *m_runner = nullptr;
    int m_stepIntervalMs = 400;
    RunOutcome m_outcome = RunOutcome::Idle;
    int m_lastPct = 0;
    QTreeWidget *m_planTree = nullptr;
    QTableWidget *m_stepTable = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_percent = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_cancel = nullptr;
    QCheckBox *m_fail = nullptr;
};

class TaskDemoTool : public ITool
{
public:
    // stepIntervalMs：每推进 10% 的间隔；默认约 4 秒跑完，测试里可以缩短。
    explicit TaskDemoTool(int stepIntervalMs = 400);

    ToolInfo info() const override;
    bool initialize(const ToolContext &context, QString *error) override;
    QVector<ToolPage> createPages() override;
    void shutdown() override;

    TaskDemoPage *taskPage() const { return m_page; }

private:
    int m_stepIntervalMs;
    ToolContext m_context;
    QPointer<TaskDemoPage> m_page;
};

} // namespace tw
