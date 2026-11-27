#include "tools/welcome/welcome_tool.h"

#include "core/app_info.h"
#include "shell/page_chrome.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace tw {
namespace {

QWidget *makeOverviewPage(const QString &dataDir, const QString &dataDirError)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *headerBar = new QHBoxLayout;
    headerBar->setSpacing(10);
    headerBar->addWidget(ui::makeLabel(QStringLiteral("平台概览"), "pageTitle"));
    QLabel *pill = ui::makeLabel(QStringLiteral("运行正常"), "pill");
    pill->setWordWrap(false);
    headerBar->addWidget(pill);
    headerBar->addStretch(1);
    layout->addLayout(headerBar);

    layout->addWidget(ui::makeLabel(
        QStringLiteral("滕王阁是面向麒麟桌面的“外壳 + 工具模块”可扩展工具箱平台。左侧选择工具模块，"
                       "顶部页签切换当前工具内的功能页面，底部集中查看消息日志与导航状态。"),
        "pageSubtitle"));

    QFrame *card = ui::makeCard();
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    const QList<int> widths{52, 130, 140, 0};
    cardLayout->addWidget(ui::makeTableRow(
        {QStringLiteral("序号"), QStringLiteral("编码"), QStringLiteral("配置项"), QStringLiteral("当前值")},
        widths, true, false));
    cardLayout->addWidget(ui::makeTableRow(
        {QStringLiteral("1"), QStringLiteral("app.name"), QStringLiteral("程序内部名称"), AppInfo::internalName()},
        widths, false, false));
    cardLayout->addWidget(ui::makeTableRow(
        {QStringLiteral("2"), QStringLiteral("app.version"), QStringLiteral("平台版本"), AppInfo::version()},
        widths, false, true));
    cardLayout->addWidget(ui::makeTableRow(
        {QStringLiteral("3"), QStringLiteral("tool.datadir"), QStringLiteral("本工具隔离目录"),
         dataDirError.isEmpty() ? dataDir : QStringLiteral("不可用：%1").arg(dataDirError)},
        widths, false, false));
    layout->addWidget(card);

    layout->addWidget(ui::makeLabel(
        QStringLiteral("说明：各工具仅读写各自的 0700 隔离数据子目录，日志自动滚动写入 logs/tengwangge.log；"
                       "可通过环境变量 TENGWANGGE_DATA_DIR 指定受控根目录。"),
        "hint"));
    layout->addStretch(1);
    return page;
}

QWidget *makeKeysPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    layout->addWidget(ui::makeLabel(QStringLiteral("键盘操作"), "pageTitle"));
    layout->addWidget(ui::makeLabel(QStringLiteral("所有工具模块共用统一的两层导航与面板快捷键。"), "pageSubtitle"));

    QFrame *card = ui::makeCard();
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    const QList<int> widths{52, 200, 0};
    cardLayout->addWidget(ui::makeTableRow(
        {QStringLiteral("序号"), QStringLiteral("快捷键"), QStringLiteral("功能说明")},
        widths, true, false));

    const QList<QPair<QString, QString>> rows = {
        {QStringLiteral("Ctrl+1 … Ctrl+9"), QStringLiteral("直接切换到第 N 个工具模块")},
        {QStringLiteral("Alt+↑ / Alt+↓"), QStringLiteral("切换到上一个 / 下一个工具模块")},
        {QStringLiteral("Ctrl+PageUp / PageDown"), QStringLiteral("切换到当前工具的上一页 / 下一页")},
        {QStringLiteral("F6"), QStringLiteral("在工具模块列表、顶部页面条、工作区、消息日志之间循环焦点")},
        {QStringLiteral("Ctrl+L"), QStringLiteral("展开 / 收起底部消息日志面板")},
        {QStringLiteral("F1"), QStringLiteral("弹出键盘操作帮助窗口")},
    };
    for (int i = 0; i < rows.size(); ++i) {
        cardLayout->addWidget(ui::makeTableRow(
            {QString::number(i + 1), rows.at(i).first, rows.at(i).second},
            widths, false, i % 2 == 1));
    }
    layout->addWidget(card);

    layout->addWidget(ui::makeLabel(
        QStringLiteral("提示：长时间任务运行期间导航自动锁定并置灰，状态栏显示锁定原因，防止误切换中断任务。"),
        "hint"));
    layout->addStretch(1);
    return page;
}

} // namespace

ToolInfo WelcomeTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("welcome");
    info.title = QStringLiteral("概览");
    info.summary = QStringLiteral("平台介绍与键盘操作");
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
                           QStringLiteral("版本与数据目录"), QStringLiteral("overview")},
                  makeOverviewPage(dir, error)});
    pages.append({PageInfo{QStringLiteral("keys"), QStringLiteral("键盘操作"),
                           QStringLiteral("导航快捷键一览"), QStringLiteral("info")},
                  makeKeysPage()});
    return pages;
}

void WelcomeTool::activated()
{
    m_context.services.showStatus(StatusState::Info, QStringLiteral("欢迎使用%1").arg(AppInfo::productName()));
}

} // namespace tw
