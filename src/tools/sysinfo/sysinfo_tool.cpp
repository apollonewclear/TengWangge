#include "tools/sysinfo/sysinfo_tool.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QSysInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace tw {
namespace {

struct SysParamRow
{
    QString category;
    QString code;
    QString name;
    QString value;
};

QString screenSummary()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QStringLiteral("未检测到主屏幕");
    const QSize size = screen->size();
    return QStringLiteral("%1 × %2（缩放倍率 %3）")
        .arg(size.width())
        .arg(size.height())
        .arg(screen->devicePixelRatio());
}

QString dpiSummary()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QStringLiteral("N/A");
    return QStringLiteral("逻辑 %1 DPI / 物理 %2 DPI")
        .arg(qRound(screen->logicalDotsPerInch()))
        .arg(qRound(screen->physicalDotsPerInch()));
}

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

QVector<SysParamRow> collectParams(const ToolServices &services)
{
    QString dirErr;
    const QString toolDir = services.dataDir(&dirErr);
    return {
        {QStringLiteral("操作系统与硬件"), QStringLiteral("os.product"),
         QStringLiteral("操作系统发行版"), QSysInfo::prettyProductName()},
        {QStringLiteral("操作系统与硬件"), QStringLiteral("os.kernel"),
         QStringLiteral("内核类型与版本"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion()},
        {QStringLiteral("操作系统与硬件"), QStringLiteral("cpu.arch"),
         QStringLiteral("处理器架构"), QSysInfo::currentCpuArchitecture()},
        {QStringLiteral("操作系统与硬件"), QStringLiteral("net.hostname"),
         QStringLiteral("本机主机名"), QSysInfo::machineHostName()},
        {QStringLiteral("Qt 运行时环境"), QStringLiteral("qt.runtime"),
         QStringLiteral("Qt 动态运行版本"), QString::fromLatin1(qVersion())},
        {QStringLiteral("Qt 运行时环境"), QStringLiteral("qt.build"),
         QStringLiteral("Qt 编译期头文件版本"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("Qt 运行时环境"), QStringLiteral("qpa.platform"),
         QStringLiteral("图形平台插件 (QPA)"), QGuiApplication::platformName()},
        {QStringLiteral("显示与隔离存储"), QStringLiteral("display.screen"),
         QStringLiteral("主屏幕分辨率"), screenSummary()},
        {QStringLiteral("显示与隔离存储"), QStringLiteral("display.dpi"),
         QStringLiteral("屏幕像素密度"), dpiSummary()},
        {QStringLiteral("显示与隔离存储"), QStringLiteral("storage.datadir"),
         QStringLiteral("本模块隔离目录"), dirErr.isEmpty() ? toolDir : QStringLiteral("不可用：%1").arg(dirErr)},
    };
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
    const ToolServices services = m_context.services;

    auto *page = new QWidget;
    auto *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(8, 6, 8, 4);
    rootLayout->setSpacing(0);

    QSplitter *splitter = ui::makeSplitter(Qt::Horizontal, page);

    // 左侧：两列分类统计表（1:1 复刻参考图二“类型名 | 数量”左侧导航表格）
    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(ui::makeLabel(QStringLiteral("环境参数分类"), "paneTitle"));

    auto *catTable = new QTableWidget;
    catTable->setAccessibleName(QStringLiteral("参数分类表"));
    ui::configureDataTable(catTable, {QStringLiteral("类型名"), QStringLiteral("数量")});
    catTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    catTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    catTable->setColumnWidth(1, 56);

    const QList<QPair<QString, int>> categories = {
        {QStringLiteral("全部环境参数"), 10},
        {QStringLiteral("操作系统与硬件"), 4},
        {QStringLiteral("Qt 运行时环境"), 3},
        {QStringLiteral("显示与隔离存储"), 3},
    };
    catTable->setRowCount(categories.size());
    for (int i = 0; i < categories.size(); ++i) {
        catTable->setRowHeight(i, 26);
        QTableWidgetItem *nameItem = makeCell(categories.at(i).first);
        nameItem->setData(Qt::UserRole, i == 0 ? QString() : categories.at(i).first);
        catTable->setItem(i, 0, nameItem);
        catTable->setItem(i, 1, makeCell(QString::number(categories.at(i).second), Qt::AlignCenter));
    }
    catTable->selectRow(0);
    leftLayout->addWidget(catTable, 1);
    leftPane->setMinimumWidth(185);
    leftPane->setMaximumWidth(250);

    // 右侧：动作按钮 + 搜索框 + 参数明细表格
    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(5);

    QLabel *rightTitle = ui::makeLabel(QStringLiteral("全部环境参数"), "paneTitle");
    rightLayout->addWidget(rightTitle);

    auto *actionBar = new QHBoxLayout;
    actionBar->setContentsMargins(0, 0, 0, 0);
    actionBar->setSpacing(6);

    QPushButton *refreshBtn = ui::makeActionButton(QStringLiteral("刷新"), QStringLiteral("refresh"));
    refreshBtn->setAccessibleName(QStringLiteral("刷新本机信息"));

    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(QStringLiteral("检索编码、名称或环境值..."));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setFixedWidth(220);
    searchEdit->setAccessibleName(QStringLiteral("检索本机参数"));

    actionBar->addWidget(refreshBtn);
    actionBar->addStretch(1);
    actionBar->addWidget(searchEdit);
    rightLayout->addLayout(actionBar);

    auto *dataTable = new QTableWidget;
    dataTable->setAccessibleName(QStringLiteral("本机环境明细表"));
    ui::configureDataTable(dataTable,
                           {QStringLiteral("序号"),
                            QStringLiteral("参数编码"),
                            QStringLiteral("参数名称"),
                            QStringLiteral("所属分类"),
                            QStringLiteral("有效标志"),
                            QStringLiteral("当前环境值")});
    dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    dataTable->setColumnWidth(0, 52);
    dataTable->setColumnWidth(1, 130);
    dataTable->setColumnWidth(2, 135);
    dataTable->setColumnWidth(3, 125);
    dataTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    dataTable->setColumnWidth(4, 72);
    rightLayout->addWidget(dataTable, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 920});
    rootLayout->addWidget(splitter, 1);

    auto populate = [catTable, dataTable, searchEdit, services, theme]() {
        const QVector<SysParamRow> rows = collectParams(services);
        QString selectedCat;
        const int catRow = catTable->currentRow();
        if (catRow >= 0 && catTable->item(catRow, 0))
            selectedCat = catTable->item(catRow, 0)->data(Qt::UserRole).toString();
        const QString kw = searchEdit->text().trimmed();

        dataTable->setRowCount(0);
        int visibleIdx = 0;
        for (const SysParamRow &r : rows) {
            if (!selectedCat.isEmpty() && r.category != selectedCat)
                continue;
            if (!kw.isEmpty()
                && !r.code.contains(kw, Qt::CaseInsensitive)
                && !r.name.contains(kw, Qt::CaseInsensitive)
                && !r.value.contains(kw, Qt::CaseInsensitive)) {
                continue;
            }
            const int row = visibleIdx++;
            dataTable->insertRow(row);
            dataTable->setRowHeight(row, 26);
            dataTable->setItem(row, 0, makeCell(QString::number(row + 1), Qt::AlignCenter));

            QTableWidgetItem *codeCell = makeCell(r.code);
            codeCell->setForeground(QBrush(theme.primary));
            dataTable->setItem(row, 1, codeCell);
            dataTable->setItem(row, 2, makeCell(r.name));
            dataTable->setItem(row, 3, makeCell(r.category, Qt::AlignCenter));

            QTableWidgetItem *flagCell = makeCell(QStringLiteral("Y"), Qt::AlignCenter);
            flagCell->setForeground(QBrush(theme.statusSuccess));
            dataTable->setItem(row, 4, flagCell);
            dataTable->setItem(row, 5, makeCell(r.value));
        }
        if (dataTable->rowCount() > 0)
            dataTable->selectRow(0);
    };

    populate();

    QObject::connect(catTable, &QTableWidget::currentCellChanged, page,
                     [catTable, rightTitle, populate](int currentRow, int, int, int) {
                         if (currentRow >= 0 && catTable->item(currentRow, 0))
                             rightTitle->setText(catTable->item(currentRow, 0)->text());
                         populate();
                     });
    QObject::connect(searchEdit, &QLineEdit::textChanged, page, [populate](const QString &) {
        populate();
    });
    QObject::connect(refreshBtn, &QPushButton::clicked, page, [populate, services]() {
        populate();
        services.logInfo(QStringLiteral("刷新本机环境信息成功"));
        services.showStatus(StatusState::Success, QStringLiteral("已刷新本机信息"));
    });

    // 单页面工具：外壳自动隐藏二层页签条，直接全幅铺开工业左右分栏工作台
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
