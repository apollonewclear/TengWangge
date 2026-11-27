#include "tools/welcome/welcome_tool.h"

#include "core/app_info.h"
#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace tw {
namespace {

struct OverviewRow
{
    QString category;
    QString code;
    QString cnName;
    QString enName;
    QString flagText;
    bool flagOk = true;
    QString detail;
    QString jumpToolId;
    QString jumpPageId;
};

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

QWidget *makeOverviewPage(const ToolServices &services, const QString &dataDir, const QString &dataDirError)
{
    const Theme &theme = Theme::workbench();
    auto *page = new QWidget;
    auto *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(8, 6, 8, 4);
    rootLayout->setSpacing(0);

    QSplitter *splitter = ui::makeSplitter(Qt::Horizontal, page);

    // 左侧：树形分类导航（对齐参考图左侧树形对象导航面板）
    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(ui::makeLabel(QStringLiteral("模块与规范导航"), "paneTitle"));

    auto *tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    tree->setIndentation(16);
    tree->setUniformRowHeights(true);
    tree->setAccessibleName(QStringLiteral("模块分类树"));

    auto *allItem = new QTreeWidgetItem(tree, QStringList{QStringLiteral("全部平台对象")});
    allItem->setData(0, Qt::UserRole, QString());

    auto *modGroup = new QTreeWidgetItem(tree, QStringList{QStringLiteral("内置工具模块")});
    modGroup->setData(0, Qt::UserRole, QStringLiteral("module"));
    auto *modWelcome = new QTreeWidgetItem(modGroup, QStringList{QStringLiteral("概览 (welcome)")});
    modWelcome->setData(0, Qt::UserRole, QStringLiteral("tool.welcome"));
    auto *modTask = new QTreeWidgetItem(modGroup, QStringList{QStringLiteral("任务演示 (task-demo)")});
    modTask->setData(0, Qt::UserRole, QStringLiteral("tool.task-demo"));
    auto *modSys = new QTreeWidgetItem(modGroup, QStringList{QStringLiteral("本机信息 (sysinfo)")});
    modSys->setData(0, Qt::UserRole, QStringLiteral("tool.sysinfo"));

    auto *coreGroup = new QTreeWidgetItem(tree, QStringList{QStringLiteral("平台核心机制")});
    coreGroup->setData(0, Qt::UserRole, QStringLiteral("mechanism"));
    auto *mechApp = new QTreeWidgetItem(coreGroup, QStringList{QStringLiteral("版本与标识")});
    mechApp->setData(0, Qt::UserRole, QStringLiteral("app."));
    auto *mechDir = new QTreeWidgetItem(coreGroup, QStringList{QStringLiteral("隔离数据目录")});
    mechDir->setData(0, Qt::UserRole, QStringLiteral("path.datadir"));
    auto *mechLog = new QTreeWidgetItem(coreGroup, QStringList{QStringLiteral("滚动日志与导航锁")});
    mechLog->setData(0, Qt::UserRole, QStringLiteral("core."));

    tree->expandAll();
    tree->setCurrentItem(allItem);
    leftLayout->addWidget(tree, 1);
    leftPane->setMinimumWidth(185);
    leftPane->setMaximumWidth(260);

    // 右侧：动作按钮栏 + 检索框 + 高密度网格表格
    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(5);

    QLabel *rightTitle = ui::makeLabel(QStringLiteral("平台配置与模块清单"), "paneTitle");
    rightLayout->addWidget(rightTitle);

    auto *actionBar = new QHBoxLayout;
    actionBar->setContentsMargins(0, 0, 0, 0);
    actionBar->setSpacing(6);

    QPushButton *refreshBtn = ui::makeActionButton(QStringLiteral("刷新"), QStringLiteral("refresh"));
    QPushButton *taskBtn = ui::makeActionButton(QStringLiteral("任务演示"), QStringLiteral("update"));
    QPushButton *sysBtn = ui::makeActionButton(QStringLiteral("本机信息"), QStringLiteral("add"));
    QPushButton *keysBtn = ui::makeActionButton(QStringLiteral("键盘操作"), QStringLiteral("help"));

    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(QStringLiteral("检索编码、名称或说明..."));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setFixedWidth(210);
    searchEdit->setAccessibleName(QStringLiteral("检索平台配置"));

    actionBar->addWidget(refreshBtn);
    actionBar->addWidget(taskBtn);
    actionBar->addWidget(sysBtn);
    actionBar->addWidget(keysBtn);
    actionBar->addStretch(1);
    actionBar->addWidget(searchEdit);
    rightLayout->addLayout(actionBar);

    auto *table = new QTableWidget;
    table->setAccessibleName(QStringLiteral("平台对象清单表"));
    ui::configureDataTable(table,
                           {QStringLiteral("序号"),
                            QStringLiteral("对象编码"),
                            QStringLiteral("中文名称"),
                            QStringLiteral("英文标识"),
                            QStringLiteral("有效标志"),
                            QStringLiteral("当前配置 / 机制说明")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 52);
    table->setColumnWidth(1, 126);
    table->setColumnWidth(2, 120);
    table->setColumnWidth(3, 125);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table->setColumnWidth(4, 72);
    rightLayout->addWidget(table, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 920});
    rootLayout->addWidget(splitter, 1);

    const bool dirOk = dataDirError.isEmpty();
    const QVector<OverviewRow> allRows = {
        {QStringLiteral("mechanism"), QStringLiteral("app.name"), QStringLiteral("程序内部名称"),
         AppInfo::internalName(), QStringLiteral("是"), true,
         QStringLiteral("麒麟桌面可扩展工具箱外壳平台（Qt 5.15 / C++17）"), QString(), QString()},
        {QStringLiteral("mechanism"), QStringLiteral("app.version"), QStringLiteral("平台发布版本"),
         AppInfo::version(), QStringLiteral("是"), true,
         QStringLiteral("第一阶段底座框架，支持静态模块注册与惰性页面实例化"), QString(), QString()},
        {QStringLiteral("module"), QStringLiteral("tool.welcome"), QStringLiteral("概览模块"),
         QStringLiteral("welcome"), QStringLiteral("就绪"), true,
         QStringLiteral("多页签参考模块：平台配置总览 + 全局键盘操作速查"),
         QStringLiteral("welcome"), QStringLiteral("overview")},
        {QStringLiteral("module"), QStringLiteral("tool.task-demo"), QStringLiteral("任务演示模块"),
         QStringLiteral("task-demo"), QStringLiteral("就绪"), true,
         QStringLiteral("演示 TaskRunner 独立工作线程、步骤进度与 RAII 导航锁定"),
         QStringLiteral("task-demo"), QStringLiteral("run")},
        {QStringLiteral("module"), QStringLiteral("tool.sysinfo"), QStringLiteral("本机信息模块"),
         QStringLiteral("sysinfo"), QStringLiteral("就绪"), true,
         QStringLiteral("单页面参考模块：操作系统、内核、Qt 运行时与屏幕参数核对"),
         QStringLiteral("sysinfo"), QStringLiteral("system")},
        {QStringLiteral("mechanism"), QStringLiteral("path.datadir"), QStringLiteral("工具隔离目录"),
         QStringLiteral("tools/welcome"), dirOk ? QStringLiteral("0700") : QStringLiteral("异常"), dirOk,
         dirOk ? dataDir : QStringLiteral("不可用：%1").arg(dataDirError), QString(), QString()},
        {QStringLiteral("mechanism"), QStringLiteral("core.log"), QStringLiteral("滚动日志落盘"),
         QStringLiteral("LogFileSink"), QStringLiteral("是"), true,
         QStringLiteral("底部常驻消息框实时显示，同时按 0600 权限滚动写入 logs/tengwangge.log"),
         QString(), QString()},
        {QStringLiteral("mechanism"), QStringLiteral("core.navgate"), QStringLiteral("引用计数导航锁"),
         QStringLiteral("NavigationGate"), QStringLiteral("是"), true,
         QStringLiteral("长任务运行期间自动置灰锁定顶部导航，任务结束或异常退出自动释放"),
         QString(), QString()},
    };

    auto rebuildTable = [table, allRows, tree, searchEdit, theme]() {
        QString filterTag;
        if (const QTreeWidgetItem *cur = tree->currentItem())
            filterTag = cur->data(0, Qt::UserRole).toString();
        const QString keyword = searchEdit->text().trimmed();

        table->setRowCount(0);
        int visibleIdx = 0;
        for (const OverviewRow &r : allRows) {
            if (!filterTag.isEmpty()) {
                const bool matchCat = (r.category == filterTag);
                const bool matchPrefix = r.code.startsWith(filterTag);
                if (!matchCat && !matchPrefix)
                    continue;
            }
            if (!keyword.isEmpty()) {
                const bool hit = r.code.contains(keyword, Qt::CaseInsensitive)
                              || r.cnName.contains(keyword, Qt::CaseInsensitive)
                              || r.enName.contains(keyword, Qt::CaseInsensitive)
                              || r.detail.contains(keyword, Qt::CaseInsensitive);
                if (!hit)
                    continue;
            }
            const int row = visibleIdx++;
            table->insertRow(row);
            table->setRowHeight(row, 26);

            table->setItem(row, 0, makeCell(QString::number(row + 1), Qt::AlignCenter));
            QTableWidgetItem *codeItem = makeCell(r.code);
            codeItem->setForeground(QBrush(theme.primary));
            codeItem->setData(Qt::UserRole, r.jumpToolId);
            codeItem->setData(Qt::UserRole + 1, r.jumpPageId);
            table->setItem(row, 1, codeItem);
            table->setItem(row, 2, makeCell(r.cnName));
            table->setItem(row, 3, makeCell(r.enName));

            QTableWidgetItem *flagItem = makeCell(r.flagText, Qt::AlignCenter);
            flagItem->setForeground(QBrush(r.flagOk ? theme.statusSuccess : theme.danger));
            table->setItem(row, 4, flagItem);
            table->setItem(row, 5, makeCell(r.detail));
        }
        if (table->rowCount() > 0)
            table->selectRow(0);
    };

    rebuildTable();

    QObject::connect(tree, &QTreeWidget::currentItemChanged, page,
                     [rebuildTable, rightTitle](QTreeWidgetItem *current, QTreeWidgetItem *) {
                         if (current)
                             rightTitle->setText(QStringLiteral("平台配置与模块清单 — %1").arg(current->text(0)));
                         rebuildTable();
                     });
    QObject::connect(searchEdit, &QLineEdit::textChanged, page, [rebuildTable](const QString &) {
        rebuildTable();
    });
    QObject::connect(refreshBtn, &QPushButton::clicked, page, [rebuildTable, services]() {
        rebuildTable();
        services.logInfo(QStringLiteral("刷新平台配置与模块清单完成"));
        services.showStatus(StatusState::Success, QStringLiteral("已刷新平台概览清单"));
    });
    QObject::connect(taskBtn, &QPushButton::clicked, page, [services]() {
        services.requestNavigation(QStringLiteral("task-demo"), QStringLiteral("run"));
    });
    QObject::connect(sysBtn, &QPushButton::clicked, page, [services]() {
        services.requestNavigation(QStringLiteral("sysinfo"), QStringLiteral("system"));
    });
    QObject::connect(keysBtn, &QPushButton::clicked, page, [services]() {
        services.requestNavigation(QStringLiteral("welcome"), QStringLiteral("keys"));
    });
    QObject::connect(table, &QTableWidget::cellDoubleClicked, page, [table, services](int row, int) {
        const QTableWidgetItem *codeItem = table->item(row, 1);
        if (!codeItem)
            return;
        const QString targetTool = codeItem->data(Qt::UserRole).toString();
        const QString targetPage = codeItem->data(Qt::UserRole + 1).toString();
        if (!targetTool.isEmpty())
            services.requestNavigation(targetTool, targetPage);
    });

    return page;
}

QWidget *makeKeysPage()
{
    const Theme &theme = Theme::workbench();
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 6, 8, 4);
    layout->setSpacing(5);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);
    topRow->addWidget(ui::makeLabel(QStringLiteral("全局导航与工作台键盘快捷键"), "paneTitle"));
    topRow->addStretch(1);
    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(QStringLiteral("检索按键或功能说明..."));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setFixedWidth(210);
    searchEdit->setAccessibleName(QStringLiteral("检索快捷键"));
    topRow->addWidget(searchEdit);
    layout->addLayout(topRow);

    auto *table = new QTableWidget;
    table->setAccessibleName(QStringLiteral("快捷键清单表"));
    ui::configureDataTable(table,
                           {QStringLiteral("序号"),
                            QStringLiteral("编码"),
                            QStringLiteral("快捷键组合"),
                            QStringLiteral("作用范围"),
                            QStringLiteral("锁定态策略"),
                            QStringLiteral("功能说明")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 52);
    table->setColumnWidth(1, 120);
    table->setColumnWidth(2, 175);
    table->setColumnWidth(3, 115);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table->setColumnWidth(4, 96);
    layout->addWidget(table, 1);

    struct KeyRow
    {
        QString code;
        QString key;
        QString scope;
        QString lockPolicy;
        QString desc;
    };
    const QVector<KeyRow> rows = {
        {QStringLiteral("key.tool.num"), QStringLiteral("Ctrl+1 … Ctrl+9"), QStringLiteral("顶部模块栏"),
         QStringLiteral("锁定期间拦截"), QStringLiteral("直接切换到第 N 个工具模块")},
        {QStringLiteral("key.tool.step"), QStringLiteral("Alt+↑ / Alt+↓"), QStringLiteral("顶部模块栏"),
         QStringLiteral("锁定期间拦截"), QStringLiteral("切换到上一个 / 下一个工具模块（到头不循环）")},
        {QStringLiteral("key.page.step"), QStringLiteral("Ctrl+PageUp / PageDown"), QStringLiteral("模块页签条"),
         QStringLiteral("锁定期间拦截"), QStringLiteral("切换到当前工具的上一页 / 下一页")},
        {QStringLiteral("key.focus.ring"), QStringLiteral("F6"), QStringLiteral("全局焦点环"),
         QStringLiteral("始终可用"), QStringLiteral("在顶部模块栏、页签条、工作区、消息日志之间循环焦点")},
        {QStringLiteral("key.log.toggle"), QStringLiteral("Ctrl+L"), QStringLiteral("底部消息框"),
         QStringLiteral("始终可用"), QStringLiteral("展开或收起底部常驻消息日志框（右键日志可开启筛选栏）")},
        {QStringLiteral("key.help.dialog"), QStringLiteral("F1"), QStringLiteral("全局窗口"),
         QStringLiteral("始终可用"), QStringLiteral("弹出键盘与操作帮助说明窗口")},
    };

    auto populate = [table, rows, searchEdit, theme]() {
        const QString kw = searchEdit->text().trimmed();
        table->setRowCount(0);
        int idx = 0;
        for (const KeyRow &r : rows) {
            if (!kw.isEmpty() && !r.code.contains(kw, Qt::CaseInsensitive)
                && !r.key.contains(kw, Qt::CaseInsensitive)
                && !r.desc.contains(kw, Qt::CaseInsensitive)) {
                continue;
            }
            const int row = idx++;
            table->insertRow(row);
            table->setRowHeight(row, 26);
            table->setItem(row, 0, makeCell(QString::number(row + 1), Qt::AlignCenter));
            QTableWidgetItem *codeCell = makeCell(r.code);
            codeCell->setForeground(QBrush(theme.primary));
            table->setItem(row, 1, codeCell);
            table->setItem(row, 2, makeCell(r.key));
            table->setItem(row, 3, makeCell(r.scope, Qt::AlignCenter));
            table->setItem(row, 4, makeCell(r.lockPolicy, Qt::AlignCenter));
            table->setItem(row, 5, makeCell(r.desc));
        }
        if (table->rowCount() > 0)
            table->selectRow(0);
    };
    populate();

    QObject::connect(searchEdit, &QLineEdit::textChanged, page, [populate](const QString &) {
        populate();
    });

    return page;
}

} // namespace

