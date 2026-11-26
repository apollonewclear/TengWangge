#pragma once

#include "core/status_board.h"
#include "core/log_buffer.h"

#include <QColor>
#include <QMap>
#include <QString>

namespace tw {

// 浅色工业/电力自动化工作台主题：
// 浅灰外框与工具条（顶栏、左侧导航栏、状态栏）搭配纯白面板与表格，
// 钢蓝色（#2376b7）用于主选中态与主操作，暖橙色（#e67e22）用于双色图标点缀与运行/锁定提示，
// 青色（#138496）用于状态胶囊与切换开关。颜色集中在此处，QSS 通过 @名称@ 引用。
struct Theme
{
    // 外框与导航区（浅色工业风）
    QColor chrome{0xf5, 0xf6, 0xf8};
    QColor chromeRaised{0xf0, 0xf2, 0xf5};
    QColor chromeHover{0xe6, 0xf0, 0xfa};
    QColor chromeSelected{0x23, 0x76, 0xb7};
    QColor chromeBorder{0xd0, 0xd5, 0xde};
    QColor chromeText{0x1f, 0x29, 0x33};
    QColor chromeMuted{0x5a, 0x66, 0x73};
    QColor chromeDisabled{0x98, 0xa2, 0xb0};
    QColor accent{0xe6, 0x7e, 0x22};
    QColor teal{0x13, 0x84, 0x96};

    // 工作区与面板/表格
    QColor surface{0xf2, 0xf3, 0xf5};
    QColor panel{0xff, 0xff, 0xff};
    QColor panelAlt{0xf5, 0xf7, 0xfa};
    QColor selectionSoft{0xe8, 0xf2, 0xfc};
    QColor dangerSoft{0xfe, 0xf2, 0xf2};
    QColor lockBg{0xff, 0xf3, 0xe0};
    QColor lockBorder{0xf0, 0xad, 0x4e};
    QColor lockText{0x9a, 0x4f, 0x00};
    QColor border{0xcf, 0xd5, 0xde};
    QColor borderSoft{0xe1, 0xe5, 0xec};
    QColor text{0x1f, 0x29, 0x33};
    QColor textMuted{0x5a, 0x66, 0x73};
    QColor disabledText{0x98, 0xa2, 0xb0};
    QColor primary{0x23, 0x76, 0xb7};
    QColor primaryHover{0x1b, 0x62, 0x9a};
    QColor danger{0xd9, 0x30, 0x25};

    // 浅色状态栏上的高对比度状态色
    QColor statusIdle{0x52, 0x60, 0x6d};
    QColor statusInfo{0x1d, 0x6f, 0xa5};
    QColor statusBusy{0xb8, 0x5d, 0x00};
    QColor statusSuccess{0x1b, 0x7a, 0x3d};
    QColor statusError{0xc5, 0x22, 0x1f};

    // 日志级别颜色
    QColor logInfo{0x1f, 0x29, 0x33};
    QColor logSuccess{0x1b, 0x7a, 0x3d};
    QColor logWarning{0xb8, 0x5d, 0x00};
    QColor logError{0xd9, 0x30, 0x25};

    QString fontFamily = QStringLiteral(
        "\"Noto Sans CJK SC\", \"Alibaba PuHuiTi 3.0\", \"Source Han Sans SC\", "
        "\"WenQuanYi Micro Hei\", \"Microsoft YaHei\", sans-serif");
    QString monoFamily = QStringLiteral(
        "\"DejaVu Sans Mono\", \"Noto Sans Mono CJK SC\", \"Noto Sans CJK SC\", monospace");
    int fontPixelSize = 13;

    static const Theme &workbench();

    QColor statusColor(StatusState state) const;
    QColor logColor(LogLevel level) const;

    // 名称 -> 值，用于替换 QSS 里的 @名称@。
    QMap<QString, QString> tokens() const;

    // 读取 :/tengwangge/theme.qss 并替换占位符；资源缺失时返回空串。
    QString styleSheet() const;

    // 用 tokens() 替换模板里的 @名称@，返回替换后的文本。
    static QString expand(const QString &templateText, const QMap<QString, QString> &tokens);
};

} // namespace tw
