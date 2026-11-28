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

// 在 32x32 高精度逻辑坐标里绘制工业软件双色/多色矢量图标（兼容单色模式与任意尺寸缩放）。
void drawGlyph32(QPainter &p, const QString &key, const QColor &color, const QColor &accent)
{
    const bool dual = accent.isValid() && accent != color;
    const QColor secondary = dual ? accent : color;
    const QColor emerald = dual ? QColor(0x1c, 0xa6, 0x64) : color;
    const QColor danger = dual ? QColor(0xe0, 0x2e, 0x24) : color;

    const QPen mainPen(color, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen accentPen(secondary, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen emeraldPen(emerald, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    p.setPen(mainPen);
    p.setBrush(Qt::NoBrush);

    if (key == QLatin1String("overview")) {
        // 对标“算法定义”：3 个蓝色圆角方格 + 右上角旋转 22° 的暖橙圆角菱形方格
        p.drawRoundedRect(QRectF(4.5, 4.5, 9.5, 9.5), 2.0, 2.0);
        p.drawRoundedRect(QRectF(4.5, 18.0, 9.5, 9.5), 2.0, 2.0);
        p.drawRoundedRect(QRectF(18.0, 18.0, 9.5, 9.5), 2.0, 2.0);
        p.save();
        p.translate(22.75, 9.25);
        p.rotate(22.0);
        p.setPen(accentPen);
        p.drawRoundedRect(QRectF(-4.6, -4.6, 9.2, 9.2), 2.0, 2.0);
        p.restore();
    } else if (key == QLatin1String("task")) {
        // 对标“分量固化”：暖橙色分扇饼图（主扇区 + 右上分离扇区）
        p.setPen(accentPen);
        QPainterPath mainPie;
        mainPie.moveTo(14.8, 17.2);
        mainPie.arcTo(QRectF(4.0, 6.4, 21.6, 21.6), 90.0, 270.0);
        mainPie.closeSubpath();
        p.drawPath(mainPie);

        p.setPen(mainPen);
        QPainterPath slice;
        slice.moveTo(17.4, 14.6);
        slice.arcTo(QRectF(6.8, 4.0, 21.2, 21.2), 5.0, 80.0);
        slice.closeSubpath();
        p.drawPath(slice);
    } else if (key == QLatin1String("info")) {
        // 对标“类型定义”：三排水平调节滑块
        const qreal ys[3] = {8.5, 16.0, 23.5};
        const qreal xs[3] = {12.0, 21.0, 10.5};
        for (int i = 0; i < 3; ++i) {
            p.setPen(mainPen);
            p.drawLine(QPointF(4.5, ys[i]), QPointF(xs[i] - 3.4, ys[i]));
            p.drawLine(QPointF(xs[i] + 3.4, ys[i]), QPointF(27.5, ys[i]));
            p.setPen(i == 1 ? accentPen : mainPen);
            p.drawEllipse(QPointF(xs[i], ys[i]), 3.0, 3.0);
        }
    } else if (key == QLatin1String("environment")) {
        // 对标“模型编辑”：等轴测 3D 立方体 + 右下暖橙色鼠标指针
        const QPointF top(15.0, 4.0);
        const QPointF left(5.2, 9.6);
        const QPointF right(24.8, 9.6);
        const QPointF center(15.0, 15.2);
        const QPointF bLeft(5.2, 21.2);
        const QPointF bottom(15.0, 26.8);
        const QPointF bRight(22.2, 22.6);

        p.setPen(mainPen);
        p.drawPolygon(QPolygonF({top, right, center, left}));
        p.drawLine(left, bLeft);
        p.drawLine(bLeft, bottom);
        p.drawLine(center, bottom);
        p.drawLine(right, QPointF(24.8, 17.5));
        p.drawLine(bottom, QPointF(19.2, 24.4));

        // 右下暖橙色箭头光标
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        QPolygonF cursor;
        cursor << QPointF(19.5, 18.0) << QPointF(28.5, 22.2) << QPointF(24.2, 24.0)
               << QPointF(26.8, 28.2) << QPointF(24.6, 29.3) << QPointF(22.0, 25.1)
               << QPointF(19.5, 27.8);
        p.drawPolygon(cursor);
    } else if (key == QLatin1String("status") || key == QLatin1String("scan")) {
        // 对标“扫描数据”：深色外环 + 翠绿色扫描扇叶
        QPainterPath ring;
        ring.arcMoveTo(QRectF(4.2, 4.2, 23.6, 23.6), 40.0);
        ring.arcTo(QRectF(4.2, 4.2, 23.6, 23.6), 40.0, 295.0);
        p.setPen(QPen(dual ? QColor(0x2d, 0x35, 0x40) : color, 2.3, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(ring);

        p.setPen(emeraldPen);
        QPainterPath wedge;
        wedge.moveTo(11.0, 10.5);
        wedge.lineTo(20.5, 16.0);
        wedge.lineTo(11.0, 21.5);
        wedge.quadTo(14.5, 16.0, 11.0, 10.5);
        p.drawPath(wedge);
    } else if (key == QLatin1String("log")) {
        p.drawRoundedRect(QRectF(4.2, 5.2, 23.6, 21.6), 3.0, 3.0);
        p.drawLine(QPointF(4.2, 11.0), QPointF(27.8, 11.0));
        p.setPen(accentPen);
        p.drawPolyline(QPolygonF({QPointF(8.5, 15.2), QPointF(12.2, 18.2), QPointF(8.5, 21.2)}));
        p.setPen(mainPen);
        p.drawLine(QPointF(15.0, 21.2), QPointF(22.8, 21.2));
    } else if (key == QLatin1String("help")) {
        // 对标右侧暖橙色辅助功能大图标
        p.setPen(accentPen);
        p.drawEllipse(QRectF(4.5, 4.5, 23.0, 23.0));
        QPainterPath qmark;
        qmark.moveTo(12.6, 12.2);
        qmark.arcTo(QRectF(12.0, 8.8, 8.0, 6.4), 165.0, -255.0);
        qmark.lineTo(16.0, 18.5);
        p.setPen(mainPen);
        p.drawPath(qmark);
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        p.drawEllipse(QPointF(16.0, 22.8), 1.5, 1.5);
    } else if (key == QLatin1String("exit")) {
        // 1:1 对标参考图右上角“退出”：左侧蓝色圆角门框 + 右侧橙色出舱箭头
        QPainterPath door;
        door.moveTo(18.5, 5.0);
        door.lineTo(8.5, 5.0);
        door.quadTo(5.5, 5.0, 5.5, 8.0);
        door.lineTo(5.5, 24.0);
        door.quadTo(5.5, 27.0, 8.5, 27.0);
        door.lineTo(18.5, 27.0);
        p.setPen(mainPen);
        p.drawPath(door);

        p.setPen(accentPen);
        p.drawLine(QPointF(12.5, 16.0), QPointF(27.0, 16.0));
        p.drawPolyline(QPolygonF({QPointF(22.0, 11.2), QPointF(27.2, 16.0), QPointF(22.0, 20.8)}));
    } else if (key == QLatin1String("refresh")) {
        // 1:1 对标按钮内“刷新”：实心橙色圆底 + 纯白旋转箭头
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        p.drawEllipse(QRectF(2.5, 2.5, 27.0, 27.0));

        const QPen whitePen(dual ? QColor(Qt::white) : QColor(255, 255, 255, 235),
                            2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(whitePen);
        p.setBrush(Qt::NoBrush);
        QPainterPath arc;
        arc.arcMoveTo(QRectF(8.5, 8.5, 15.0, 15.0), 40.0);
        arc.arcTo(QRectF(8.5, 8.5, 15.0, 15.0), 40.0, 275.0);
        p.drawPath(arc);
        p.drawPolyline(QPolygonF({QPointF(19.0, 7.6), QPointF(23.5, 11.2), QPointF(18.4, 13.0)}));
    } else if (key == QLatin1String("add")) {
        // 1:1 对标按钮内“新增”：实心橙色圆底 + 纯白十字
        p.setPen(Qt::NoPen);
        p.setBrush(secondary);
        p.drawEllipse(QRectF(2.5, 2.5, 27.0, 27.0));
        p.setPen(QPen(Qt::white, 2.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(16.0, 9.5), QPointF(16.0, 22.5));
        p.drawLine(QPointF(9.5, 16.0), QPointF(22.5, 16.0));
    } else if (key == QLatin1String("update")) {
        // 1:1 对标按钮内“更新”：蓝色线框 + 箭头
        p.setPen(mainPen);
        p.drawRoundedRect(QRectF(4.0, 4.0, 24.0, 24.0), 4.0, 4.0);
        p.drawLine(QPointF(16.0, 22.0), QPointF(16.0, 10.0));
        p.drawPolyline(QPolygonF({QPointF(11.2, 14.5), QPointF(16.0, 9.8), QPointF(20.8, 14.5)}));
    } else if (key == QLatin1String("delete")) {
        // 1:1 对标按钮内“删除”：红色线框垃圾桶
        const QPen redPen(danger, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(redPen);
        p.drawLine(QPointF(5.5, 8.5), QPointF(26.5, 8.5));
        p.drawRoundedRect(QRectF(12.0, 4.5, 8.0, 4.0), 1.2, 1.2);
        p.drawRoundedRect(QRectF(7.8, 8.5, 16.4, 18.8), 2.0, 2.0);
        p.drawLine(QPointF(13.2, 13.0), QPointF(13.2, 22.8));
        p.drawLine(QPointF(18.8, 13.0), QPointF(18.8, 22.8));
    } else if (key == QLatin1String("lock")) {
        p.drawRoundedRect(QRectF(6.0, 14.0, 20.0, 14.0), 2.4, 2.4);
        p.setPen(accentPen);
        QPainterPath shackle;
        shackle.moveTo(10.4, 14.0);
        shackle.lineTo(10.4, 10.0);
        shackle.arcTo(QRectF(10.4, 4.2, 11.2, 11.6), 180.0, -180.0);
        shackle.lineTo(21.6, 14.0);
        p.drawPath(shackle);
    } else if (key == QLatin1String("export")) {
        p.drawPolyline(QPolygonF({QPointF(6.0, 20.0), QPointF(6.0, 26.8),
                                  QPointF(26.0, 26.8), QPointF(26.0, 20.0)}));
        p.setPen(accentPen);
        p.drawLine(QPointF(16.0, 20.5), QPointF(16.0, 5.2));
        p.drawPolyline(QPolygonF({QPointF(10.5, 10.5), QPointF(16.0, 4.8), QPointF(21.5, 10.5)}));
    } else if (key == QLatin1String("compare")) {
        // 对标工业双栏对比图标：左右并排文档框 + 中央蓝/橙双向对比箭头
        p.setPen(mainPen);
        p.drawRoundedRect(QRectF(3.5, 5.0, 10.5, 22.0), 2.0, 2.0);
        p.drawRoundedRect(QRectF(18.0, 5.0, 10.5, 22.0), 2.0, 2.0);
        p.drawLine(QPointF(6.2, 10.5), QPointF(11.2, 10.5));
        p.drawLine(QPointF(20.8, 10.5), QPointF(25.8, 10.5));
        p.setPen(accentPen);
        p.drawLine(QPointF(6.2, 16.0), QPointF(11.2, 16.0));
        p.drawLine(QPointF(20.8, 16.0), QPointF(25.8, 16.0));
        p.drawLine(QPointF(11.5, 21.5), QPointF(20.5, 21.5));
        p.drawPolyline(QPolygonF({QPointF(17.8, 19.0), QPointF(20.6, 21.5), QPointF(17.8, 24.0)}));
    } else if (key == QLatin1String("folder")) {
        p.setPen(mainPen);
        QPainterPath tab;
        tab.moveTo(4.0, 9.5);
        tab.lineTo(4.0, 7.5);
        tab.quadTo(4.0, 5.5, 6.0, 5.5);
        tab.lineTo(13.0, 5.5);
        tab.lineTo(15.5, 9.0);
        tab.lineTo(26.0, 9.0);
        tab.quadTo(28.0, 9.0, 28.0, 11.0);
        tab.lineTo(28.0, 24.5);
        tab.quadTo(28.0, 26.5, 26.0, 26.5);
        tab.lineTo(6.0, 26.5);
        tab.quadTo(4.0, 26.5, 4.0, 24.5);
        tab.closeSubpath();
        p.drawPath(tab);
        p.setPen(accentPen);
        p.drawLine(QPointF(8.0, 14.0), QPointF(24.0, 14.0));
    } else if (key == QLatin1String("merge")) {
        // 三方合并 Y 形汇聚箭头
        p.setPen(mainPen);
        p.drawEllipse(QPointF(8.5, 7.5), 3.0, 3.0);
        p.drawEllipse(QPointF(16.0, 24.5), 3.0, 3.0);
        QPainterPath leftBranch;
        leftBranch.moveTo(8.5, 10.5);
        leftBranch.quadTo(8.5, 17.0, 16.0, 17.5);
        leftBranch.lineTo(16.0, 21.5);
        p.drawPath(leftBranch);

        p.setPen(accentPen);
        p.drawEllipse(QPointF(23.5, 7.5), 3.0, 3.0);
        QPainterPath rightBranch;
        rightBranch.moveTo(23.5, 10.5);
        rightBranch.quadTo(23.5, 17.0, 16.0, 17.5);
        p.drawPath(rightBranch);
    } else if (key == QLatin1String("save")) {
        p.setPen(mainPen);
        p.drawRoundedRect(QRectF(5.0, 5.0, 22.0, 22.0), 2.4, 2.4);
        p.drawRect(QRectF(10.0, 5.0, 11.0, 7.5));
        p.setPen(accentPen);
        p.drawRoundedRect(QRectF(9.0, 17.0, 14.0, 10.0), 1.5, 1.5);
    } else if (key == QLatin1String("swap")) {
        p.setPen(mainPen);
        p.drawLine(QPointF(6.0, 11.5), QPointF(25.5, 11.5));
        p.drawPolyline(QPolygonF({QPointF(21.0, 7.5), QPointF(25.8, 11.5), QPointF(21.0, 15.5)}));
        p.setPen(accentPen);
        p.drawLine(QPointF(26.0, 20.5), QPointF(6.5, 20.5));
        p.drawPolyline(QPolygonF({QPointF(11.0, 16.5), QPointF(6.2, 20.5), QPointF(11.0, 24.5)}));
    } else if (key == QLatin1String("copy_right")) {
        p.setPen(accentPen);
        p.drawLine(QPointF(6.0, 16.0), QPointF(25.5, 16.0));
        p.drawPolyline(QPolygonF({QPointF(19.0, 9.5), QPointF(25.8, 16.0), QPointF(19.0, 22.5)}));
    } else if (key == QLatin1String("copy_left")) {
        p.setPen(mainPen);
        p.drawLine(QPointF(26.0, 16.0), QPointF(6.5, 16.0));
        p.drawPolyline(QPolygonF({QPointF(13.0, 9.5), QPointF(6.2, 16.0), QPointF(13.0, 22.5)}));
    } else if (key == QLatin1String("hub")) {
        // 工具中心：3x3 紧凑模块宫格（左上主格暖橙高亮，其余钢蓝）
        const qreal coords[3] = {5.0, 13.5, 22.0};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                const bool highlight = (r == 0 && c == 0);
                p.setPen(Qt::NoPen);
                p.setBrush(highlight ? secondary : color);
                p.drawRoundedRect(QRectF(coords[c], coords[r], 5.6, 5.6), 1.4, 1.4);
            }
        }
    } else if (key == QLatin1String("search")) {
        // 全局快速搜索放大镜
        p.setPen(mainPen);
        p.drawEllipse(QRectF(5.5, 5.5, 15.5, 15.5));
        p.setPen(accentPen);
        p.drawLine(QPointF(19.0, 19.0), QPointF(26.8, 26.8));
    } else {
        p.drawRoundedRect(QRectF(5.0, 5.0, 22.0, 22.0), 4.0, 4.0);
    }
}

} // namespace

QStringList IconFactory::knownKeys()
{
    return {QStringLiteral("overview"),   QStringLiteral("task"),        QStringLiteral("status"),
            QStringLiteral("scan"),       QStringLiteral("info"),        QStringLiteral("environment"),
            QStringLiteral("log"),        QStringLiteral("lock"),        QStringLiteral("refresh"),
            QStringLiteral("add"),        QStringLiteral("update"),      QStringLiteral("delete"),
            QStringLiteral("help"),       QStringLiteral("exit"),        QStringLiteral("export"),
            QStringLiteral("compare"),    QStringLiteral("folder"),      QStringLiteral("merge"),
            QStringLiteral("save"),       QStringLiteral("swap"),        QStringLiteral("copy_right"),
            QStringLiteral("copy_left"),  QStringLiteral("hub"),         QStringLiteral("search")};
}

QPixmap IconFactory::pixmap(const QString &key, int size, const QColor &color,
                            qreal devicePixelRatio, const QColor &accent)
{
    const qreal dpr = qMax(1.0, devicePixelRatio);
    const QColor effectiveAccent = accent.isValid() ? accent : color;
    const QString cacheKey = QStringLiteral("tw_icon32:%1:%2:%3:%4:%5")
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
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size / 32.0, size / 32.0);
    drawGlyph32(painter, key, color, effectiveAccent);
    painter.end();

    QPixmapCache::insert(cacheKey, pixmap);
    return pixmap;
}

QIcon IconFactory::icon(const QString &key, const QColor &normal, const QColor &selected,
                        const QColor &disabled, const QColor &normalAccent)
{
    QIcon icon;
    for (int sz : {16, 32}) {
        for (qreal dpr : {1.0, 2.0}) {
            icon.addPixmap(pixmap(key, sz, normal, dpr, normalAccent), QIcon::Normal, QIcon::Off);
            icon.addPixmap(pixmap(key, sz, selected, dpr, normalAccent.isValid() ? normalAccent : selected),
                           QIcon::Selected, QIcon::Off);
            icon.addPixmap(pixmap(key, sz, selected, dpr, normalAccent.isValid() ? normalAccent : selected),
                           QIcon::Active, QIcon::Off);
            icon.addPixmap(pixmap(key, sz, disabled, dpr, disabled), QIcon::Disabled, QIcon::Off);
        }
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
        p.scale(sz / 32.0, sz / 32.0);
        drawGlyph32(p, QStringLiteral("overview"), theme.primary, theme.accent);
        p.end();
        icon.addPixmap(px);
    }
    return icon;
}

} // namespace tw
