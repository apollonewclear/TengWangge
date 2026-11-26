#pragma once

#include "core/platform_services.h"
#include "shell/i_tool.h"

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace tw {

// “任务”页：模拟一次长时间任务，演示外壳服务的三种用法
//   1. 任务期间持有导航锁（NavigationLock），结束或取消时释放；
//   2. 用 showStatus 汇报 进行中 / 成功 / 错误；
//   3. 用 log 写日志。
// 没有任何真实业务逻辑。
class TaskDemoPage : public QWidget
{
    Q_OBJECT

public:
    TaskDemoPage(const ToolServices &services, int stepIntervalMs, QWidget *parent = nullptr);

    bool isRunning() const { return m_running; }
    int progress() const { return m_progress; }
    QPushButton *startButton() const { return m_start; }
    QPushButton *cancelButton() const { return m_cancel; }
    QCheckBox *failCheck() const { return m_fail; }

    // 页面销毁或工具关闭时调用：停止计时并释放锁。
    void abort();

signals:
    void finished(bool success);

private:
    void start();
    void cancel();
    void tick();
    void end(bool success, const QString &message);
    void updateUi();

    ToolServices m_services;
    NavigationLock m_lock;
    QTimer *m_timer = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_percent = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_cancel = nullptr;
    QCheckBox *m_fail = nullptr;
    int m_progress = 0;
    bool m_running = false;
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
