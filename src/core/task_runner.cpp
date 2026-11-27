#include "core/task_runner.h"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <exception>
#include <utility>

namespace tw {
namespace {

class ClosureThread final : public QThread
{
public:
    explicit ClosureThread(std::function<void()> fn)
        : m_fn(std::move(fn))
    {
    }

protected:
    void run() override
    {
        if (m_fn)
            m_fn();
    }

private:
    std::function<void()> m_fn;
};

} // namespace

TaskControl::TaskControl(TaskRunner *runner, quint64 runId, ToolServices services,
                         std::shared_ptr<detail::CancelTokenState> token)
    : m_runner(runner)
    , m_runId(runId)
    , m_services(std::move(services))
    , m_token(std::move(token))
{
}

bool TaskControl::isCancelRequested() const noexcept
{
    return m_token && m_token->canceled.load(std::memory_order_acquire);
}

bool TaskControl::sleepMs(int ms) const
{
    return m_token ? m_token->sleepMs(ms) : false;
}

void TaskControl::reportProgress(int percent, const QString &statusText)
{
    if (!m_runner || isCancelRequested())
        return;
    const int clamped = std::clamp(percent, 0, 100);
    TaskRunner *runner = m_runner;
    const quint64 runId = m_runId;
    QMetaObject::invokeMethod(
        runner,
        [runner, runId, clamped, statusText]() {
            runner->onWorkerProgress(runId, clamped, statusText);
        },
        Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------

TaskRunner::TaskRunner(const ToolServices &services, QObject *parent)
    : QObject(parent)
    , m_services(services)
{
}

TaskRunner::~TaskRunner()
{
    abort();
}

bool TaskRunner::start(const QString &lockReason, WorkFn work)
{
    if (m_running || !work)
        return false;

    m_running = true;
    m_progress = 0;
    m_reason = lockReason.trimmed().isEmpty() ? QStringLiteral("后台任务运行中") : lockReason;
    m_token = std::make_shared<detail::CancelTokenState>();
    const quint64 runId = ++m_runId;

    m_lock = m_services.lockNavigation(m_reason);
    m_services.logInfo(QStringLiteral("%1：开始").arg(m_reason));
    m_services.showStatus(StatusState::Busy, QStringLiteral("%1 0%").arg(m_reason), 0);

    const ToolServices workerServices = m_services;
    const auto token = m_token;
    TaskRunner *self = this;

    m_thread = new ClosureThread([self, runId, workerServices, token,
                                  work = std::move(work)]() mutable {
        TaskControl control(self, runId, workerServices, token);
        QString error;
        bool ok = false;
        try {
            ok = work(control, &error);
        } catch (const std::exception &e) {
            ok = false;
            error = QString::fromLocal8Bit(e.what());
        } catch (...) {
            ok = false;
            error = QStringLiteral("后台任务发生未知异常");
        }
        const bool canceled = token->canceled.load(std::memory_order_acquire);
        QMetaObject::invokeMethod(
            self,
            [self, runId, ok, canceled, error]() {
                self->onWorkerDone(runId, ok, canceled, error);
            },
            Qt::QueuedConnection);
    });

    emit started(m_reason);
    m_thread->start();
    return true;
}

void TaskRunner::cancel()
{
    if (!m_running || !m_token)
        return;
    m_token->requestCancel();
    m_services.showStatus(StatusState::Busy, QStringLiteral("正在取消：%1…").arg(m_reason), 0);
}

void TaskRunner::cancelAndWait()
{
    if (!m_running)
        return;
    if (m_token)
        m_token->requestCancel();

    if (m_thread) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    ++m_runId; // 丢弃工作线程投递的尾随事件
    m_running = false;
    m_lock.release();

    const QString msg = QStringLiteral("%1已取消（进度 %2%）").arg(m_reason).arg(m_progress);
    m_services.logWarning(msg);
    m_services.showStatus(StatusState::Info, msg);
    emit finished(false, true, msg);
}

void TaskRunner::abort()
{
    if (m_token)
        m_token->requestCancel();

    if (m_thread) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    if (m_running) {
        ++m_runId;
        m_running = false;
    }
    m_lock.release();
}

void TaskRunner::onWorkerProgress(quint64 runId, int percent, const QString &statusText)
{
    if (!m_running || runId != m_runId)
        return;
    m_progress = percent;
    const QString text = statusText.trimmed().isEmpty()
        ? QStringLiteral("%1 %2%").arg(m_reason).arg(percent)
        : statusText;
    m_services.showStatus(StatusState::Busy, text, 0);
    emit progressChanged(percent, text);
}

void TaskRunner::onWorkerDone(quint64 runId, bool ok, bool canceled, const QString &message)
{
    if (!m_running || runId != m_runId)
        return;

    if (m_thread) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    m_running = false;
    m_lock.release();

    if (canceled) {
        const QString msg = message.trimmed().isEmpty()
            ? QStringLiteral("%1已取消（进度 %2%）").arg(m_reason).arg(m_progress)
            : message;
        m_services.logWarning(msg);
        m_services.showStatus(StatusState::Info, msg);
        emit finished(false, true, msg);
        return;
    }

    if (ok) {
        m_progress = 100;
        const QString msg = message.trimmed().isEmpty()
            ? QStringLiteral("%1已完成").arg(m_reason)
            : message;
        m_services.logSuccess(msg);
        m_services.showStatus(StatusState::Success, msg);
        emit finished(true, false, msg);
    } else {
        const QString msg = message.trimmed().isEmpty()
            ? QStringLiteral("%1执行失败").arg(m_reason)
            : message;
        m_services.logError(msg);
        m_services.showStatus(StatusState::Error, msg);
        emit finished(false, false, msg);
    }
}

} // namespace tw
