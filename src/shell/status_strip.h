#pragma once

#include "core/navigation_gate.h"
#include "core/status_board.h"

#include <QDateTime>
#include <QFrame>

class QLabel;
class QPushButton;
class QTimer;

namespace tw {

class ElidedLabel;

// 底部浅灰状态栏：左侧状态色点 + 状态文字 + 导航锁定提示，
// 右侧实时刷新“用户:<user> 启动时间:... 运行时长:HH:mm:ss”（1:1 对齐参考图状态栏样式）+ 日志开关。
class StatusStrip : public QFrame
{
    Q_OBJECT

public:
    explicit StatusStrip(QWidget *parent = nullptr);

    void setStatus(const StatusMessage &message);
    void setLocked(bool locked, const QString &reason);
    void setLocks(const QVector<NavigationGate::HoldInfo> &holds);

    StatusMessage status() const { return m_status; }
    QString statusText() const;
    bool isLockShown() const;
    QString lockText() const;

    QPushButton *logToggle() const { return m_logToggle; }

private:
    void updateUptime();

    StatusMessage m_status;
    QDateTime m_startTime;
    QString m_userName;
    QTimer *m_uptimeTimer = nullptr;
    QLabel *m_dot = nullptr;
    ElidedLabel *m_text = nullptr;
    QLabel *m_lock = nullptr;
    QLabel *m_meta = nullptr;
    QPushButton *m_logToggle = nullptr;
};

} // namespace tw
