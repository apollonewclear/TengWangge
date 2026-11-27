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

    m_strip = new QFrame;
    m_strip->setObjectName(QStringLiteral("pageStrip"));
    m_strip->setFixedHeight(32);
    m_strip->hide(); // 单页面工具自动隐藏页签条，多页面时自动显示
    auto *stripLayout = new QHBoxLayout(m_strip);
    stripLayout->setContentsMargins(8, 0, 8, 0);
    stripLayout->setSpacing(0);

    m_tabs = new QTabBar;
    m_tabs->setObjectName(QStringLiteral("pageTabs"));
    m_tabs->setAccessibleName(QStringLiteral("%1 页面").arg(info.title));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->setUsesScrollButtons(false);
    m_tabs->setIconSize(QSize(14, 14));
    m_tabs->setFocusPolicy(Qt::StrongFocus);
    m_tabs->setChangeCurrentOnDrag(false);
    m_tabs->installEventFilter(this);
    m_tabs->setToolTip(QStringLiteral("左右方向键切换页面，Ctrl+PageUp / Ctrl+PageDown 也可以"));
    stripLayout->addWidget(m_tabs, 1);
    layout->addWidget(m_strip);

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
    m_strip->setVisible(m_pages.size() > 1);
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
        const bool enabled = m_tabs->isEnabled();
        const bool selected = (i == active) && enabled;
        const QIcon icon = !enabled
            ? IconFactory::icon(key, theme.disabledText, theme.disabledText, theme.disabledText, theme.disabledText)
            : (selected
                   ? IconFactory::icon(key, theme.primary, theme.primary, theme.disabledText, theme.accent)
                   : IconFactory::icon(key, theme.textMuted, theme.primary, theme.disabledText, theme.accent));
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
