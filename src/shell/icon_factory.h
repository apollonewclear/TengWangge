#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace tw {

// 用 QPainter 现画的双色/单色线性图标，带 QPixmapCache 缓存，不引入外部图片资源。
// ToolInfo/PageInfo 里的 iconKey 在这里解析；未知或为空的 key 退回通用方块图标。
class IconFactory
{
public:
    static QStringList knownKeys();
    static QPixmap pixmap(const QString &key, int size, const QColor &color,
                          qreal devicePixelRatio = 2.0, const QColor &accent = QColor());
    static QIcon icon(const QString &key, const QColor &normal, const QColor &selected,
                      const QColor &disabled, const QColor &normalAccent = QColor());
    static QIcon appIcon();
};

} // namespace tw
