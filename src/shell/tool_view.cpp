#include "shell/tool_view.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QVBoxLayout>
#include <QVariant>

namespace tw {

ToolView::ToolView(const ToolInfo &info, QWidget *parent)
    : QWidget(parent)
    , m_info(info)
{
    setObjectName(QStringLiteral("toolView"));
    setProperty("toolId", info.id);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *strip = new QFrame;
    strip->setObjectName(QStringLiteral("pageStrip"));
    strip->setFixedHeight(42);
    auto *stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(10, 0, 10, 0);
    stripLayout->setSpacing(0);

    m_tabs = new QTabBar;
    m_tabs->setObjectName(QStringLiteral("pageTabs"));
    m_tabs->setAccessibleName(QStringLiteral("%1 页面").arg(info.title));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setIconSize(QSize(16, 16));
    m_tabs->setFocusPolicy(Qt::StrongFocus);
    m_tabs->setChangeCurrentOnDrag(false);
    m_tabs->installEventFilter(this);
    m_tabs->setToolTip(QStringLiteral("左右方向键切换页面，Ctrl+PageUp / Ctrl+PageDown 也可以"));
    stripLayout->addWidget(m_tabs);
    stripLayout->addStretch(1);
    layout->addWidget(strip);

    m_stack = new QStackedWidget;
    m_stack->setObjectName(QStringLiteral("pageStack"));
    layout->addWidget(m_stack, 1);

    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index < m_stack->count())
            m_stack->setCurrentIndex(index);
        refreshTabIcons();
        emit currentPageChanged(index);
    });
}

// QSS 的 :focus::tab:selected 在 Qt 5.15 下会错误命中非选中页签，
// 所以用属性 tabFocus 自己表达“页面条拥有键盘焦点”。
bool ToolView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        m_tabs->setProperty("tabFocus", event->type() == QEvent::FocusIn);
        m_tabs->style()->unpolish(m_tabs);
        m_tabs->style()->polish(m_tabs);
    }
    return QWidget::eventFilter(watched, event);
}

void ToolView::addPage(const PageInfo &info, QWidget *widget)
{
    m_pages.append(info);
    m_pageWidgets.append(widget);

    // 外包无边框 QScrollArea，防止小窗口或展开日志面板时业务表单被截断。
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("pageScrollArea"));
    if (scroll->viewport())
        scroll->viewport()->setObjectName(QStringLiteral("pageScrollViewport"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setWidget(widget);
    m_stack->addWidget(scroll);

    const int tab = m_tabs->addTab(info.title);
    m_tabs->setTabToolTip(tab, info.summary.isEmpty() ? info.title : info.summary);
    refreshTabIcons();
}

void ToolView::refreshTabIcons()
{
    const Theme &theme = Theme::workbench();
    const int active = m_tabs->currentIndex();
    for (int i = 0; i < m_pages.size() && i < m_tabs->count(); ++i) {
        const QString &key = m_pages.at(i).iconKey;
        if (key.isEmpty())
            continue;
        const bool selected = (i == active) && m_tabs->isEnabled();
        const QIcon icon = selected
            ? IconFactory::icon(key, theme.panel, theme.panel, theme.disabledText, theme.panel)
            : IconFactory::icon(key, theme.primary, theme.panel, theme.disabledText, theme.accent);
        m_tabs->setTabIcon(i, icon);
    }
}

int ToolView::currentPage() const
{
    return m_tabs->currentIndex();
}

PageInfo ToolView::pageInfo(int index) const
{
    if (index < 0 || index >= m_pages.size())
        return {};
    return m_pages.at(index);
}

QWidget *ToolView::pageWidget(int index) const
{
    if (index < 0 || index >= m_pageWidgets.size())
        return nullptr;
    return m_pageWidgets.at(index);
}

int ToolView::indexOfPage(const QString &pageId) const
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).id == pageId)
            return i;
    }
    return -1;
}

bool ToolView::setCurrentPage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return false;
    m_tabs->setCurrentIndex(index);
    return true;
}

bool ToolView::stepPage(int delta)
{
    return setCurrentPage(currentPage() + delta) && delta != 0;
}

void ToolView::setNavigationEnabled(bool enabled)
{
    m_tabs->setEnabled(enabled);
    refreshTabIcons();
}

} // namespace tw
