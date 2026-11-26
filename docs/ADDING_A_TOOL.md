# 如何新增一个工具

新增工具需要三步：写一个 `ITool` 实现、让 CMake 编译它、在注册清单里加一行。不需要改外壳。
下面以一个叫 `notes`（“便签”）的工具为例。内置的 `src/tools/welcome`（多页面）、
`src/tools/task_demo`（长任务、状态栏、导航锁）、`src/tools/sysinfo`（单页面）可作为参考。

## 1. 建目录并写工具类

```
src/tools/notes/
  CMakeLists.txt
  notes_tool.h
  notes_tool.cpp
```

`notes_tool.h`

```cpp
#pragma once

#include "shell/i_tool.h"

namespace tw {

class NotesTool : public ITool
{
public:
    ToolInfo info() const override;
    bool initialize(const ToolContext &context, QString *error) override;
    QVector<ToolPage> createPages() override;

private:
    ToolContext m_context;
};

} // namespace tw
```

`notes_tool.cpp`

```cpp
#include "tools/notes/notes_tool.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QVariant>

namespace tw {

ToolInfo NotesTool::info() const
{
    ToolInfo info;
    info.id = QStringLiteral("notes");       // 小写字母开头；会进入目录名，创建后不要改
    info.title = QStringLiteral("便签");      // 工具栏和顶栏显示的名字
    info.summary = QStringLiteral("记录临时文字");
    info.iconKey = QStringLiteral("log");     // 见下文“图标”
    info.order = 40;                          // 越小越靠前；相同则按注册顺序
    return info;
}

bool NotesTool::initialize(const ToolContext &context, QString *error)
{
    m_context = context;                      // 保存服务句柄
    QString dirError;
    if (m_context.services.dataDir(&dirError).isEmpty()) {
        *error = dirError;                    // 返回 false：只有本工具显示为“不可用”
        return false;
    }
    return true;
}

QVector<ToolPage> NotesTool::createPages()
{
    auto *page = new QWidget;                 // 所有权在 createPages() 返回后移交外壳
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);

    auto *title = new QLabel(QStringLiteral("便签"));
    title->setProperty("role", QStringLiteral("pageTitle"));   // 套用统一样式
    layout->addWidget(title);
    layout->addStretch(1);

    QVector<ToolPage> pages;
    pages.append({PageInfo{QStringLiteral("main"), QStringLiteral("便签"),
                           QStringLiteral("编辑便签"), QStringLiteral("log")},
                  page});
    return pages;
}

} // namespace tw
```

`src/tools/notes/CMakeLists.txt`

```cmake
tw_add_tool(notes SOURCES
    notes_tool.cpp
    notes_tool.h
)
```

## 2. 接入

`src/tools/CMakeLists.txt`：在其他 `add_subdirectory` 旁边加一行

```cmake
add_subdirectory(notes)
```

`src/tools/builtin_tools.cpp`：包含头文件并追加一行注册

```cpp
#include "tools/notes/notes_tool.h"
// ...
    return registry.add(std::make_unique<WelcomeTool>(), error)
        && registry.add(std::make_unique<TaskDemoTool>(), error)
        && registry.add(std::make_unique<SysInfoTool>(), error)
        && registry.add(std::make_unique<NotesTool>(), error);
```

重新运行 `cmake -S . -B build && cmake --build build`，启动后工具栏就会多出“便签”，快捷键
`Ctrl+4` 自动可用。`registry.add` 会拒绝空标题、非法或重复的 id，并通过 `error` 说明原因。

## 3. 生命周期要点

- `initialize()` 里**不要创建 Widgets**，只做廉价的检查并保存 `ToolContext`。
- `createPages()` 在第一次被选中时才调用，且只调用一次；多页面就返回多个 `ToolPage`，
  每个页面 `id` 在本工具内唯一。
- 需要在被切换进来/出去时做事（比如开始/暂停刷新），重写 `activated()` / `deactivated()`。
- 有计时器、线程、锁，就在 `shutdown()` 里收尾。`shutdown()` 之后平台才会销毁。
- 工具失败不要抛到外面：在 `initialize()` 里返回 `false` 并写明原因；外壳也会兜住异常，但
  明确的错误信息对用户更有用。

## 4. 使用外壳服务

`m_context.services` 是 `ToolServices`，可以随意复制进页面类。

