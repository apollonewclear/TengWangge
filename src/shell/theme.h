#pragma once

#include "core/log_buffer.h"
#include "core/status_board.h"

#include <QColor>
#include <QMap>
#include <QString>

namespace tw {

// 素雅浅色工业工作台主题（对齐电力自动化工具设计规范）：
// 大面积中性浅灰外框与工具带（#f5f6f8）+ 纯白表格与日志面板（#ffffff）+ 细灰边框（#cfd4dc），
// 去除抢眼的大面积实心色块；色彩集中在双色矢量图标、表格钢蓝选中行（#2578b5）与语义告警文字上。
struct Theme
{
    // 顶部 Ribbon 工具带与状态栏
    QColor chrome{0xf6, 0xf7, 0xf9};
    QColor chromeRaised{0xf0, 0xf2, 0xf5};
    QColor chromeHover{0xeb, 0xf0, 0xf6};
    QColor chromeSelected{0xe3, 0xed, 0xf8};
    QColor chromeSelectedBorder{0xb5, 0xcf, 0xea};
    QColor chromeBorder{0xd4, 0xd8, 0xe0};
    QColor chromeText{0x1a, 0x20, 0x2c};
    QColor chromeMuted{0x59, 0x63, 0x70};
    QColor chromeDisabled{0x9c, 0xa5, 0xb2};
    QColor accent{0xee, 0x78, 0x1f};
    QColor teal{0x13, 0x84, 0x96};
    QColor emerald{0x1c, 0xa6, 0x64};

    // 工作区、表格与日志框
    QColor surface{0xf2, 0xf3, 0xf5};
    QColor panel{0xff, 0xff, 0xff};
    QColor panelAlt{0xf5, 0xf6, 0xf8};
    QColor selectionSoft{0xe8, 0xf2, 0xfc};
    QColor dangerSoft{0xfe, 0xf2, 0xf2};
    QColor lockBg{0xff, 0xf4, 0xe5};
    QColor lockBorder{0xee, 0xa2, 0x36};
    QColor lockText{0x9a, 0x4f, 0x00};
    QColor border{0xcf, 0xd4, 0xdc};
    QColor borderSoft{0xe2, 0xe5, 0xec};
    QColor text{0x1f, 0x23, 0x29};
    QColor textMuted{0x59, 0x63, 0x70};
    QColor disabledText{0x9c, 0xa5, 0xb2};
    QColor primary{0x25, 0x78, 0xb5};
    QColor primaryHover{0x1d, 0x63, 0x98};
    QColor danger{0xe0, 0x2e, 0x24};

    // 浅灰状态栏上的状态色
    QColor statusIdle{0x4a, 0x55, 0x68};
    QColor statusInfo{0x1d, 0x6f, 0xa5};
    QColor statusBusy{0xc0, 0x56, 0x00};
    QColor statusSuccess{0x1b, 0x7a, 0x3d};
    QColor statusError{0xd9, 0x26, 0x20};

    // 消息日志颜色（对齐参考图：常规深黑，重要/告警/错误醒目红橙绿）
    QColor logInfo{0x1f, 0x23, 0x29};
    QColor logSuccess{0x18, 0x79, 0x4e};
    QColor logWarning{0xc0, 0x56, 0x00};
    QColor logError{0xe0, 0x24, 0x24};

    QString fontFamily = QStringLiteral(
        "\"Noto Sans CJK SC\", \"Alibaba PuHuiTi 3.0\", \"Source Han Sans SC\", "
        "\"WenQuanYi Micro Hei\", \"Microsoft YaHei\", sans-serif");
    QString monoFamily = QStringLiteral(
        "\"DejaVu Sans Mono\", \"Noto Sans Mono CJK SC\", \"Noto Sans CJK SC\", monospace");
    int fontPixelSize = 13;

    static const Theme &workbench();

    QColor statusColor(StatusState state) const;
    QColor logColor(LogLevel level) const;
    QMap<QString, QString> tokens() const;
    QString styleSheet() const;
    static QString expand(const QString &templateText, const QMap<QString, QString> &tokens);
};

} // namespace tw
