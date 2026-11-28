#include "tools/compare/compare_tool.h"

#include "tools/compare/folder_compare_page.h"
#include "tools/compare/ini_compare_page.h"
#include "tools/compare/text_compare_page.h"
#include "tools/compare/three_way_merge_page.h"

namespace tw {

ToolInfo CompareTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("compare");
    info.title = QStringLiteral("文件对比");
    info.summary = QStringLiteral("文件夹、文本、INI 配置对比与三方合并（参考 Beyond Compare）");
    info.iconKey = QStringLiteral("compare");
    info.category = QStringLiteral("文件与对比");
    info.order = 15;
    return info;
}

bool CompareTool::initialize(const ToolContext &context, QString *)
{
    m_context = context;
    return true;
}

QVector<ToolPage> CompareTool::createPages()
{
    auto *folder = new FolderComparePage(m_context.services);
    auto *text = new TextComparePage(m_context.services);
    auto *ini = new IniComparePage(m_context.services);
    auto *merge = new ThreeWayMergePage(m_context.services);

    m_folderPage = folder;
    m_textPage = text;
    m_iniPage = ini;
    m_mergePage = merge;

    // 文件夹双击文件后自动加载并跳转到文本对比页或 INI 对比页
    const ToolServices services = m_context.services;
    QObject::connect(folder, &FolderComparePage::openFileCompareRequested, folder,
                     [text, ini, services](const QString &leftPath, const QString &rightPath, bool isIni) {
                         if (isIni) {
                             ini->openFiles(leftPath, rightPath);
                             services.requestNavigation(QStringLiteral("compare"), QStringLiteral("ini"));
                         } else {
                             text->openFiles(leftPath, rightPath);
                             services.requestNavigation(QStringLiteral("compare"), QStringLiteral("text"));
                         }
                     });

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("folder"), QStringLiteral("文件夹比较"),
                           QStringLiteral("本地目录递归差异扫描与双向同步"), QStringLiteral("folder")},
                  folder});
    pages.append({PageInfo{QStringLiteral("text"), QStringLiteral("文本比较"),
                           QStringLiteral("左右并排文本对比、行内高亮、编辑与块复制"), QStringLiteral("compare")},
                  text});
    pages.append({PageInfo{QStringLiteral("ini"), QStringLiteral("INI 配置比较"),
                           QStringLiteral("按节键核对 sca_model_conf.ini 等多机配置"), QStringLiteral("info")},
                  ini});
    pages.append({PageInfo{QStringLiteral("merge"), QStringLiteral("三方合并"),
                           QStringLiteral("Base / Left / Right 三方合并与冲突解决"), QStringLiteral("merge")},
                  merge});
    return pages;
}

void CompareTool::activated()
{
    m_context.services.showStatus(StatusState::Info, QStringLiteral("文件对比就绪：支持文件夹、文本、INI 对比与三方合并"));
}

void CompareTool::pageActivated(const QString &pageId)
{
    if (pageId == QLatin1String("folder"))
        m_context.services.showStatus(StatusState::Info, QStringLiteral("文件夹比较：支持大小时间 / SHA-256 递归对比与双向同步"));
    else if (pageId == QLatin1String("text"))
        m_context.services.showStatus(StatusState::Info, QStringLiteral("文本比较：支持行内字符差异高亮、实时编辑与差异块复制"));
    else if (pageId == QLatin1String("ini"))
        m_context.services.showStatus(StatusState::Info, QStringLiteral("INI 配置比较：按 [Section] 与键名核对多机配置差异"));
    else if (pageId == QLatin1String("merge"))
        m_context.services.showStatus(StatusState::Info, QStringLiteral("三方合并：支持 Base / Left / Right 冲突定位与决议输出"));
}

void CompareTool::shutdown()
{
    if (m_folderPage)
        m_folderPage->abort();
}

FolderComparePage *CompareTool::folderPage() const { return m_folderPage; }
TextComparePage *CompareTool::textPage() const { return m_textPage; }
IniComparePage *CompareTool::iniPage() const { return m_iniPage; }
ThreeWayMergePage *CompareTool::mergePage() const { return m_mergePage; }

} // namespace tw
