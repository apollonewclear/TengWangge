#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <vector>

namespace tw {

// 长时间任务期间锁定工具和页面导航。
//
// 工具不直接开关导航，而是持有一把带原因的锁；多把锁可以并存，全部释放后
// 导航才恢复。这样两个任务互相覆盖、或某个任务忘记恢复，都不会让导航状态错乱。
class NavigationGate : public QObject
{
    Q_OBJECT

public:
    using Token = quint64;

    struct HoldInfo
    {
        QString ownerToolId;
        QString reason;
    };

    explicit NavigationGate(QObject *parent = nullptr);

    Token acquire(const QString &ownerToolId, const QString &reason);
    void release(Token token);                 // 重复释放无副作用
    void releaseAllFor(const QString &ownerToolId); // 工具卸载或失败时兜底

    bool isLocked() const { return !m_holds.empty(); }
    int holdCount() const { return static_cast<int>(m_holds.size()); }
    QString reason() const;                    // 最近一把仍持有的锁的原因
    QString ownerToolId() const;
    QVector<HoldInfo> activeHolds() const;

signals:
    void lockedChanged(bool locked);
    void changed();

private:
    struct Hold {
        Token token;
        QString owner;
        QString reason;
    };
    std::vector<Hold> m_holds;
    Token m_nextToken = 1;
};

// 作用域锁：构造即上锁，析构或 release() 时解锁。可移动，不可复制。
class NavigationLock
{
public:
    NavigationLock() = default;
    NavigationLock(NavigationGate *gate, const QString &ownerToolId, const QString &reason);
    ~NavigationLock();

    NavigationLock(const NavigationLock &) = delete;
    NavigationLock &operator=(const NavigationLock &) = delete;
    NavigationLock(NavigationLock &&other) noexcept;
    NavigationLock &operator=(NavigationLock &&other) noexcept;

    bool active() const { return m_token != 0 && !m_gate.isNull(); }
    void release();

private:
    QPointer<NavigationGate> m_gate;
    NavigationGate::Token m_token = 0;
};

} // namespace tw
