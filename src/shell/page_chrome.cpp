#include "shell/page_chrome.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyle>
#include <QTableWidget>
#include <QVariant>

namespace tw {
namespace ui {
namespace {

// 横竖通用五圆点抓手分割条（..... / ⋮），1:1 还原工业软件分栏拖拽条。
class DottedSplitterHandle final : public QSplitterHandle
{
public:
    DottedSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
        : QSplitterHandle(orientation, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QSplitterHandle::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::workbench().chromeDisabled);
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        for (int i = -2; i <= 2; ++i) {
            if (orientation() == Qt::Vertical)
                p.drawEllipse(QPointF(cx + i * 5.0, cy), 1.1, 1.1);
            else
                p.drawEllipse(QPointF(cx, cy + i * 5.0), 1.1, 1.1);
        }
    }
};

class DottedSplitter final : public QSplitter
{
public:
    explicit DottedSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSplitter(orientation, parent)
    {
    }

protected:
    QSplitterHandle *createHandle() override
    {
        return new DottedSplitterHandle(orientation(), this);
    }
};

} // namespace

QLabel *makeLabel(const QString &text, const char *role, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("role", QString::fromLatin1(role));
    label->setWordWrap(true);
    return label;
}

void setRole(QWidget *widget, const char *role)
{
    if (!widget)
        return;
    const QString next = QString::fromLatin1(role);
    if (widget->property("role").toString() == next)
        return;
    widget->setProperty("role", next);
    if (QStyle *s = widget->style()) {
        s->unpolish(widget);
        s->polish(widget);
    }
    widget->update();
}

QFrame *makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setProperty("role", QStringLiteral("card"));
    return card;
}

QFrame *makeToolbar(QWidget *parent)
{
    auto *bar = new QFrame(parent);
    bar->setProperty("role", QStringLiteral("toolbar"));
    return bar;
}

QPushButton *makeActionButton(const QString &text, const QString &iconKey, QWidget *parent)
{
    const Theme &theme = Theme::workbench();
    auto *btn = new QPushButton(text, parent);
    if (!iconKey.isEmpty()) {
        btn->setIcon(IconFactory::icon(iconKey, theme.primary, theme.primary,
                                       theme.disabledText, theme.accent));
        btn->setIconSize(QSize(15, 15));
    }
    return btn;
}

QSplitter *makeSplitter(Qt::Orientation orientation, QWidget *parent)
{
    auto *splitter = new DottedSplitter(orientation, parent);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    return splitter;
}

void configureDataTable(QTableWidget *table, const QStringList &headers)
{
    if (!table)
        return;
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(false);
    table->setShowGrid(true);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignLeft);
}

QFrame *makeTableRow(const QStringList &columns, const QList<int> &widths,
                     bool header, bool alternate)
{
    auto *row = new QFrame;
    row->setProperty("role", QString::fromLatin1(header ? "tableHeader"
                                                        : (alternate ? "tableRowAlt" : "tableRow")));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(14);

    for (int i = 0; i < columns.size(); ++i) {
        const char *role = header
            ? "fieldLabel"
            : (i == 1 ? "code" : (i == columns.size() - 1 ? "value" : "pageSubtitle"));
        auto *lbl = makeLabel(columns.at(i), role);
        if (!header && i == columns.size() - 1)
            lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (i < widths.size() && widths.at(i) > 0) {
            lbl->setWordWrap(false);
            lbl->setFixedWidth(widths.at(i));
        }
        layout->addWidget(lbl, (i < widths.size() && widths.at(i) > 0) ? 0 : 1);
    }
    return row;
}

} // namespace ui
} // namespace tw
