#include "shell/tool_hub_view.h"

#include "shell/icon_factory.h"
#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace tw {
namespace {

class ToolCard final : public QFrame
{
public:
    ToolCard(int index, const ToolInfo &info, bool available, const QString &error,
             std::function<void(int)> onClick, QWidget *parent = nullptr)
        : QFrame(parent)
        , m_index(index)
        , m_onClick(std::move(onClick))
    {
        const Theme &theme = Theme::workbench();
        setObjectName(QStringLiteral("toolHubCard"));
        setProperty("available", available);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(92);
        setMinimumWidth(210);
        setToolTip(available
                       ? (info.summary.isEmpty() ? info.title : info.summary)
                       : (error.isEmpty() ? QStringLiteral("工具当前不可用") : error));

        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(12, 10, 12, 10);
        row->setSpacing(10);

        auto *iconLbl = new QLabel;
        iconLbl->setFixedSize(36, 36);
        iconLbl->setAlignment(Qt::AlignCenter);
        const QPixmap px = available
            ? IconFactory::pixmap(info.iconKey, 30, theme.primary, 2.0, theme.accent)
            : IconFactory::pixmap(info.iconKey, 30, theme.disabledText, 2.0, theme.disabledText);
        iconLbl->setPixmap(px);
        row->addWidget(iconLbl, 0, Qt::AlignTop);

        auto *col = new QVBoxLayout;
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(3);

        auto *head = new QHBoxLayout;
        head->setContentsMargins(0, 0, 0, 0);
        head->setSpacing(6);
        auto *titleLbl = new QLabel(info.title);
        titleLbl->setObjectName(QStringLiteral("hubCardTitle"));
        head->addWidget(titleLbl);

        auto *catLbl = new QLabel(available ? info.effectiveCategory() : QStringLiteral("不可用"));
        catLbl->setObjectName(available ? QStringLiteral("hubCardTag") : QStringLiteral("hubCardTagErr"));
        head->addWidget(catLbl);
        head->addStretch(1);
        col->addLayout(head);

        auto *summaryLbl = new QLabel(available
                                          ? (info.summary.isEmpty() ? info.id : info.summary)
                                          : (error.isEmpty() ? QStringLiteral("初始化失败") : error));
        summaryLbl->setObjectName(QStringLiteral("hubCardSummary"));
        summaryLbl->setWordWrap(true);
        col->addWidget(summaryLbl, 1);

        auto *idLbl = new QLabel(info.id);
        idLbl->setObjectName(QStringLiteral("hubCardId"));
        col->addWidget(idLbl);

        row->addLayout(col, 1);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_onClick)
            m_onClick(m_index);
        QFrame::mouseReleaseEvent(event);
    }

private:
    int m_index = -1;
    std::function<void(int)> m_onClick;
};

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

} // namespace

ToolHubView::ToolHubView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("toolHubView"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(0);

    QSplitter *splitter = ui::makeSplitter(Qt::Horizontal, this);

    // 左侧：业务分类统计表
    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(ui::makeLabel(QStringLiteral("工具业务分类"), "paneTitle"));

    m_catTable = new QTableWidget;
    m_catTable->setAccessibleName(QStringLiteral("工具中心分类表"));
    ui::configureDataTable(m_catTable, {QStringLiteral("分类名称"), QStringLiteral("数量")});
    m_catTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_catTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_catTable->setColumnWidth(1, 56);
    leftLayout->addWidget(m_catTable, 1);
    leftPane->setMinimumWidth(185);
    leftPane->setMaximumWidth(250);

    // 右侧：检索栏 + 自适应多列卡片网格
    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);
    m_rightTitle = ui::makeLabel(QStringLiteral("工具中心 — 全部工具"), "paneTitle");
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("检索工具名称、编码、分类或简介..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(250);
    m_searchEdit->setAccessibleName(QStringLiteral("工具中心搜索框"));
    topRow->addWidget(m_rightTitle);
    topRow->addStretch(1);
    topRow->addWidget(m_searchEdit);
    rightLayout->addLayout(topRow);

    m_scroll = new QScrollArea;
    m_scroll->setObjectName(QStringLiteral("hubScrollArea"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_gridHost = new QWidget;
    m_gridHost->setObjectName(QStringLiteral("hubGridHost"));
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(2, 2, 6, 8);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(10);
    m_scroll->setWidget(m_gridHost);
    rightLayout->addWidget(m_scroll, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 940});
    root->addWidget(splitter, 1);

    connect(m_catTable, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
                if (row >= 0 && m_catTable->item(row, 0)) {
                    m_selectedCategory = m_catTable->item(row, 0)->data(Qt::UserRole).toString();
                    rebuildCards();
                }
            });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        rebuildCards();
    });
}

