#include "shell/page_chrome.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVariant>

namespace tw {
namespace ui {

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

QFrame *makeTableRow(const QStringList &columns, const QList<int> &widths,
                     bool header, bool alternate)
{
    auto *row = new QFrame;
    row->setProperty("role", QString::fromLatin1(header ? "tableHeader"
                                                        : (alternate ? "tableRowAlt" : "tableRow")));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 7, 12, 7);
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
