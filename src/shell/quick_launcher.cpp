#include "shell/quick_launcher.h"

#include "shell/icon_factory.h"
#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace tw {

QuickLauncherDialog::QuickLauncherDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("quickLauncher"));
    setWindowTitle(QStringLiteral("快速切换工具 (Ctrl+K)"));
    resize(520, 360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(8);

    m_query = new QLineEdit;
    m_query->setObjectName(QStringLiteral("quickLauncherInput"));
    m_query->setPlaceholderText(QStringLiteral("输入工具名称、编码、分类或简介，↑↓ 选择，回车直达..."));
    m_query->setClearButtonEnabled(true);
    m_query->setAccessibleName(QStringLiteral("快速搜索工具"));
    m_query->installEventFilter(this);
    layout->addWidget(m_query);

    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("quickLauncherList"));
    m_list->setIconSize(QSize(20, 20));
    m_list->setUniformItemSizes(true);
    layout->addWidget(m_list, 1);

    layout->addWidget(ui::makeLabel(QStringLiteral("提示：回车打开选中工具，Esc 关闭，Ctrl+0 返回工具中心。"), "hint"));

    connect(m_query, &QLineEdit::textChanged, this, [this](const QString &) { rebuildList(); });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) { acceptCurrent(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { acceptCurrent(); });
}

void QuickLauncherDialog::setTools(const QVector<ToolInfo> &tools, const QVector<bool> &available)
{
    m_tools = tools;
    m_available = available;
    m_selectedIndex = -1;
    m_query->clear();
    rebuildList();
    m_query->setFocus();
}

void QuickLauncherDialog::setQuery(const QString &query)
{
    m_query->setText(query);
}

int QuickLauncherDialog::matchedCount() const
{
    return m_list->count();
}

bool QuickLauncherDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_query && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Down && m_list->count() > 0) {
            const int next = qMin(m_list->count() - 1, m_list->currentRow() + 1);
            m_list->setCurrentRow(next);
            return true;
        }
        if (ke->key() == Qt::Key_Up && m_list->count() > 0) {
            const int prev = qMax(0, m_list->currentRow() - 1);
            m_list->setCurrentRow(prev);
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            acceptCurrent();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickLauncherDialog::rebuildList()
{
    const Theme &theme = Theme::workbench();
    const QString kw = m_query->text().trimmed();
    m_list->clear();

    for (int i = 0; i < m_tools.size(); ++i) {
        const ToolInfo &info = m_tools.at(i);
        const bool avail = (i < m_available.size()) ? m_available.at(i) : true;
        if (!kw.isEmpty()) {
            const bool hit = info.title.contains(kw, Qt::CaseInsensitive)
                          || info.id.contains(kw, Qt::CaseInsensitive)
                          || info.summary.contains(kw, Qt::CaseInsensitive)
                          || info.effectiveCategory().contains(kw, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }
        const QString text = QStringLiteral("[%1]  %2  (%3)  —  %4")
                                 .arg(info.effectiveCategory(),
                                      info.title,
                                      info.id,
                                      avail ? (info.summary.isEmpty() ? info.title : info.summary)
                                            : QStringLiteral("不可用"));
        auto *item = new QListWidgetItem(
            avail ? IconFactory::icon(info.iconKey, theme.primary, theme.primary, theme.disabledText, theme.accent)
                  : IconFactory::icon(info.iconKey, theme.disabledText, theme.disabledText, theme.disabledText, theme.disabledText),
            text);
        item->setData(Qt::UserRole, i);
        item->setSizeHint(QSize(0, 32));
        m_list->addItem(item);
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void QuickLauncherDialog::acceptCurrent()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur)
        return;
    m_selectedIndex = cur->data(Qt::UserRole).toInt();
    accept();
}

} // namespace tw
