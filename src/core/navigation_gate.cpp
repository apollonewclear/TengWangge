#include "core/navigation_gate.h"

#include <algorithm>

namespace tw {

NavigationGate::NavigationGate(QObject *parent)
    : QObject(parent)
{
}

NavigationGate::Token NavigationGate::acquire(const QString &ownerToolId, const QString &reason)
{
    const bool wasLocked = isLocked();
    const Token token = m_nextToken++;
    m_holds.push_back(Hold{token, ownerToolId, reason});
    if (!wasLocked)
        emit lockedChanged(true);
    emit changed();
    return token;
}

void NavigationGate::release(Token token)
{
    const auto it = std::find_if(m_holds.begin(), m_holds.end(),
                                 [token](const Hold &hold) { return hold.token == token; });
    if (it == m_holds.end())
        return;
    m_holds.erase(it);
    if (!isLocked())
        emit lockedChanged(false);
    emit changed();
}

void NavigationGate::releaseAllFor(const QString &ownerToolId)
{
    const auto before = m_holds.size();
    m_holds.erase(std::remove_if(m_holds.begin(), m_holds.end(),
                                 [&](const Hold &hold) { return hold.owner == ownerToolId; }),
                  m_holds.end());
    if (m_holds.size() == before)
        return;
    if (!isLocked())
        emit lockedChanged(false);
    emit changed();
}

QString NavigationGate::reason() const
{
    return m_holds.empty() ? QString() : m_holds.back().reason;
}

QString NavigationGate::ownerToolId() const
{
    return m_holds.empty() ? QString() : m_holds.back().owner;
}

QVector<NavigationGate::HoldInfo> NavigationGate::activeHolds() const
{
    QVector<HoldInfo> list;
    list.reserve(static_cast<int>(m_holds.size()));
    for (const Hold &hold : m_holds)
        list.append(HoldInfo{hold.owner, hold.reason});
    return list;
}

NavigationLock::NavigationLock(NavigationGate *gate, const QString &ownerToolId,
                               const QString &reason)
    : m_gate(gate)
{
    if (gate)
        m_token = gate->acquire(ownerToolId, reason);
}

NavigationLock::~NavigationLock()
{
    release();
}

NavigationLock::NavigationLock(NavigationLock &&other) noexcept
    : m_gate(other.m_gate)
    , m_token(other.m_token)
{
    other.m_token = 0;
    other.m_gate.clear();
}

NavigationLock &NavigationLock::operator=(NavigationLock &&other) noexcept
{
    if (this != &other) {
        release();
        m_gate = other.m_gate;
        m_token = other.m_token;
        other.m_token = 0;
        other.m_gate.clear();
    }
    return *this;
}

void NavigationLock::release()
{
    if (m_token != 0 && !m_gate.isNull())
        m_gate->release(m_token);
    m_token = 0;
    m_gate.clear();
}

} // namespace tw
