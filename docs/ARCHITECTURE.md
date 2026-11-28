# 架构说明

滕王阁（TengWangge）是“外壳 + 多个工具模块”的桌面工具箱平台。外壳负责导航、状态、日志、
主题和生命周期；工具只负责自己的页面和业务。第一阶段所有工具在编译期接入，不做插件 ABI。

## 1. 分层

```
┌────────────────────────────────────────────────────────────────┐
│ src/app            可执行文件 TengWangge：命令行、组装、启动      │
├────────────────────────────────────────────────────────────────┤
│ src/tools          内置工具（每个工具一个静态库 tw_tool_<名字>） │
│   welcome  compare  task_demo  sysinfo + builtin_tools.cpp      │
├────────────────────────────────────────────────────────────────┤
│ src/shell          Qt Widgets 外壳（静态库 tengwangge_shell）    │
│   ITool  ToolRegistry  ToolHost  MainWindow  ToolRail  ToolView │
│   StatusStrip  LogPanel  Theme + theme.qss  IconFactory         │
├────────────────────────────────────────────────────────────────┤
│ src/core           只依赖 Qt Core（静态库 tengwangge_core）      │
│   AppInfo  ToolInfo/PageInfo  DataPaths  LogBuffer  LogFileSink  │
│   ShellConfig  StatusBoard  NavigationGate  TaskRunner           │
│   PlatformServices / ToolServices                                │
└────────────────────────────────────────────────────────────────┘
```

依赖只能向下。三条硬规则：

1. **`core` 不依赖 Widgets/Gui。** 由 CMake 链接关系保证（`tengwangge_core` 只链接 `Qt5::Core`），
   并由 ctest `core_layering` 扫描 `src/core` 的 `#include` 和链接声明兜底。
2. **`shell` 不认识任何具体工具。** 它只通过 `ITool` 和一个已经填好的 `ToolRegistry` 工作；
   测试里注入假工具即可验证全部外壳行为。
3. **工具之间互不依赖，也不依赖 `MainWindow`。** 工具只看到 `ITool`、`ToolContext` 和
   `ToolServices`。

为什么 core 单独成库：日志、状态、导航锁、数据目录是平台语义，不是界面语义。把它们放在
无 Widgets 的库里，可以用 `QCoreApplication` 直接测，将来也能被命令行工具或后台服务复用。

## 2. 工具接口（`src/shell/i_tool.h`）

```cpp
class ITool {
public:
    virtual ToolInfo info() const = 0;                          // id/标题/摘要/图标键/排序
    virtual bool initialize(const ToolContext&, QString *err) = 0;
    virtual QVector<ToolPage> createPages() = 0;
    virtual void activated() {}
    virtual void deactivated() {}
    virtual void shutdown() {}
};
```

| 阶段 | 何时调用 | 约定 |
| --- | --- | --- |
| `info()` | 注册时和之后任意时刻 | 纯数据，不能有副作用；`id` 必须是合法标识（小写字母开头，字母数字，可用 `.`/`-` 分段） |
| `initialize()` | 窗口构造时，对所有工具**急切**调用一次 | 不创建 Widgets；保存 `ToolContext`；返回 `false` 或抛异常 = 该工具“不可用” |
| `createPages()` | 工具**第一次被选中**时调用一次 | 返回的页面所有权移交外壳；页面清单非法（空、重复 id、空指针）= 该工具“不可用” |
| `activated()` | 每次被切换为当前工具 | 重复选择同一工具不会重复通知 |
| `deactivated()` | 切换到别的工具时 | 与 `activated()` 成对 |
| `shutdown()` | 窗口销毁前，每个就绪工具一次 | 停止计时器、释放锁；`ToolHost::shutdown()` 幂等 |

要点：

- **失败隔离。** 一个工具初始化或建页失败，只让它在工具栏变灰并标“不可用”，选中它能看到原因；
  其他工具不受影响。失败时平台会兜底释放它持有的导航锁并写一条平台错误日志。
- **页面只建一次。** 切换工具只改变外层栈的当前项，各工具的当前页和控件状态天然保留。
- **标识会进入目录名和对象名**，所以限制字符集，不允许路径分隔符和大小写差异。

## 3. 外壳服务（`src/core/platform_services.h`）

`PlatformServices` 拥有三个共享对象，`ToolServices` 是发给每个工具的小句柄（可复制，自动带上
工具 id）：

