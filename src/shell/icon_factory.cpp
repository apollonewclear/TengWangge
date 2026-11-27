#include "shell/icon_factory.h"

#include "shell/theme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmapCache>
#include <QPolygonF>

namespace tw {
namespace {

// 在 16x16 的逻辑坐标里绘制双色或单色工业线性图标。
void drawGlyph(QPainter &p, const QString &key, const QColor &color, const QColor &accent)
{
    const QColor secondary = accent.isValid() ? accent : color;
    const QPen primaryPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen accentPen(secondary, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    p.setPen(primaryPen);
    p.setBrush(Qt::NoBrush);

    if (key == QLatin1String("overview")) {
        // 类似“算法定义”的四宫格图标：三个主色方块 + 右上暖色点缀方块
        p.drawRoundedRect(QRectF(2.0, 2.0, 5.0, 5.0), 1.0, 1.0);
        p.drawRoundedRect(QRectF(2.0, 9.0, 5.0, 5.0), 1.0, 1.0);
        p.drawRoundedRect(QRectF(9.0, 9.0, 5.0, 5.0), 1.0, 1.0);
        p.setPen(accentPen);
        p.drawRoundedRect(QRectF(9.0, 2.0, 5.0, 5.0), 1.0, 1.0);
    } else if (key == QLatin1String("task")) {
        p.drawEllipse(QRectF(1.5, 1.5, 13.0, 13.0));
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        p.drawPolygon(QPolygonF({QPointF(6.3, 4.8), QPointF(11.5, 8.0), QPointF(6.3, 11.2)}));
    } else if (key == QLatin1String("status")) {
        QPolygonF pulse;
        pulse << QPointF(1.5, 8.5) << QPointF(5.0, 8.5) << QPointF(7.0, 3.0)
              << QPointF(9.5, 13.0) << QPointF(11.0, 8.5) << QPointF(14.5, 8.5);
        p.setPen(accentPen);
        p.drawPolyline(pulse);
    } else if (key == QLatin1String("info")) {
        p.drawEllipse(QRectF(1.5, 1.5, 13.0, 13.0));
        p.drawLine(QPointF(8.0, 7.3), QPointF(8.0, 11.5));
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        p.drawEllipse(QPointF(8.0, 4.7), 1.0, 1.0);
    } else if (key == QLatin1String("environment")) {
        p.drawRoundedRect(QRectF(1.5, 2.3, 13.0, 8.8), 1.2, 1.2);
        p.drawLine(QPointF(5.2, 14.0), QPointF(10.8, 14.0));
        p.drawLine(QPointF(8.0, 11.2), QPointF(8.0, 14.0));
        p.setPen(accentPen);
        p.drawLine(QPointF(4.2, 6.7), QPointF(6.8, 6.7));
        p.drawLine(QPointF(8.8, 6.7), QPointF(11.8, 6.7));
    } else if (key == QLatin1String("log")) {
        p.drawLine(QPointF(5.0, 4.0), QPointF(13.5, 4.0));
        p.drawLine(QPointF(5.0, 8.0), QPointF(13.5, 8.0));
        p.drawLine(QPointF(5.0, 12.0), QPointF(11.5, 12.0));
        p.setPen(accentPen);
        p.drawPoint(QPointF(2.6, 4.0));
        p.drawPoint(QPointF(2.6, 8.0));
        p.drawPoint(QPointF(2.6, 12.0));
    } else if (key == QLatin1String("lock")) {
        p.drawRoundedRect(QRectF(3.0, 7.0, 10.0, 7.0), 1.2, 1.2);
        p.setPen(accentPen);
        QPainterPath shackle;
        shackle.moveTo(5.2, 7.0);
        shackle.lineTo(5.2, 5.0);
        shackle.arcTo(QRectF(5.2, 2.0, 5.6, 6.0), 180, -180);
        shackle.lineTo(10.8, 7.0);
        p.drawPath(shackle);
    } else if (key == QLatin1String("refresh")) {
        p.setPen(accentPen);
        QPainterPath arc;
        arc.arcMoveTo(QRectF(2.2, 2.2, 11.6, 11.6), 35);
        arc.arcTo(QRectF(2.2, 2.2, 11.6, 11.6), 35, 290);
        p.drawPath(arc);
        p.drawPolyline(QPolygonF({QPointF(10.4, 2.0), QPointF(13.4, 4.5), QPointF(10.0, 5.6)}));
    } else if (key == QLatin1String("help")) {
        p.drawEllipse(QRectF(1.5, 1.5, 13.0, 13.0));
        QPainterPath qmark;
        qmark.moveTo(6.3, 5.6);
        qmark.arcTo(QRectF(6.0, 3.8, 4.0, 3.2), 160, -250);
        qmark.lineTo(8.0, 9.0);
        p.setPen(accentPen);
        p.drawPath(qmark);
        p.drawPoint(QPointF(8.0, 11.6));
    } else if (key == QLatin1String("export")) {
        p.drawPolyline(QPolygonF({QPointF(3.0, 10.0), QPointF(3.0, 13.5),
                                  QPointF(13.0, 13.5), QPointF(13.0, 10.0)}));
        p.setPen(accentPen);
        p.drawLine(QPointF(8.0, 10.2), QPointF(8.0, 2.5));
        p.drawPolyline(QPolygonF({QPointF(5.2, 5.2), QPointF(8.0, 2.3), QPointF(10.8, 5.2)}));
    } else {
        p.drawRoundedRect(QRectF(2.5, 2.5, 11.0, 11.0), 2.0, 2.0);
    }
}

} // namespace

QStringList IconFactory::knownKeys()
{
    return {QStringLiteral("overview"),    QStringLiteral("task"),   QStringLiteral("status"),
            QStringLiteral("info"),        QStringLiteral("environment"), QStringLiteral("log"),
            QStringLiteral("lock"),        QStringLiteral("refresh"), QStringLiteral("help"),
            QStringLiteral("export")};
}

QPixmap IconFactory::pixmap(const QString &key, int size, const QColor &color,
                            qreal devicePixelRatio, const QColor &accent)
{
    const qreal dpr = qMax(1.0, devicePixelRatio);
    const QColor effectiveAccent = accent.isValid() ? accent : color;
    const QString cacheKey = QStringLiteral("tw_icon:%1:%2:%3:%4:%5")
                                 .arg(key)
                                 .arg(size)
                                 .arg(qRound(dpr * 100))
                                 .arg(color.rgba(), 8, 16, QLatin1Char('0'))
                                 .arg(effectiveAccent.rgba(), 8, 16, QLatin1Char('0'));

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    const int pixels = qMax(1, qRound(size * dpr));
    QPixmap pixmap(pixels, pixels);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 16.0, size / 16.0);
    drawGlyph(painter, key, color, effectiveAccent);
    painter.end();