ToolInfo WelcomeTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("welcome");
    info.title = QStringLiteral("概览");
    info.summary = QStringLiteral("平台配置总览与键盘操作规范");
    info.iconKey = QStringLiteral("overview");
    info.order = 10;
    return info;
}

bool WelcomeTool::initialize(const ToolContext &context, QString *)
{
    m_context = context;
    return true;
}

QVector<ToolPage> WelcomeTool::createPages()
{
    QString error;
    const QString dir = m_context.services.dataDir(&error);

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("overview"), QStringLiteral("平台概览"),
                           QStringLiteral("模块与平台配置清单"), QStringLiteral("overview")},
                  makeOverviewPage(m_context.services, dir, error)});
    pages.append({PageInfo{QStringLiteral("keys"), QStringLiteral("键盘操作"),
                           QStringLiteral("导航快捷键一览"), QStringLiteral("info")},
                  makeKeysPage()});
    return pages;
}

void WelcomeTool::activated()
{
    m_context.services.showStatus(StatusState::Info, QStringLiteral("欢迎使用%1").arg(AppInfo::productName()));
}

void WelcomeTool::pageActivated(const QString &pageId)
{
    if (pageId == QLatin1String("keys"))
        m_context.services.showStatus(StatusState::Info, QStringLiteral("当前查看：导航与面板键盘快捷键"));
    else
        m_context.services.showStatus(StatusState::Info, QStringLiteral("欢迎使用%1").arg(AppInfo::productName()));
}

} // namespace tw