| 能力 | API | 说明 |
| --- | --- | --- |
| 日志 | `logInfo/Success/Warning/Error(msg)` | 写入同一个 `LogBuffer` 环形缓冲，每条带来源工具 id；日志面板默认只显示当前工具 + 平台条目 |
| 状态栏 | `showStatus(state, text, timeoutMs)` / `clearStatus()` | 每个工具各一条；状态是显式枚举（`Idle/Info/Busy/Success/Error`），不靠文字猜；信息/成功默认 6 秒后回到“就绪”，进行中/错误保持到被替换或清除 |
| 导航锁 | `lockNavigation(reason)` → `NavigationLock` | 长任务期间持有；ref-counted，多把锁可并存，全部释放才解锁；RAII，析构即释放 |
| 数据目录 | `dataDir(&err)` | `<根>/tools/<工具id>/`，每个工具只拿到自己的目录 |

**为什么导航锁不是 `setNavigationEnabled(bool)`：** 两个任务互相覆盖、或某个任务忘了恢复，
布尔开关就会错乱。带原因的引用计数锁没有这个问题，状态栏还能说明“为什么不能切换”。
工具被关闭或初始化失败时，平台调用 `releaseAllFor(toolId)` 兜底。

`ToolServices` 内部用 `QPointer` 指向平台：平台销毁后所有调用退化为空操作，不会崩溃。

## 4. 两层导航与主窗口

```
┌ 顶部大图标模块栏（#topBar，68px，面向 20~50+ 工具扩展）                 ┐
│ [工具中心]│ [概览][文件对比][…][»]   [速搜 Ctrl+K] [消息日志][帮助][退出] │
├───────────────────────────────────────────────────────────────────────┤
│  [页面1] [页面2]     ← 第二层：ToolView 页面条（单页面工具自动隐藏）    │
├───────────────────────────────────────────────────────────────────────┤
│  工具工作区（100% 全宽）或「工具中心」分类卡片网格页（Ctrl+0 切换）      │
├────────────────────────────────────── ..... （五圆点横向分割抓手）─────┤
│  [INFO][2026/11/28 19:58:35] 平台启动完成 …   ← 常驻极简消息框          │
├───────────────────────────────────────────────────────────────────────┤
│ ● 就绪  用户:nusp 启动时间:… 有效时长:00:05:12   [导航锁]   [消息日志] ┘
```

- **第一层：可扩展顶部大图标模块栏（`工具中心` + 可横向滚轮滚动 `ToolRail` + `»` 分类溢出菜单 + `Ctrl+K` 速搜）**。当工具数达到 20~50+ 时，顶栏不会撑宽窗口，超出视口的模块可通过鼠标滚轮横向浏览、`»` 分类子菜单、`工具中心`（`Ctrl+0`）分类卡片网格或 `Ctrl+K` 全局速搜秒级直达，同时保持中央工作区始终 100% 全宽。
- **第二层：`ToolView`。** 页面条（`QTabBar`）+ 该工具的页面栈。单页面工具（如“本机信息”）自动隐藏页面条，避免顶栏叠床架屋；多页面工具保留各自独立的当前页记忆。
- **`ToolHost`** 管生命周期和视图创建，不认识 `MainWindow`，可以单独用假工具测试。
- **`MainWindow`** 把 `ToolHost`、导航锁、状态板、日志缓冲粘在一起：切换工具时刷新窗口标题（`滕王阁@<主机名> — <工具> · <页面>`）、日志过滤、状态栏；导航锁变化时把顶部模块栏和所有页面条置灰并在状态栏显示原因。

### 销毁顺序

`MainWindow` 拥有 `PlatformServices` 和 `ToolRegistry`，保证：
工具 `shutdown()` → 所有页面销毁 → 工具对象销毁 → 平台服务销毁。页面或工具里还持有的
`NavigationLock` 在析构时释放，此时平台仍然存活。测试 `lock_destroyingWindowWithLockedToolIsSafe`
覆盖这一点。

## 5. 主题（`Theme` + `resources/theme.qss`）

- 视觉：高密度浅色工业工程工作台风格——浅灰（`#f6f7f9`）顶部大图标模块栏与底部状态栏，搭配纯白左树右表工作区、五圆点（`⋮` / `.....`）分割抓手与带清晰浅灰网格线的 `QTableWidget`；克制的浅冰蓝圆角框（`#e3edf8` 底 + `#b5cfea` 描边）用于顶部模块选中态，钢蓝色（`#2578b5`）用于表格选中行与编码字段，暖橙色（`#ee7816`）用于双色线性图标点缀与锁定提示。
- 所有颜色和字体集中在 `Theme` 结构体。QSS 里用两个 `@` 夹住名字的占位符，加载时展开，
  所以 QSS 和 C++ 绘制代码读的是同一份数值。
