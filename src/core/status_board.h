#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QTimer;

namespace tw {

enum class StatusState { Idle, Info, Busy, Success, Error };

struct StatusMessage
{
    StatusState state = StatusState::Idle;
    QString text;
};

// 每个工具各自一条当前状态。状态由调用方显式给出，而不是靠文字猜测。
// 外壳切换工具时只是读取另一个工具的状态，不会串到别的工具上。
class StatusBoard : public QObject
{
    Q_OBJECT

public:
    // set() 不传超时时使用按状态区分的默认值：信息/成功自动回到空闲，
    // 进行中/错误保持到被替换或清除。
    static constexpr int UseDefaultTimeout = -1;

    explicit StatusBoard(QObject *parent = nullptr);

    void set(const QString &toolId, StatusState state, const QString &text,
             int timeoutMs = UseDefaultTimeout);
    void clear(const QString &toolId);
    StatusMessage current(const QString &toolId) const;

    static int defaultTimeout(StatusState state);
    static QString idleText();
    // 作为 QSS 属性值使用："idle" "info" "busy" "success" "error"
    static QString stateName(StatusState state);

signals:
    void changed(const QString &toolId);

private:
    struct Slot {
        StatusMessage message;
        quint64 generation = 0;
        QTimer *timer = nullptr;
    };
    QHash<QString, Slot> m_slots;
    quint64 m_nextGeneration = 1;
};

} // namespace tw