    QPixmapCache::insert(cacheKey, pixmap);
    return pixmap;
}

QIcon IconFactory::icon(const QString &key, const QColor &normal, const QColor &selected,
                        const QColor &disabled, const QColor &normalAccent)
{
    QIcon icon;
    for (qreal dpr : {1.0, 2.0}) {
        icon.addPixmap(pixmap(key, 16, normal, dpr, normalAccent), QIcon::Normal, QIcon::Off);
        icon.addPixmap(pixmap(key, 16, selected, dpr, selected), QIcon::Selected, QIcon::Off);
        icon.addPixmap(pixmap(key, 16, selected, dpr, selected), QIcon::Active, QIcon::Off);
        icon.addPixmap(pixmap(key, 16, disabled, dpr, disabled), QIcon::Disabled, QIcon::Off);
    }
    return icon;
}

QIcon IconFactory::appIcon()
{
    const Theme &theme = Theme::workbench();
    QIcon icon;
    for (int sz : {16, 24, 32, 48, 64}) {
        QPixmap px(sz, sz);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(theme.primary);
        const qreal margin = sz * 0.06;
        p.drawRoundedRect(QRectF(margin, margin, sz - 2 * margin, sz - 2 * margin),
                          sz * 0.18, sz * 0.18);
        // 右上角暖橙色点缀条
        p.setBrush(theme.accent);
        p.drawRoundedRect(QRectF(sz * 0.68, sz * 0.14, sz * 0.18, sz * 0.18),
                          sz * 0.05, sz * 0.05);
        QFont f;
        f.setPixelSize(qMax(10, qRound(sz * 0.54)));
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, 0, sz, sz), Qt::AlignCenter, QStringLiteral("滕"));
        p.end();
        icon.addPixmap(px);
    }
    return icon;
}

} // namespace tw
