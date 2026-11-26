#include "shell/tool_host.h"

#include "shell/tool_view.h"

#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <exception>

namespace tw {
namespace {

QString describeException(const std::exception &e)
{
    return QString::fromLocal8Bit(e.what());
}

// 页面清单是否合法；不合法时返回原因。
QString validatePages(const QVector<ToolPage> &pages)
{
    if (pages.isEmpty())
        return QStringLiteral("工具没有提供任何页面");
    QSet<QString> seen;
    QSet<QWidget *> seenWidgets;
    for (const ToolPage &page : pages) {
        if (!page.widget)
            return QStringLiteral("页面“%1”没有窗口部件").arg(page.info.id);
        if (seenWidgets.contains(page.widget))
            return QStringLiteral("页面“%1”复用了同一个窗口部件实例").arg(page.info.id);
        if (!isValidIdentifier(page.info.id))
            return QStringLiteral("页面标识无效：%1").arg(page.info.id);
        if (page.info.title.trimmed().isEmpty())
            return QStringLiteral("页面 %1 缺少标题").arg(page.info.id);
        if (seen.contains(page.info.id))
            return QStringLiteral("页面标识重复：%1").arg(page.info.id);
        seen.insert(page.info.id);
        seenWidgets.insert(page.widget);
    }
    return {};
}

} // namespace

ToolHost::ToolHost(ToolRegistry *registry, PlatformServices *platform, QWidget *dialogParent,
                   QStackedWidget *stack, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_platform(platform)
    , m_dialogParent(dialogParent)
    , m_stack(stack)
{
    m_slots.resize(registry ? registry->count() : 0);
}

ToolHost::~ToolHost()
{
    shutdown();
}

ToolInfo ToolHost::info(int index) const
{
    return m_registry ? m_registry->infoAt(index) : ToolInfo();
}

ToolHost::State ToolHost::state(int index) const
{
    if (index < 0 || index >= m_slots.size())
        return State::Failed;
    return m_slots.at(index).state;
}

QString ToolHost::error(int index) const
{
    if (index < 0 || index >= m_slots.size())
        return {};
    return m_slots.at(index).error;
}

ToolView *ToolHost::view(int index) const
{
    if (index < 0 || index >= m_slots.size())
        return nullptr;
    return m_slots.at(index).view.data();
}

QWidget *ToolHost::currentWidget() const
{
    return m_stack ? m_stack->currentWidget() : nullptr;
}

void ToolHost::initializeAll()
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].state != State::Pending)
            continue;
        ITool *tool = m_registry->at(i);
        const ToolInfo toolInfo = m_registry->infoAt(i);
        ToolContext context;
        context.info = toolInfo;
        context.services = m_platform->servicesFor(toolInfo.id);
        context.dialogParent = m_dialogParent;

        QString message;
        bool ok = false;
        try {
            ok = tool->initialize(context, &message);
        } catch (const std::exception &e) {
            message = describeException(e);
        } catch (...) {
            message = QStringLiteral("未知异常");
        }
        if (ok) {
            m_slots[i].initialized = true;
            m_slots[i].state = State::Ready;
        } else {
            fail(i, message.isEmpty() ? QStringLiteral("初始化失败") : message);
        }
    }
}

void ToolHost::fail(int index, const QString &message)
{
    Slot &slot = m_slots[index];
    slot.state = State::Failed;
    slot.error = message;
    const ToolInfo toolInfo = m_registry->infoAt(index);
    m_platform->gate()->releaseAllFor(toolInfo.id);
    m_platform->servicesFor(QString()).logError(
        QStringLiteral("工具“%1”不可用：%2").arg(toolInfo.title, message));
    emit toolFailed(index, message);
}

bool ToolHost::createView(int index)
{
    Slot &slot = m_slots[index];
    slot.pagesAttempted = true;

    ITool *tool = m_registry->at(index);
    QVector<ToolPage> pages;
    QString message;
    try {
        pages = tool->createPages();
    } catch (const std::exception &e) {
        message = describeException(e);
    } catch (...) {
        message = QStringLiteral("创建页面时发生未知异常");
    }
    if (message.isEmpty())
        message = validatePages(pages);
    if (!message.isEmpty()) {
        QSet<QWidget *> uniqueWidgets;
        for (const ToolPage &page : pages) {
            if (page.widget)
                uniqueWidgets.insert(page.widget);
        }
        qDeleteAll(uniqueWidgets);
        fail(index, QStringLiteral("页面无效：%1").arg(message));
        return false;
    }

    auto *view = new ToolView(m_registry->infoAt(index), m_stack);
    for (const ToolPage &page : pages)
        view->addPage(page.info, page.widget);
    m_stack->addWidget(view);
    slot.view = view;
    emit viewCreated(index);
    return true;
}