- 顶部模块栏单元由自定义 delegate 绘制（`30×30` 精细双色/多色矢量图标、居中标题、柔和浅冰蓝圆角选中框、焦点虚线框、“不可用”红标），因为 QSS 无法表达这些组合状态。
- 图标由 `IconFactory` 在 `32×32` 坐标系下用 `QPainter` 现画（覆盖模块大图标与 `16×16` 按钮彩色小图标），没有任何外部图片资源，也不引入第三方商业素材。
- 工具页面不需要私有样式：通过 `src/shell/page_chrome.h`（`ui::makeSplitter`、`ui::makeActionButton`、`ui::configureDataTable`）和 `role` 属性即可构建统一外观（见 `docs/ADDING_A_TOOL.md`）。
- 资源放在静态库里，链接器可能丢掉未引用的 qrc 目标文件，所以 `Theme::styleSheet()` 显式
  `Q_INIT_RESOURCE`。测试 `theme_stylesheetIsFullyExpanded` 守住这一点。

## 6. 键盘与选中态

| 键 | 作用 |
| --- | --- |
| `Ctrl+1 … Ctrl+9` | 切换到第 N 个工具（悬停提示显示快捷键） |
| `Alt+↑ / Alt+↓`（及模块栏内 `←/→/↑/↓`） | 上一个 / 下一个工具，到头不循环 |
| `Ctrl+PageUp / PageDown` | 当前工具的上一页 / 下一页 |
| `F6` | 在 顶部模块栏 → 页面条（可见时） → 页面 → 消息日志 → 日志开关 之间循环焦点 |
| `Ctrl+L` | 展开 / 收起底部常驻消息日志面板（日志框内右键可开启检索/级别筛选条或导出） |
| `F1` | 键盘帮助 |

选中态和焦点态是两个独立的视觉信号，可以同时出现：

- 当前工具：顶部模块栏浅冰蓝圆角底框 + 细蓝边 + 加粗深色标题；焦点在模块栏时叠加虚线焦点框；不可用工具置灰并右上角标“不可用”，不伪装成激活态。
- 当前页面：紧凑二层页签条白底上浮 + 钢蓝色顶边与粗体文字；页面条获得焦点时底边呈暖橙色高亮。
- 锁定时：整个导航变灰，但当前工具/页面的位置仍然可辨认。

这些全部由外壳实现，工具加得再多也不需要每个工具重复处理。

> 说明：页面条焦点用 `tabFocus` 属性而不是 `:focus::tab:selected`，因为后者在 Qt 5.15 的
> QSS 里会错误命中未选中的页签。

## 7. 数据与安全边界

- 数据根目录：`$TENGWANGGE_DATA_DIR`，否则 `~/.local/share/TengWangge`（Qt 的
  `GenericDataLocation` + 产品目录名）。目录名只在 `AppInfo::dataDirName()` 定义一次。
- 根目录拒绝为空、`/` 或 `/bin /boot /dev /etc /lib /lib32 /lib64 /proc /sbin /sys /usr`
  及其子目录；启动时检查，失败则报错退出，不会回退到别处。
- 新建的根目录权限为 `0700`。
- 平台不读取、不写入任何其他产品（包括 platformToolNew）的路径；源码里没有密钥、令牌或
  部署凭据，也没有任何绝对路径依赖。

## 8. 为什么暂时不做插件 ABI

插件 ABI 一旦发布就要长期兼容。当前 `ITool` 虽然很小，但生命周期细节（失败语义、锁的兜底、
页面所有权）还要被第一个真实业务工具验证。编译期接入的工具已经与外壳完全解耦（独立静态库，
只依赖 `ITool`/`ToolServices`），将来改成动态加载只需要：给 `ITool` 加一个 C 风格的入口、
明确 ABI 版本号，并保证 `ToolServices` 跨库边界可用。工具代码本身不用改。

## 9. 测试策略

| 测试 | 内容 |
| --- | --- |
| `core_tests` | 只链接 Qt Core + Qt Test，用 `QCoreApplication`：标识校验、数据目录隔离与系统目录拒绝、日志环形缓冲、状态超时与代际、导航锁引用计数、`ToolServices` 在平台销毁后的行为 |
| `core_layering` | 扫描 `src/core`，不允许出现 Widgets/Gui 的 include 或链接 |
| `ui_tests` | offscreen：注册表排序与校验；`ToolHost` 的惰性建页、生命周期配对、失败隔离；两层导航；导航锁（含键盘、焦点恢复、关闭确认、销毁安全）；状态与日志；全部键盘快捷键；主题展开；内置工具 |

`ui_tests` 里的工具大多是 `FakeTool`，所以这些测试不依赖任何具体业务工具。设置
`TENGWANGGE_SCREENSHOT_DIR` 会在关键状态保存截图。
