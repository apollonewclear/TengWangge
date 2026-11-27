#pragma once

#include "core/platform_services.h"
#include "core/tool_info.h"

#include <QString>
#include <QVector>

class QWidget;

namespace tw {

// 工具提供的一个页面。widget 的所有权在 createPages() 返回后移交给外壳。
struct ToolPage
{
    PageInfo info;
    QWidget *widget = nullptr;
};

// 外壳在 initialize() 时交给工具的上下文。services 可以按值保存。
struct ToolContext
{
    ToolInfo info;
    ToolServices services;
    QWidget *dialogParent = nullptr; // 弹窗的父窗口（主窗口）
};

// 工具模块接口。生命周期由外壳驱动：
//
//   info()          注册时读取一次，之后视为不变
//   initialize()    启动时调用一次，不得创建窗口部件；失败则工具显示为“不可用”
//   createPages()   第一次切到该工具时调用一次；之后页面一直保留，不会重建
//   activated()     每次切到该工具之后
//   deactivated()   每次离开该工具之前
//   shutdown()      退出时调用一次，此后页面才会被销毁，工具对象最后销毁
//
// 所有方法都在 GUI 线程调用。耗时工作自行放到后台线程，结果再回到 GUI 线程
// 用 ToolServices 更新状态和日志；执行期间用 lockNavigation() 锁定导航。
class ITool
{
public:
    virtual ~ITool() = default;

    virtual ToolInfo info() const = 0;
    virtual bool initialize(const ToolContext &context, QString *error) = 0;
    virtual QVector<ToolPage> createPages() = 0;

    virtual void activated() {}
    virtual void pageActivated(const QString &pageId) { (void)pageId; }
    virtual void deactivated() {}
    virtual void shutdown() {}
};

} // namespace tw
