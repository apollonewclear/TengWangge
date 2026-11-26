#include "tools/sysinfo/sysinfo_tool.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QVariant>

namespace tw {
namespace {

QLabel *makeLabel(const QString &text, const char *role)
{
    auto *label = new QLabel(text);
    label->setProperty("role", QString::fromLatin1(role));
    label->setWordWrap(true);
    return label;
}

QString screenSummary()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QStringLiteral("未检测到");
    const QSize size = screen->size();
    return QStringLiteral("%1 × %2，缩放 %3")
        .arg(size.width())
        .arg(size.height())
        .arg(screen->devicePixelRatio());
}

struct EnvRow
{
    QString code;
    QString name;
    QLabel *valueLabel = nullptr;
};

QFrame *makeHeaderRow()
{
    auto *row = new QFrame;
    row->setProperty("role", QStringLiteral("tableHeader"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->setSpacing(14);

    auto addCol = [&](const QString &title, int width) {
        QLabel *lbl = makeLabel(title, "fieldLabel");
        if (width > 0) {
            lbl->setWordWrap(false);
            lbl->setFixedWidth(width);
            layout->addWidget(lbl, 0);
        } else {
            layout->addWidget(lbl, 1);
        }
    };
    addCol(QStringLiteral("序号"), 48);
    addCol(QStringLiteral("编码"), 130);
    addCol(QStringLiteral("参数名称"), 130);
    addCol(QStringLiteral("当前环境值"), 0);
    addCol(QStringLiteral("状态"), 60);
    return row;
}

QFrame *makeDataRow(int index, const QString &code, const QString &name, QLabel **outValue)
{
    auto *row = new QFrame;
    row->setProperty("role", QString::fromLatin1((index % 2 == 0) ? "tableRowAlt" : "tableRow"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->setSpacing(14);

    auto *seqLbl = makeLabel(QString::number(index), "value");
    seqLbl->setWordWrap(false);
    seqLbl->setFixedWidth(48);

    auto *codeLbl = makeLabel(code, "code");
    codeLbl->setWordWrap(false);
    codeLbl->setFixedWidth(130);

    auto *nameLbl = makeLabel(name, "fieldLabel");
    nameLbl->setWordWrap(false);
    nameLbl->setFixedWidth(130);

    auto *valLbl = makeLabel(QString(), "value");
    valLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *flagLbl = makeLabel(QStringLiteral("Y"), "code");
    flagLbl->setWordWrap(false);
    flagLbl->setFixedWidth(60);

    layout->addWidget(seqLbl, 0);
    layout->addWidget(codeLbl, 0);
    layout->addWidget(nameLbl, 0);
    layout->addWidget(valLbl, 1);
    layout->addWidget(flagLbl, 0);

    *outValue = valLbl;
    return row;
}

} // namespace

ToolInfo SysInfoTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("sysinfo");
    info.title = QStringLiteral("本机信息");
    info.summary = QStringLiteral("操作系统、Qt 运行时与显示环境");
    info.iconKey = QStringLiteral("environment");
    info.order = 30;
    return info;
}

bool SysInfoTool::initialize(const ToolContext &context, QString *)
{
    m_context = context;
    return true;
}

QVector<ToolPage> SysInfoTool::createPages()
{
    const Theme &theme = Theme::workbench();
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    // 顶部操作工具栏（贴合设计稿：左侧标题/操作按钮，支持一键刷新环境参数）
    auto *actionBar = new QHBoxLayout;
    actionBar->setSpacing(8);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"));
    refreshBtn->setIcon(IconFactory::icon(QStringLiteral("refresh"), theme.accent, theme.accent,
                                          theme.disabledText, theme.accent));
    refreshBtn->setAccessibleName(QStringLiteral("刷新本机信息"));
    actionBar->addWidget(refreshBtn);
    actionBar->addSpacing(8);
    actionBar->addWidget(makeLabel(QStringLiteral("只读运行环境参数，用于麒麟现场环境核对与排障。"), "pageSubtitle"), 1);
    layout->addLayout(actionBar);

    auto *card = new QFrame;
    card->setProperty("role", QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    cardLayout->addWidget(makeHeaderRow());

    QVector<QLabel *> valueLabels(8, nullptr);
    const QList<QPair<QString, QString>> defs = {
        {QStringLiteral("os.product"), QStringLiteral("操作系统")},
        {QStringLiteral("os.kernel"), QStringLiteral("内核版本")},
        {QStringLiteral("cpu.arch"), QStringLiteral("CPU 架构")},
        {QStringLiteral("net.hostname"), QStringLiteral("主机名")},
        {QStringLiteral("qt.runtime"), QStringLiteral("Qt 运行时")},
        {QStringLiteral("qt.build"), QStringLiteral("Qt 编译版本")},
        {QStringLiteral("qpa.platform"), QStringLiteral("显示后端")},
        {QStringLiteral("display.screen"), QStringLiteral("主屏幕")},
    };
    for (int i = 0; i < defs.size(); ++i) {
        cardLayout->addWidget(makeDataRow(i + 1, defs.at(i).first, defs.at(i).second, &valueLabels[i]));
    }
    layout->addWidget(card);
    layout->addStretch(1);

    auto populate = [valueLabels]() {
        const QStringList vals = {
            QSysInfo::prettyProductName(),
            QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion(),
            QSysInfo::currentCpuArchitecture(),
            QSysInfo::machineHostName(),
            QString::fromLatin1(qVersion()),
            QStringLiteral(QT_VERSION_STR),
            QGuiApplication::platformName(),
            screenSummary(),
        };
        for (int i = 0; i < vals.size() && i < valueLabels.size(); ++i) {
            if (valueLabels[i])
                valueLabels[i]->setText(vals.at(i));
        }
    };
    populate();

    const ToolServices services = m_context.services;
    QObject::connect(refreshBtn, &QPushButton::clicked, page, [populate, services]() {
        populate();
        services.logInfo(QStringLiteral("刷新本机环境信息成功"));
        services.showStatus(StatusState::Success, QStringLiteral("已刷新本机信息"));
    });

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("system"), QStringLiteral("系统环境"),
                           QStringLiteral("操作系统与 Qt 版本"), QStringLiteral("environment")},
                  page});
    return pages;
}

void SysInfoTool::activated()
{
    m_context.services.showStatus(StatusState::Info, QStringLiteral("已读取本机信息"));
}

} // namespace tw
