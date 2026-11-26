#include "core/app_info.h"
#include "core/data_paths.h"
#include "core/platform_services.h"
#include "shell/icon_factory.h"
#include "shell/main_window.h"
#include "shell/tool_registry.h"
#include "tools/builtin_tools.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>

#include <cstdio>
#include <memory>

namespace {

bool parseSize(const QString &text, QSize *size)
{
    const QStringList parts = text.split(QLatin1Char('x'));
    if (parts.size() != 2)
        return false;
    bool okW = false;
    bool okH = false;
    const int w = parts.at(0).toInt(&okW);
    const int h = parts.at(1).toInt(&okH);
    if (!okW || !okH || w < 400 || h < 300 || w > 8000 || h > 8000)
        return false;
    *size = QSize(w, h);
    return true;
}

bool isHeadlessPlatform()
{
    const QString platform = QGuiApplication::platformName();
    return platform == QLatin1String("offscreen") || platform == QLatin1String("minimal");
}

} // namespace

int main(int argc, char *argv[])
{
    // Qt 5.15 默认不开启高分屏缩放，需在构造 QApplication 前显式启用以适配麒麟 2K/4K 屏幕。
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    QApplication::setApplicationName(tw::AppInfo::internalName());
    QApplication::setApplicationDisplayName(tw::AppInfo::productName());
    QApplication::setApplicationVersion(tw::AppInfo::version());
    QApplication::setWindowIcon(tw::IconFactory::appIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("滕王阁 — 可扩展的桌面工具箱平台"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption toolOption(QStringLiteral("tool"), QStringLiteral("启动后选中的工具标识"),
                                  QStringLiteral("id"));
    QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("启动后选中的页面标识"),
                                  QStringLiteral("id"));
    QCommandLineOption logOption(QStringLiteral("show-log"), QStringLiteral("启动时展开日志面板"));
    QCommandLineOption sizeOption(QStringLiteral("window-size"), QStringLiteral("窗口大小，例如 1180x760"),
                                  QStringLiteral("WxH"));
    QCommandLineOption shotOption(QStringLiteral("screenshot"),
                                  QStringLiteral("渲染窗口、保存为 PNG 后退出（可配合 QT_QPA_PLATFORM=offscreen）"),
                                  QStringLiteral("file"));
    parser.addOption(toolOption);
    parser.addOption(pageOption);
    parser.addOption(logOption);
    parser.addOption(sizeOption);
    parser.addOption(shotOption);
    parser.process(app);

    const tw::DataPaths paths = tw::DataPaths::standard();
    const QString problem = tw::DataPaths::rootProblem(paths.root());
    if (!problem.isEmpty()) {
        const QString message = QStringLiteral("数据目录不可用：%1\n（%2）\n可通过环境变量 %3 指定。")
                                    .arg(paths.root(), problem,
                                         QString::fromLatin1(tw::DataPaths::overrideVariable()));
        QTextStream(stderr) << message << Qt::endl;
        if (!parser.isSet(shotOption) && !isHeadlessPlatform())
            QMessageBox::critical(nullptr, tw::AppInfo::productName(), message);
        return 2;
    }

    QSize size;
    if (parser.isSet(sizeOption) && !parseSize(parser.value(sizeOption), &size)) {
        QTextStream(stderr) << "--window-size 格式应为 宽x高，例如 1180x760" << Qt::endl;
        return 1;
    }

    tw::ToolRegistry registry;
    QString error;
    if (!tw::registerBuiltinTools(registry, &error)) {
        QTextStream(stderr) << "内置工具注册失败：" << error << Qt::endl;
        return 3;
    }

    const QString requestedTool = parser.isSet(toolOption) ? parser.value(toolOption) : QString();
    if (!requestedTool.isEmpty() && registry.indexOf(requestedTool) < 0) {
        QTextStream(stderr) << "未知工具：" << requestedTool << Qt::endl;
        return 1;
    }

    // 直接将初始工具传入 MainWindow，避免默认第 0 个工具被无条件提前实例化页面与数据目录。
    tw::MainWindow window(std::make_unique<tw::PlatformServices>(paths), std::move(registry),
                          nullptr, requestedTool);

    if (parser.isSet(sizeOption))
        window.resize(size);
    if (parser.isSet(pageOption) && !window.selectPageById(parser.value(pageOption))) {
        QTextStream(stderr) << "未知页面：" << parser.value(pageOption) << Qt::endl;
        return 1;
    }
    window.show();
    if (parser.isSet(logOption))
        window.setLogPanelVisible(true);

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        int exitCode = 0;
        QTimer::singleShot(120, &app, [&] {
            QApplication::processEvents();
            if (!window.grab().save(file, "PNG")) {
                QTextStream(stderr) << "无法保存截图：" << file << Qt::endl;
                exitCode = 1;
            }
            QApplication::quit();
        });
        const int result = app.exec();
        return exitCode ? exitCode : result;
    }
    return app.exec();
}
