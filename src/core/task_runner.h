#pragma once

#include "core/platform_services.h"

#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>

#include <atomic>
#include <functional>
#include <memory>

class QThread;

namespace tw {

class TaskRunner;

namespace detail {
struct CancelTokenState
{
    std::atomic_bool canceled{false};
    QMutex mutex;
    QWaitCondition cond;

    void requestCancel()
    {
        {
            QMutexLocker lock(&mutex);
            canceled.store(true, std::memory_order_release);
        }
        cond.wakeAll();
    }

    // 可被取消立即唤醒的休眠；若因取消而提前返回则返回 false。
    bool sleepMs(int ms)
    {
        if (canceled.load(std::memory_order_acquire))
            return false;
        if (ms <= 0)
            return !canceled.load(std::memory_order_acquire);
        QMutexLocker lock(&mutex);
        if (canceled.load(std::memory_order_acquire))
            return false;
        cond.wait(&mutex, static_cast<unsigned long>(ms));
        return !canceled.load(std::memory_order_acquire);
    }
};
} // namespace detail

// 传给后台工作线程函数的协作控制句柄：线程安全地检查取消标志、可中断休眠、汇报进度和写日志。
class TaskControl
{
public:
    bool isCancelRequested() const noexcept;
    bool sleepMs(int ms) const;
    void reportProgress(int percent, const QString &statusText = QString());
    const ToolServices &services() const noexcept { return m_services; }

private:
    friend class TaskRunner;
    TaskControl(TaskRunner *runner, quint64 runId, ToolServices services,
                std::shared_ptr<detail::CancelTokenState> token);

    TaskRunner *m_runner = nullptr;
    quint64 m_runId = 0;
    ToolServices m_services;
    std::shared_ptr<detail::CancelTokenState> m_token;
};

// 标准后台长任务运行器：
// - 在 GUI 线程启动时自动获取 RAII NavigationLock、写启动日志并置 Busy 状态；
// - 将耗时闭包放入独立工作线程执行，捕获异常防止进程崩溃；
// - 支持异步 cancel()、同步取消并回收锁 cancelAndWait()，以及静默收尾 abort()（供 shutdown 调用）；
// - 结束时先释放导航锁、更新状态栏与日志，再发射 finished() 信号。
class TaskRunner : public QObject
{
    Q_OBJECT

public:
    using WorkFn = std::function<bool(TaskControl &control, QString *errorMessage)>;

    explicit TaskRunner(const ToolServices &services, QObject *parent = nullptr);
    ~TaskRunner() override;

    bool isRunning() const { return m_running; }
    int progress() const { return m_progress; }

    bool start(const QString &lockReason, WorkFn work);
    void cancel();
    void cancelAndWait();
    void abort();

signals:
    void started(const QString &reason);
    void progressChanged(int percent, const QString &statusText);
    void finished(bool success, bool canceled, const QString &message);

private:
    friend class TaskControl;
    void onWorkerProgress(quint64 runId, int percent, const QString &statusText);
    void onWorkerDone(quint64 runId, bool ok, bool canceled, const QString &message);

    ToolServices m_services;
    NavigationLock m_lock;
    QString m_reason;
    QThread *m_thread = nullptr;
    std::shared_ptr<detail::CancelTokenState> m_token;
    quint64 m_runId = 0;
    int m_progress = 0;
    bool m_running = false;
};

} // namespace tw