QWidget *ToolHost::ensureUnavailableView(int index)
{
    Slot &slot = m_slots[index];
    if (slot.unavailable)
        return slot.unavailable.data();

    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("unavailableView"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setProperty("toolId", m_registry->infoAt(index).id);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(8);
    auto *title = new QLabel(QStringLiteral("“%1”当前不可用").arg(m_registry->infoAt(index).title));
    title->setObjectName(QStringLiteral("unavailableTitle"));
    auto *reason = new QLabel(slot.error.isEmpty() ? QStringLiteral("原因未知") : slot.error);
    reason->setObjectName(QStringLiteral("unavailableReason"));
    reason->setWordWrap(true);
    reason->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *hint = new QLabel(QStringLiteral("其他工具不受影响。修复后重新启动即可恢复。"));
    hint->setProperty("role", QStringLiteral("hint"));
    layout->addWidget(title);
    layout->addWidget(reason);
    layout->addWidget(hint);
    layout->addStretch(1);
    m_stack->addWidget(page);
    slot.unavailable = page;
    return page;
}

bool ToolHost::activate(int index)
{
    if (m_shutDown || index < 0 || index >= m_slots.size())
        return false;

    if (m_current >= 0 && m_current != index && m_current < m_slots.size()) {
        Slot &previous = m_slots[m_current];
        if (previous.active) {
            previous.active = false;
            const ToolInfo prevInfo = m_registry->infoAt(m_current);
            try {
                m_registry->at(m_current)->deactivated();
            } catch (const std::exception &e) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”停用回调异常：%2")
                        .arg(prevInfo.title, describeException(e)));
            } catch (...) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”停用回调发生未知异常").arg(prevInfo.title));
            }
        }
    }

    Slot &slot = m_slots[index];
    if (slot.state == State::Ready && !slot.view && !slot.pagesAttempted)
        createView(index);

    m_current = index;
    if (slot.state == State::Ready && slot.view) {
        m_stack->setCurrentWidget(slot.view);
        if (!slot.active) {
            slot.active = true;
            const ToolInfo curInfo = m_registry->infoAt(index);
            try {
                m_registry->at(index)->activated();
            } catch (const std::exception &e) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”激活回调异常：%2")
                        .arg(curInfo.title, describeException(e)));
            } catch (...) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”激活回调发生未知异常").arg(curInfo.title));
            }
        }
    } else {
        m_stack->setCurrentWidget(ensureUnavailableView(index));
    }
    emit toolActivated(index);
    return true;
}

void ToolHost::shutdown()
{
    if (m_shutDown)
        return;
    m_shutDown = true;
    for (int i = 0; i < m_slots.size(); ++i) {
        Slot &slot = m_slots[i];
        if (!slot.initialized)
            continue;
        ITool *tool = m_registry->at(i);
        const ToolInfo toolInfo = m_registry->infoAt(i);
        if (slot.active) {
            try {
                tool->deactivated();
            } catch (const std::exception &e) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”退出停用异常：%2")
                        .arg(toolInfo.title, describeException(e)));
            } catch (...) {
                m_platform->servicesFor(QString()).logError(
                    QStringLiteral("工具“%1”退出停用发生未知异常").arg(toolInfo.title));
            }
            slot.active = false;
        }
        try {
            tool->shutdown();
        } catch (const std::exception &e) {
            m_platform->servicesFor(QString()).logError(
                QStringLiteral("工具“%1”关闭清理异常：%2")
                    .arg(toolInfo.title, describeException(e)));
        } catch (...) {
            m_platform->servicesFor(QString()).logError(
                QStringLiteral("工具“%1”关闭清理发生未知异常").arg(toolInfo.title));
        }
        slot.initialized = false;
        m_platform->gate()->releaseAllFor(toolInfo.id);
    }
}

} // namespace tw
