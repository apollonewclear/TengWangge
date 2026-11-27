#include "shell/tool_rail.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QStyledItemDelegate>

namespace tw {
namespace {

constexpr int kItemWidth = 74;
constexpr int kItemHeight = 60;
constexpr int kIconSize = 30;

class ToolRailDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(kItemWidth, kItemHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const Theme &theme = Theme::workbench();
        const bool enabled = option.state & QStyle::State_Enabled;
        const bool selected = option.state & QStyle::State_Selected;
        const bool available = index.data(ToolRail::AvailableRole).toBool();
        const bool hovered = (option.state & QStyle::State_MouseOver) && enabled && available;
        const bool focused = (option.state & QStyle::State_HasFocus) && enabled;
        const QRect rect = option.rect;
        const QRect box = rect.adjusted(3, 2, -3, -2);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        if (selected && enabled && available) {
            painter->setPen(QPen(focused ? theme.primary : theme.chromeSelectedBorder, 1));
            painter->setBrush(theme.chromeSelected);
            painter->drawRoundedRect(box, 6, 6);
        } else if (selected) {
            painter->setPen(QPen(theme.border, 1));
            painter->setBrush(theme.borderSoft);
            painter->drawRoundedRect(box, 6, 6);
        } else if (hovered) {
            painter->setPen(QPen(theme.borderSoft, 1));
            painter->setBrush(theme.chromeHover);
            painter->drawRoundedRect(box, 6, 6);
        } else if (focused) {
            painter->setPen(QPen(theme.primary, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(box, 6, 6);
        }

        const QString iconKey = index.data(ToolRail::IconKeyRole).toString();
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 2.0;
        const QPixmap icon = (!enabled || !available)
            ? IconFactory::pixmap(iconKey, kIconSize, theme.disabledText, dpr, theme.disabledText)
            : IconFactory::pixmap(iconKey, kIconSize, theme.primary, dpr, theme.accent);

        const int iconX = rect.left() + (rect.width() - kIconSize) / 2;
        const int iconY = rect.top() + 5;
        painter->drawPixmap(iconX, iconY, icon);

        if (!available) {
            // 右上角红色故障小圆点，标示模块当前不可用
            painter->setPen(Qt::NoPen);
            painter->setBrush(theme.danger);
            painter->drawEllipse(QPointF(rect.right() - 12, rect.top() + 9), 3.5, 3.5);
        }

        const int basePx = option.font.pixelSize() > 0 ? option.font.pixelSize()
                                                       : QFontInfo(option.font).pixelSize();
        QFont font = option.font;
        font.setPixelSize(qBound(11, basePx - 1, 13));
        font.setBold(selected && available);
        painter->setFont(font);

        QColor textColor = theme.chromeText;
        if (!enabled)
            textColor = selected ? theme.textMuted : theme.disabledText;
        else if (!available)
            textColor = theme.disabledText;
        painter->setPen(textColor);

        const QFontMetrics metrics(font);
        const QRect textRect(rect.left() + 4, rect.top() + 38, rect.width() - 8, 18);
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter,
                          metrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                             Qt::ElideRight, textRect.width()));
        painter->restore();
    }
};

} // namespace

ToolRail::ToolRail(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName(QStringLiteral("toolRail"));
    setAccessibleName(QStringLiteral("工具模块栏"));
    setItemDelegate(new ToolRailDelegate(this));
    setFlow(QListView::LeftToRight);
    setWrapping(false);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setUniformItemSizes(true);
    setFixedHeight(kItemHeight);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setToolTip(QString());
}

void ToolRail::setTools(const QVector<ToolInfo> &tools)
{
    clear();
    for (int i = 0; i < tools.size(); ++i) {
        const ToolInfo &info = tools.at(i);
        auto *item = new QListWidgetItem(info.title);
        const QString shortcutHint = i < 9
            ? QStringLiteral(" (Ctrl+%1)").arg(i + 1)
            : QString();
        const QString tip = (info.summary.isEmpty() ? info.title : info.summary) + shortcutHint;
        item->setData(IconKeyRole, info.iconKey);
        item->setData(AvailableRole, true);
        item->setData(SummaryRole, tip);
        if (i < 9)
            item->setData(ShortcutRole, QString::number(i + 1));
        item->setToolTip(tip);
        item->setData(Qt::AccessibleTextRole, info.title);
        item->setSizeHint(QSize(kItemWidth, kItemHeight));
        addItem(item);
    }
    updateGeometry();
}

void ToolRail::setAvailable(int index, bool available, const QString &reason)
{
    QListWidgetItem *entry = item(index);
    if (!entry)
        return;
    entry->setData(AvailableRole, available);
    if (!available)
        entry->setToolTip(reason.isEmpty() ? QStringLiteral("工具不可用") : reason);
    else
        entry->setToolTip(entry->data(SummaryRole).toString());
    viewport()->update();
}

bool ToolRail::isAvailable(int index) const
{
    const QListWidgetItem *entry = item(index);
    return entry && entry->data(AvailableRole).toBool();
}

QSize ToolRail::sizeHint() const
{
    return QSize(kItemWidth * qMax(1, count()) + 4, kItemHeight);
}

void ToolRail::keyPressEvent(QKeyEvent *event)
{
    // 横向工具带同时兼容 ←/→ 与 ↑/↓ 逐项切换，到头不循环。
    if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) {
            if (currentRow() > 0)
                setCurrentRow(currentRow() - 1);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) {
            if (currentRow() + 1 < count())
                setCurrentRow(currentRow() + 1);
            event->accept();
            return;
        }
    }
    QListWidget::keyPressEvent(event);
}

} // namespace tw
