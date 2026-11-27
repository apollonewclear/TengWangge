# 滕王阁 TengWangge

面向麒麟桌面的可扩展工具箱平台：一个“外壳 + 多个工具模块”的 Qt 桌面程序。外壳采用高密度浅色工业工程软件工作台布局——**顶部 68px 大图标横向模块栏（图标上、文字下）+ 单页自动隐藏的紧凑二层页签条 + 全宽左树右表工作区 + 常驻极简消息日志框 + 实时有效时长状态栏**，并提供长任务期间的引用计数导航锁；工具以编译期接入的静态库形式挂在外壳上。

当前已内置四个模块：**概览（`welcome`）**、**文件对比（`compare`，参考 Beyond Compare，提供本地文件夹对比、文本并排编辑对比、INI 配置按节键核对与三方合并）**、**任务演示（`task-demo`）** 与 **本机信息（`sysinfo`）**。

- 仅桌面端；Qt 5.15、C++17、CMake；不使用 Qt 6 专有 API
- 核心库（`src/core`）只依赖 Qt Core，与 Widgets 外壳分离
- 界面为简体中文；不含密钥、凭据，不改系统目录
- 数据目录是产品自己的隔离路径，不依赖本机上任何其他产品的路径

文档：[架构说明](docs/ARCHITECTURE.md) · [如何新增一个工具](docs/ADDING_A_TOOL.md) · [变更记录](docs/CHANGELOG.md)

## 目录

```
CMakeLists.txt
cmake/                      ctest 用的分层检查脚本
packaging/TengWangge.desktop
src/core/                   平台语义（Qt Core）：日志、状态、导航锁、数据目录、服务句柄
src/shell/                  Widgets 外壳：ITool、注册表、ToolHost、主窗口、主题 + QSS
src/tools/                  内置工具，每个工具一个静态库；builtin_tools.cpp 是注册清单
src/app/                    可执行文件 TengWangge
tests/                      core_tests、core_layering、ui_tests（offscreen）
docs/
```

## 环境要求

| 组件 | 要求 | 说明 |
| --- | --- | --- |
| 编译器 | GCC 10 或更新（也用 Clang 验证过），支持 C++17 | 麒麟 KylinSec 3.5 自带 GCC 10 |
| CMake | ≥ 3.16（按 3.22 验证） | |
| Qt | 5.15（Core、Widgets、Test） | 不支持 Qt 6 |
| 字体 | 任意中文字体；推荐 Noto Sans CJK SC / 思源黑体 | 主题按 Noto Sans CJK SC → 思源黑体 → 文泉驿微米黑 → 微软雅黑 → sans-serif 回退 |

安装依赖（包名因发行版而异，下面是常见写法，请按本机包管理器确认）：

```bash
# RPM 系（麒麟 KylinSec / openEuler 系）
sudo dnf install gcc-c++ cmake make qt5-qtbase-devel

# Debian / Ubuntu / 优麒麟
sudo apt install build-essential cmake qtbase5-dev
```

`qt5-qtbase-devel` / `qtbase5-dev` 已包含 Core、Gui、Widgets 和 Test 的头文件与库。

## 编译

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

可选项：

| 选项 | 默认 | 作用 |
| --- | --- | --- |
| `-DCMAKE_BUILD_TYPE=Debug` | Release | 默认 Release |
| `-DTENGWANGGE_BUILD_TESTS=OFF` | ON | 不编译测试（不需要 Qt Test） |
| `-DTENGWANGGE_WARNINGS_AS_ERRORS=ON` | OFF | 把 `-Wall -Wextra -Wpedantic` 的警告当作错误 |
| `-DTENGWANGGE_ENABLE_SANITIZERS=ON` | OFF | 开启 AddressSanitizer 与 UndefinedBehaviorSanitizer |
| `-DTENGWANGGE_PREFER_SYSTEM_QT=ON` | ON | Linux 下优先链接系统 `/usr/lib64` 或 `/usr/lib` 的 Qt 5.15 |

也支持 CMake Presets（CMake ≥ 3.21）：

```bash
cmake --preset release && cmake --build --preset release -j"$(nproc)" && ctest --preset release
```

在麒麟 KylinSec / openEuler 上构建 RPM 安装包（需安装 `rpm-build`）：

```bash
./packaging/build_rpm.sh
```

Qt 不在默认路径时：`cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt5.15`。

## 运行

```bash
./build/src/app/TengWangge
```

首次运行会在 `~/.local/share/TengWangge/` 下创建数据目录（权限 0700）。想换位置，或在测试、
受控环境里隔离数据：

```bash
TENGWANGGE_DATA_DIR=/tmp/tengwangge-data ./build/src/app/TengWangge
```

根目录不允许是空、`/` 或系统目录（`/etc`、`/usr` 等）；不合规会直接报错退出，不会回退到别处。

命令行：

| 参数 | 说明 |
| --- | --- |
| `--version` | 打印版本 |
| `--tool <id>` | 启动后选中某个工具，如 `task-demo` |
| `--page <id>` | 启动后选中当前工具的某一页 |
| `--show-log` | 启动时展开日志面板 |
| `--window-size 宽x高` | 窗口大小，例如 `1280x800` |
| `--screenshot <文件>` | 渲染后保存 PNG 并退出，可配合 `QT_QPA_PLATFORM=offscreen` 在无显示环境下出图 |

安装（可选）：`cmake --install build --prefix ~/.local` 会安装可执行文件和 `.desktop` 文件，
不会写任何系统目录（前提是 prefix 不指向系统目录）。

### 键盘操作

| 键 | 作用 |
| --- | --- |
| `Ctrl+1 … Ctrl+9` | 切换到第 N 个工具 |
| `Alt+↑ / Alt+↓` | 上一个 / 下一个工具 |
| `Ctrl+PageUp / PageDown` | 当前工具的上一页 / 下一页 |
| `F6` | 在工具栏、页面条、页面、日志之间移动焦点 |
| `Ctrl+L` | 显示 / 隐藏日志面板 |
| `F1` | 键盘帮助 |

长任务期间导航被锁定：工具栏和页面条变灰，状态栏显示“导航已锁定：原因”，切换类快捷键无效。
在“任务演示”工具里可以直接体验。

## 测试

```bash
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

| 测试 | 说明 |
| --- | --- |
| `core_tests` | 核心库单元测试，只用 `QCoreApplication` |
| `core_layering` | 检查 `src/core` 没有引入 Widgets / Gui |
| `ui_tests` | offscreen UI 测试：注册表、生命周期、两层导航、导航锁、状态栏、日志、键盘、主题、内置工具 |

`ui_tests` 需要 Qt 的 `offscreen` 平台插件（Qt 5.15 默认带），无需 X11/Wayland 显示。
想看关键状态的截图：

```bash
mkdir -p /tmp/tw-shots
TENGWANGGE_SCREENSHOT_DIR=/tmp/tw-shots QT_QPA_PLATFORM=offscreen ./build/tests/tengwangge_ui_tests
```

## 验证状态

本仓库在下列环境验证过（全部 `-DTENGWANGGE_WARNINGS_AS_ERRORS=ON`，三类测试全部通过）：
- **麒麟 KylinSec OS 3.5**（内核 `4.19.90-2408.1.0.0288.kb30.ky3_5.x86_64`，GCC 10.3.1，CMake 3.22.0，Qt 5.15.2）
- Ubuntu 24.04 + Qt 5.15.13（g++ 10、g++ 13、clang++ 18；CMake 3.22.6 与 3.28）

## 许可与素材

界面图标由代码绘制，没有引入任何第三方图片或字体文件；没有复制第三方商业软件的界面或素材。
