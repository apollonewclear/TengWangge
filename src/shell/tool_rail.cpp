#include "shell/tool_rail.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QStyledItemDelegate>

namespace tw {
namespace {

constexpr int kRowHeight = 38;
constexpr int kAccentWidth = 3;
constexpr int kIconSize = 18;

class ToolRailDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return QSize(option.rect.width(), kRowHeight);
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

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

        const bool activeSelected = selected && enabled && available;
        if (activeSelected) {
            painter->fillRect(rect, theme.chromeSelected);
        } else if (selected) {
            // 导航锁定或工具不可用时：灰底 + 灰竖条，保留当前位置可辨识但绝不伪装成正常激活态
            painter->fillRect(rect, theme.borderSoft);
            painter->fillRect(QRect(rect.left(), rect.top(), kAccentWidth, rect.height()),
                              theme.chromeDisabled);
        } else if (hovered) {
            painter->fillRect(rect, theme.chromeHover);
        } else {
            painter->fillRect(rect, theme.panel);
        }

        // 底部分隔细线，贴合工业软件列表/树行视觉
        painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), 1), theme.borderSoft);

        QColor textColor = theme.text;
        if (activeSelected)
            textColor = theme.panel;
        else if (!enabled)
            textColor = selected ? theme.textMuted : theme.disabledText;
        else if (!available)
            textColor = theme.disabledText;

        const QString iconKey = index.data(ToolRail::IconKeyRole).toString();
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 2.0;
        QPixmap icon;
        if (activeSelected) {
            icon = IconFactory::pixmap(iconKey, kIconSize, theme.panel, dpr, theme.panel);
        } else if (!enabled || !available) {
            icon = IconFactory::pixmap(iconKey, kIconSize, theme.disabledText, dpr, theme.disabledText);
        } else {
            icon = IconFactory::pixmap(iconKey, kIconSize, theme.primary, dpr, theme.accent);
        }
        const int iconY = rect.top() + (rect.height() - kIconSize) / 2;
        painter->drawPixmap(rect.left() + 10, iconY, icon);

        QFont font = option.font;
        font.setBold(selected);
        painter->setFont(font);
        painter->setPen(textColor);

        const QString shortcut = index.data(ToolRail::ShortcutRole).toString();
        const QFontMetrics metrics(font);
        int rightEdge = rect.right() - 8;

        const int basePx = option.font.pixelSize() > 0 ? option.font.pixelSize()
                                                       : QFontInfo(option.font).pixelSize();
        QFont smallFont = option.font;
        smallFont.setPixelSize(qMax(10, basePx - 2));
        smallFont.setBold(false);
        const QFontMetrics smallMetrics(smallFont);

        if (!available) {
            const QString badge = QStringLiteral("不可用");
            const QRect badgeRect(rightEdge - smallMetrics.horizontalAdvance(badge) - 10,
                                  rect.top() + (rect.height() - 16) / 2,
                                  smallMetrics.horizontalAdvance(badge) + 10, 16);
            painter->setFont(smallFont);
            painter->setPen(QPen(theme.danger, 1));
            painter->drawRoundedRect(badgeRect, 2, 2);
            painter->drawText(badgeRect, Qt::AlignCenter, badge);
            rightEdge = badgeRect.left() - 6;
            painter->setFont(font);
            painter->setPen(textColor);
        } else if (!shortcut.isEmpty()) {
            painter->setFont(smallFont);
            const QColor hintColor = activeSelected ? theme.selectionSoft
                : (enabled ? theme.chromeDisabled : theme.disabledText);
            painter->setPen(hintColor);
            const QString hint = QStringLiteral("Ctrl+") + shortcut;
            const int hintWidth = smallMetrics.horizontalAdvance(hint);
            painter->drawText(QRect(rightEdge - hintWidth, rect.top(), hintWidth, rect.height()),
                              Qt::AlignVCenter | Qt::AlignRight, hint);
            rightEdge -= hintWidth + 8;
            painter->setFont(font);
            painter->setPen(textColor);
        }

        const int textLeft = rect.left() + 10 + kIconSize + 8;
        const QRect textRect(textLeft, rect.top(), qMax(0, rightEdge - textLeft), rect.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          metrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                             textRect.width()));

        if (focused) {
            const QColor focusColor = activeSelected ? theme.accent : theme.primary;
            painter->setPen(QPen(focusColor, 1, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect.adjusted(1, 1, -2, -2));
        }
        painter->restore();
    }
};

} // namespace

ToolRail::ToolRail(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName(QStringLiteral("toolRail"));
    setAccessibleName(QStringLiteral("工具列表"));
    setItemDelegate(new ToolRailDelegate(this));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setFrameShape(QFrame::StyledPanel);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setUniformItemSizes(true);
    setToolTip(QString());
}

void ToolRail::setTools(const QVector<ToolInfo> &tools)
{
    clear();
    for (int i = 0; i < tools.size(); ++i) {
        const ToolInfo &info = tools.at(i);
        auto *item = new QListWidgetItem(info.title);
        const QString tip = info.summary.isEmpty() ? info.title : info.summary;
        item->setData(IconKeyRole, info.iconKey);
        item->setData(AvailableRole, true);
        item->setData(SummaryRole, tip);
        if (i < 9)
            item->setData(ShortcutRole, QString::number(i + 1));
        item->setToolTip(tip);
        item->setData(Qt::AccessibleTextRole, info.title);
        addItem(item);
    }
}

void ToolRail::setAvailable(int index, bool available, const QString &reason)
{
    QListWidgetItem *entry = item(index);
    if (!entry)
        return;
    entry->setData(AvailableRole, available);
    if (!available) {
        entry->setToolTip(reason.isEmpty() ? QStringLiteral("工具不可用") : reason);
    } else {
        entry->setToolTip(entry->data(SummaryRole).toString());
    }
    viewport()->update();
}

bool ToolRail::isAvailable(int index) const
{
    const QListWidgetItem *entry = item(index);
    return entry && entry->data(AvailableRole).toBool();
}

QSize ToolRail::sizeHint() const
{
    return QSize(184, kRowHeight * qMax(1, count()) + 4);
}

} // namespace tw
