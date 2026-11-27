#include "tools/builtin_tools.h"

#include "tools/compare/compare_tool.h"
#include "tools/sysinfo/sysinfo_tool.h"
#include "tools/task_demo/task_demo_tool.h"
#include "tools/welcome/welcome_tool.h"

#include <memory>

namespace tw {

bool registerBuiltinTools(ToolRegistry &registry, QString *error)
{
    return registry.add(std::make_unique<WelcomeTool>(), error)
        && registry.add(std::make_unique<CompareTool>(), error)
        && registry.add(std::make_unique<TaskDemoTool>(), error)
        && registry.add(std::make_unique<SysInfoTool>(), error);
}

} // namespace tw