```cpp
services.logInfo(QStringLiteral("开始处理"));
services.showStatus(StatusState::Busy, QStringLiteral("处理中 30%"));   // 进行中：保持到被替换
services.showStatus(StatusState::Success, QStringLiteral("已完成"));     // 成功/信息：约 6 秒后回到“就绪”
services.showStatus(StatusState::Error, QStringLiteral("失败：…"));      // 错误：保持到被替换或清除

// 长任务：持有锁期间不能切换工具或页面。锁对象析构即释放，也可以手动 release()。
NavigationLock lock = services.lockNavigation(QStringLiteral("正在导出"));
// ... 任务结束后
lock.release();

QString error;
const QString dir = services.dataDir(&error);   // <数据根>/tools/<工具id>/，已创建
```

规则：

- 把 `NavigationLock` 作为成员保存（如 `TaskDemoPage::m_lock`），**不要**只靠计时器去“恢复导航”。
  任务无论成功、失败还是取消，都要走到 `release()`；页面销毁时它也会自动释放。
- 状态由你显式给出（`StatusState`），不要靠文字猜。
- 每个工具的状态和日志互相独立，不会串到别的工具。
- 数据只写进 `dataDir()` 返回的目录，不要自己拼路径，更不要写系统目录。
- 不要在代码或日志里放密钥、令牌或真实凭据。

## 5. 样式：用 `role` 属性，不要写私有 QSS

外壳的主题已经覆盖了工具页面里常用的控件。给控件设置 `role` 属性即可：

| 控件 | `role` 值 |
| --- | --- |
| `QLabel` | `pageTitle` 页面标题、`pageSubtitle` 副标题、`sectionTitle` 分组标题、`fieldLabel` 字段名、`hint` 提示、`value` 等宽取值、`code` 钢蓝色编码、`pill` 青色状态胶囊 |
| `QFrame` | `card` 白底框线卡片、`toolbar` 顶部浅灰操作栏、`tableHeader` 表头行、`tableRow` / `tableRowAlt` 表格数据行 |
| `QPushButton` | `primary` 主操作、`teal` 青色胶囊按钮、`danger` 危险操作；不设置则为白底框线普通按钮 |

```cpp
label->setProperty("role", QStringLiteral("pageTitle"));
button->setProperty("role", QStringLiteral("primary"));
```

`QLineEdit`、`QCheckBox`、`QProgressBar`、滚动条、对话框等无需设置，自动套用主题。需要新的
通用外观时，改 `src/shell/resources/theme.qss`（颜色改 `src/shell/theme.cpp`），而不是在某个
工具里 `setStyleSheet`，这样所有工具保持一致。

## 6. 图标

`iconKey` 取自 `IconFactory::knownKeys()`：`overview task status info environment log lock refresh help`。
未知的 key 显示通用方块图标。新增图标：在 `src/shell/icon_factory.cpp` 的 `drawGlyph()`
里加一个分支（16×16 坐标系的线性图形），并把 key 加进 `knownKeys()`；
`theme_iconsRenderForAllKnownKeys` 会校验每个 key 都画出了像素。

## 7. 键盘

不需要为导航写任何键盘代码：`Ctrl+N`、`Alt+↑/↓`、`Ctrl+PageUp/PageDown`、`F6`、`Ctrl+L`
由外壳统一处理。页面内部请保证可以用 Tab 走完所有控件（`setTabOrder`），并给控件设置
`setAccessibleName`，这样 `F6` 能把焦点带进你的页面，屏幕阅读器也能读出来。

## 8. 测试

- 页面和服务逻辑：直接在 `tests/ui_tests.cpp` 里照 `builtin_taskDemo*` 写，
  用 `PlatformServices(DataPaths(临时目录))` 构造平台，用 `MainWindow` 驱动。
- 外壳行为：用 `FakeTool` 注入各种失败，不要依赖你的工具。
- 运行：`cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`。
- 看效果：`./build/src/app/TengWangge --tool notes --screenshot /tmp/notes.png`。

## 9. 检查清单

- [ ] `id` 合法且从此不变；`title` 非空；`order` 合适
- [ ] `initialize()` 不创建 Widgets；失败时给出可读原因
- [ ] 长任务持有 `NavigationLock`，成功/失败/取消都释放
- [ ] 状态用 `showStatus` 的显式状态；日志用 `log*`
- [ ] 数据只写 `dataDir()`；没有硬编码路径和凭据
- [ ] 控件用 `role` 属性套用主题；没有私有 QSS
- [ ] 页面可以只用键盘走完；控件有 `accessibleName`
- [ ] 不依赖其他工具，也不依赖 `MainWindow`