void ToolHubView::setTools(const QVector<ToolInfo> &tools)
{
    m_entries.clear();
    m_categories.clear();
    for (const ToolInfo &info : tools) {
        m_entries.append({info, true, QString()});
        const QString cat = info.effectiveCategory();
        if (!m_categories.contains(cat))
            m_categories.append(cat);
    }
    m_selectedCategory.clear();
    rebuildCategoryTable();
    rebuildCards();
}

void ToolHubView::setAvailable(int index, bool available, const QString &reason)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_entries[index].available = available;
    m_entries[index].error = reason;
    rebuildCards();
}

void ToolHubView::selectCategory(const QString &category)
{
    for (int r = 0; r < m_catTable->rowCount(); ++r) {
        if (m_catTable->item(r, 0)->data(Qt::UserRole).toString() == category) {
            m_catTable->selectRow(r);
            return;
        }
    }
}

void ToolHubView::setSearchText(const QString &text)
{
    m_searchEdit->setText(text);
}

void ToolHubView::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

int ToolHubView::categoryCount() const
{
    return m_categories.size();
}

void ToolHubView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int cols = computeColumns();
    if (cols != m_lastCols && !m_entries.isEmpty())
        rebuildCards();
}

int ToolHubView::computeColumns() const
{
    const int w = m_scroll && m_scroll->viewport() ? m_scroll->viewport()->width() : 860;
    return qBound(2, (w + 10) / 245, 5);
}

void ToolHubView::rebuildCategoryTable()
{
    m_catTable->setRowCount(0);
    m_catTable->insertRow(0);
    m_catTable->setRowHeight(0, 26);
    QTableWidgetItem *allItem = makeCell(QStringLiteral("全部工具"));
    allItem->setData(Qt::UserRole, QString());
    m_catTable->setItem(0, 0, allItem);
    m_catTable->setItem(0, 1, makeCell(QString::number(m_entries.size()), Qt::AlignCenter));

    for (const QString &cat : m_categories) {
        int count = 0;
        for (const Entry &e : m_entries) {
            if (e.info.effectiveCategory() == cat)
                ++count;
        }
        const int row = m_catTable->rowCount();
        m_catTable->insertRow(row);
        m_catTable->setRowHeight(row, 26);
        QTableWidgetItem *cItem = makeCell(cat);
        cItem->setData(Qt::UserRole, cat);
        m_catTable->setItem(row, 0, cItem);
        m_catTable->setItem(row, 1, makeCell(QString::number(count), Qt::AlignCenter));
    }
    m_catTable->selectRow(0);
}

void ToolHubView::rebuildCards()
{
    while (QLayoutItem *child = m_grid->takeAt(0)) {
        if (QWidget *w = child->widget())
            w->deleteLater();
        delete child;
    }

    const QString kw = m_searchEdit->text().trimmed();
    m_visibleIndices.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries.at(i);
        if (!m_selectedCategory.isEmpty() && e.info.effectiveCategory() != m_selectedCategory)
            continue;
        if (!kw.isEmpty()) {
            const bool hit = e.info.title.contains(kw, Qt::CaseInsensitive)
                          || e.info.id.contains(kw, Qt::CaseInsensitive)
                          || e.info.summary.contains(kw, Qt::CaseInsensitive)
                          || e.info.effectiveCategory().contains(kw, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }
        m_visibleIndices.append(i);
    }

    const int cols = computeColumns();
    m_lastCols = cols;

    for (int k = 0; k < m_visibleIndices.size(); ++k) {
        const int idx = m_visibleIndices.at(k);
        const Entry &e = m_entries.at(idx);
        auto *card = new ToolCard(idx, e.info, e.available, e.error,
                                  [this](int toolIdx) { emit toolActivated(toolIdx); });
        m_grid->addWidget(card, k / cols, k % cols);
    }

    if (m_visibleIndices.isEmpty()) {
        QLabel *empty = ui::makeLabel(QStringLiteral("未找到匹配的工具模块"), "hint");
        empty->setAlignment(Qt::AlignCenter);
        m_grid->addWidget(empty, 0, 0, 1, cols);
    }

    const int lastRow = (m_visibleIndices.size() + cols - 1) / cols;
    m_grid->setRowStretch(lastRow, 1);
    for (int c = 0; c < cols; ++c)
        m_grid->setColumnStretch(c, 1);

    const QString catTitle = m_selectedCategory.isEmpty() ? QStringLiteral("全部工具") : m_selectedCategory;
    m_rightTitle->setText(QStringLiteral("工具中心 — %1（共 %2 项）").arg(catTitle).arg(m_visibleIndices.size()));
}

} // namespace tw
