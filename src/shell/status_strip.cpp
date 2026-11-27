#include "shell/status_strip.h"

#include "core/app_info.h"
#include "shell/elided_label.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QVariant>

namespace tw {
namespace {

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

QString currentUserName()
{
    QString user = qEnvironmentVariable("USER").trimmed();
    if (user.isEmpty())
        user = qEnvironmentVariable("USERNAME").trimmed();
    return user.isEmpty() ? QStringLiteral("local") : user;
}

} // namespace

StatusStrip::StatusStrip(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("statusStrip"));
    setAccessibleName(QStringLiteral("状态栏"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 8, 0);
    layout->setSpacing(8);

    m_dot = new QLabel;
    m_dot->setObjectName(QStringLiteral("statusDot"));
    m_dot->setFixedSize(8, 8);

    m_text = new ElidedLabel;
    m_text->setObjectName(QStringLiteral("statusText"));
    m_text->setAccessibleName(QStringLiteral("当前状态"));

    m_lock = new QLabel;
    m_lock->setObjectName(QStringLiteral("statusLockChip"));
    m_lock->setAccessibleName(QStringLiteral("导航锁定"));
    m_lock->hide();

    m_meta = new QLabel(
        QStringLiteral("用户:%1   启动时间:%2   版本:v%3")
            .arg(currentUserName(),
                 QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 AppInfo::version()));
    m_meta->setObjectName(QStringLiteral("statusMeta"));
    m_meta->setFocusPolicy(Qt::NoFocus);

    m_logToggle = new QPushButton(QStringLiteral("日志"));
    m_logToggle->setObjectName(QStringLiteral("logToggle"));
    m_logToggle->setCheckable(true);
    m_logToggle->setFocusPolicy(Qt::StrongFocus);
    m_logToggle->setToolTip(QStringLiteral("显示/隐藏消息日志面板 (Ctrl+L)"));
    m_logToggle->setAccessibleName(QStringLiteral("日志开关"));

    layout->addWidget(m_dot, 0, Qt::AlignVCenter);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_lock, 0, Qt::AlignVCenter);
    layout->addWidget(m_meta, 0, Qt::AlignVCenter);
    layout->addWidget(m_logToggle, 0, Qt::AlignVCenter);

    setStatus(StatusMessage{StatusState::Idle, StatusBoard::idleText()});
}

void StatusStrip::setStatus(const StatusMessage &message)
{
    m_status = message;
    const QString state = StatusBoard::stateName(message.state);
    const bool changed = m_dot->property("state").toString() != state;
    m_dot->setProperty("state", state);
    m_text->setProperty("state", state);
    m_text->setFullText(message.text);
    if (changed) {
        repolish(m_dot);
        repolish(m_text);
    }
}

void StatusStrip::setLocked(bool locked, const QString &reason)
{
    if (!locked) {
        setLocks({});
        return;
    }
    setLocks({NavigationGate::HoldInfo{QString(), reason}});
}

void StatusStrip::setLocks(const QVector<NavigationGate::HoldInfo> &holds)
{
    const bool locked = !holds.isEmpty();
    m_lock->setVisible(locked);
    if (!locked) {
        m_lock->clear();
        m_lock->setToolTip(QString());
        return;
    }

    const QString latest = holds.last().reason;
    const QString shortReason = latest.size() > 28 ? latest.left(28) + QChar(0x2026) : latest;
    QString text;
    if (holds.size() > 1) {
        text = shortReason.isEmpty()
            ? QStringLiteral("导航已锁定（%1 项任务）").arg(holds.size())
            : QStringLiteral("导航已锁定（%1 项）：%2").arg(holds.size()).arg(shortReason);
    } else {
        text = shortReason.isEmpty()
            ? QStringLiteral("导航已锁定")
            : QStringLiteral("导航已锁定：%1").arg(shortReason);
    }
    m_lock->setText(text);

    QStringList tipLines{QStringLiteral("长时间任务进行中，全部结束前不能切换工具或页面：")};
    for (int i = 0; i < holds.size(); ++i) {
        const QString &owner = holds.at(i).ownerToolId;
        const QString &r = holds.at(i).reason;
        tipLines.append(owner.isEmpty()
                            ? QStringLiteral("• %1").arg(r.isEmpty() ? QStringLiteral("未命名任务") : r)
                            : QStringLiteral("• [%1] %2").arg(owner, r.isEmpty() ? QStringLiteral("未命名任务") : r));
    }
    m_lock->setToolTip(tipLines.join(QLatin1Char('\n')));
}

QString StatusStrip::statusText() const
{
    return m_text->fullText();
}

bool StatusStrip::isLockShown() const
{
    return !m_lock->isHidden();
}

QString StatusStrip::lockText() const
{
    return m_lock->text();
}

} // namespace tw
