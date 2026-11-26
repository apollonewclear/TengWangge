#pragma once

#include "core/status_board.h"

#include <QFrame>

class QLabel;
class QPushButton;

namespace tw {

class ElidedLabel;

// 状态栏内容：状态色点 + 状态文字（过长省略）+ 导航锁定提示 + 日志开关。
// 状态用 QSS 属性 state 着色，除了颜色，文字本身也带含义，不单靠颜色区分。
class StatusStrip : public QFrame
{
    Q_OBJECT

public:
    explicit StatusStrip(QWidget *parent = nullptr);

    void setStatus(const StatusMessage &message);
    void setLocked(bool locked, const QString &reason);

    StatusMessage status() const { return m_status; }
    QString statusText() const;
    bool isLockShown() const;
    QString lockText() const;

    QPushButton *logToggle() const { return m_logToggle; }

private:
    StatusMessage m_status;
    QLabel *m_dot = nullptr;
    ElidedLabel *m_text = nullptr;
    QLabel *m_lock = nullptr;
    QLabel *m_meta = nullptr;
    QPushButton *m_logToggle = nullptr;
};

} // namespace tw
