# 变更记录（Changelog）

本文件记录滕王阁（TengWangge）每一批代码改动、修改前备份位置、麒麟实机验证结果与对应 Git 提交，便于追溯与回滚。

---

## 2026-11-28 · 双架构免源码部署包发布与规范固化（x86_64 + ARM64）

**主题**：新增免源码直接部署打包方案并固化为长期发布约定——今后每次版本发布均同步产出 **麒麟 `x86_64`** 与 **麒麟 `aarch64`（ARM64）** 两套免源码部署包

### 产出与校验（基于 commit `5c7671d`）
- **x86_64 便携全量版**（23 MB）：`TengWangge-0.1.0-kylin-x86_64.tar.gz`（SHA-256: `4637a688c8254f118e73fed8fbf71a57924b3738a5e6aca34cc47ee67c744d92`）
- **x86_64 轻量版**（312 KB）：`TengWangge-0.1.0-kylin-x86_64-lite.tar.gz`（SHA-256: `23788864d81ba265ff51c7c2f2d39f6cf93c73ac662bee3d30383cac5dff7b4f`）
- **ARM64 便携全量版**（27 MB）：`TengWangge-0.1.0-kylin-aarch64.tar.gz`（SHA-256: `ab4b4ecbd90194f3a471123f6a6e685b9b1057a097f718c7d9e6bd441e292008`）

### 双架构验证结果
- **x86_64（KylinSec OS 3.5）**：在洁净目录分别通过系统 Qt5 模式与 `TENGWANGGE_USE_BUNDLED_QT=1` 兜底模式启动与渲染验证。
- **ARM64（`openEuler 22.03 LTS aarch64` / `glibc 2.34`，同源 `ky3_5.aarch64`）**：原生编译零警告，`core_tests` **48/48**、`core_layering`、`ui_tests` **46/46** 全部通过；在无 Qt5 洁净容器验证 `bin/TengWangge` 与 `plugins/platforms/libqxcb.so` 均 **0 missing libs** 并成功渲染。

### 文档更新
- 新增 [docs/DEPLOYMENT.md](DEPLOYMENT.md)：固化 `x86_64` + `aarch64` 双架构构建、洁净验证、SHA-256 校验与目标机解压即用规范。
- 更新 `README.md`：补充双架构验证环境说明与部署规范链接。

---

## 2026-11-27 · 第六批改进（commit `5c7671d`）

**主题**：围绕麒麟电力现场高频使用场景，深度增强 `compare`（文件对比）模块的编码兼容、目录浏览、批量同步、文内检索与键盘效率

### 修改前备份
- **本地 Git 备份分支**：`backup/pre-compare-enhance-e3ed62b`
- **本地归档**：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_compare_enhance_e3ed62b.{bundle,tar.gz}`
- **麒麟远程备份**：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_compare_enhance_e3ed62b.tar.gz`

### 主要改动
1. **多字符编码自动探测与原编码无损回写**（`compare_engine.{h,cpp}`、`text_compare_page.{h,cpp}`、`ini_compare_page.{h,cpp}`）
   - 新增 `TextEncoding`（`Auto` / `Utf8` / `Gb18030` / `Utf16`），支持 BOM 检测与严格 UTF-8 校验，非 UTF-8 文件自动回退 `GB18030/GBK`，彻底解决麒麟现场老配置文件中文乱码问题。
   - 文本对比页与 INI 对比页新增编码下拉框，标题栏实时标注检测编码，保存时默认按原编码回写。
2. **文件夹对比树形视图、多选批量同步、自定义忽略与拖放**（`folder_compare_page.{h,cpp}`）
   - 新增「列表 / 树形」双视图切换，树形模式按目录层级折叠并自动在父目录行汇总子项差异计数（`差异 N` / `一致`）。
   - 开启 `ExtendedSelection` 多选，支持一次性批量复制到右侧或左侧（覆盖前可选自动 `.bak` 备份）。
   - 新增可编辑的忽略规则输入框（默认 `.git;*.bak;*.o;build`，分号/逗号分隔）与本地目录/文件拖放加载。
3. **Beyond Compare 快捷键、文内查找与历史会话**
   - 文本比较页支持 `F7`/`F8` 跳上一处/下一处差异、`Alt+←/→` 当前块双向复制、`Ctrl+F` 聚焦文内查找框（左右同步定位）、`Ctrl+S` 保存。
   - 三方合并页支持 `F7`/`F8` 跳冲突、`Ctrl+S` 保存合并结果；INI 配置比较页支持多行批量同步与 `Ctrl+S` 保存。
   - 隔离目录 `<dataDir>/compare_history.json`（`0600` 权限）持久化最近 10 组对比路径，各页顶栏「历史」按钮一键回溯。
4. **自动化测试**
   - `tests/ui_tests.cpp`：新增 `compare_encodingAutoDetectAndHistory`（验证 GB18030/UTF-16 读写探测、历史去重与忽略规则解析），并扩展 `compare_folderDiffAndUiWorkflow` 覆盖树形视图、批量复制与文内查找。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- `cmake --build --preset release`：`-Wall -Wextra -Wpedantic -Werror` 零警告通过。
- `QT_QPA_PLATFORM=offscreen ctest --preset release`：`core_tests` **48/48**、`core_layering`、`ui_tests` **46/46** 全部通过。
- 实机渲染核验文件夹树形视图、文本编码/查找高亮与 INI 编码切换界面，排版清晰无横向滚动条溢出。

---

## 2026-11-27 · 第五批改进（commit `e3ed62b`）

**主题**：上线首个正式业务工具模块 `compare`（文件对比），覆盖本地文件夹递归对比、文本并排编辑对比、INI/配置文件按节键核对与 Base/Left/Right 三方合并

### 修改前备份与历史备份清理
- 已按确认删除前四批历史备份（`backup/pre-batch1..4` 及本地/麒麟归档）。
- **本批修改前备份**：
  - 本地 Git 备份分支：`backup/pre-compare-0503d6a`
  - 本地归档：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_compare_0503d6a.{bundle,tar.gz}`
  - 麒麟远程备份：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_compare_0503d6a.tar.gz`

### 主要改动
1. **纯 Qt Core 比较与合并核心引擎**（`src/tools/compare/compare_engine.{h,cpp}`）
   - **文本比较**：行级 LCS / 锚点分治比对（支持忽略空白、忽略大小写、忽略空行）+ 行内字符级差异定位 + 差异块双向复制（`applyHunkToRight` / `applyHunkToLeft`）+ Unified 报告导出 + `QSaveFile` 原子保存与自动 `.bak` 备份。
   - **三方合并**：基于 `Base / Left / Right` 的非重叠自动合并与冲突检测，支持单块/批量采纳左侧、右侧、基线、双侧保留（左+右）及冲突标记生成。
   - **INI 配置按节键比对**：面向 `sca_model_conf.ini` 等工程配置，解析并保留原文件注释与行尾注释，支持按 `[Section]` + 键名比对、双向键值同步与无损序列化回写。
   - **文件夹递归比对**：支持“大小与时间”与“内容 SHA-256”两种模式、可取消扫描、进度回调与带 `.bak` 备份的安全文件复制。
2. **四个 Beyond Compare 风格页面与模块注册**（`src/tools/compare/`）
   - `folder_compare_page.{h,cpp}`：基于 `TaskRunner` 后台工作线程与 RAII 导航锁扫描本地目录，支持过滤（全部/仅差异/仅左/仅右）、双向文件同步，**双击差异文件自动识别 `.ini/.conf/.cfg` 或普通文本并跳转到对应对比页**。
   - `text_compare_page.{h,cpp}`：带行号边栏的左右并排可编辑代码框、同步滚动、行级与字符级高亮、右侧差异概览缩略条、`仅差异` 行表切换、差异块导航/复制与 `.bak` 保存。
   - `ini_compare_page.{h,cpp}`：左侧 `[Section]` 差异统计表 + 五圆点分割条 + 右侧节键差异明细表，支持原位编辑值、选中项左右同步与导出报告。
   - `three_way_merge_page.{h,cpp}`：上方三栏（左侧 / 基线 / 右侧）源视图 + 下方冲突块列表与可直接编辑的合并输出区。
   - `src/shell/icon_factory.cpp`、`src/shell/resources/theme.qss`、`src/tools/builtin_tools.cpp`：新增 `compare / folder / merge / save / swap / copy_right / copy_left` 七枚双色矢量图标，补充 `QPushButton:checked` 选中态样式，注册 `CompareTool`（`order = 15`）。
3. **测试与文档**
   - `tests/ui_tests.cpp`：新增 `compare_textDiffInlineAndHunkCopy`、`compare_threeWayMergeAndConflictResolve`、`compare_iniSectionKeyDiffAndPreserveComments`、`compare_folderDiffAndUiWorkflow` 四组单元与全窗口 UI 测试。
   - `README.md`、`docs/ARCHITECTURE.md`、`docs/CHANGELOG.md`：同步更新模块说明与变更记录。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- `cmake --preset release && cmake --build --preset release`：`-Wall -Wextra -Wpedantic -Werror` 零警告通过。
- `QT_QPA_PLATFORM=offscreen ctest --preset release`：`core_tests` **48/48**、`core_layering`、`ui_tests` **45/45** 全部通过。
- 实机渲染核验文件夹、文本、INI、三方合并四页及顶栏新图标，排版与高亮无遮挡异常。

---

## 2026-11-27 · 第四批改进（commit `0503d6a`）

**主题**：解除旧版左侧边栏与粗糙简笔画限制，按电力自动化工程工具视觉规范全面重构顶部大图标 Ribbon 栏、高精度双色/多色矢量图标库、左树右表网格工作区与常驻极简消息框

### 修改前备份
- **本地 Git 备份分支**：`backup/pre-batch4-5d9f961`
- **本地归档**：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_batch4_5d9f961.{bundle,tar.gz}`
- **麒麟远程备份**：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_batch4_5d9f961.tar.gz`

### 主要改动
1. **统一 68px 顶部大图标横向模块栏（去除外层 204px 左侧栏与叠床架屋双顶栏）**
   - `src/shell/tool_rail.{h,cpp}`、`src/shell/main_window.{h,cpp}`：将 `ToolRail` 重构为横向 `QListView::LeftToRight` 大图标模块栏（`74×60` 单元，`30×30` 精细双色图标居上、中文标题居中居下，选中态为克制的 `#e3edf8` 浅冰蓝圆角底框与 `#b5cfea` 细描边）；顶栏右侧对称排布 `消息日志`、`操作帮助`、`退出` 大图标工具按钮；窗口标题升级为 `滕王阁@<主机名> — <模块> · <页面>`。
   - `src/shell/tool_view.{h,cpp}`：当工具仅有 1 个页面（如“本机信息”）时自动隐藏二层页签条 `#pageStrip`，多页面工具展示紧凑 `32px` 上浮白底页签条。
2. **32×32 高精度双色/多色矢量图标库与浅灰白克制配色**
   - `src/shell/icon_factory.cpp`：升级至 `32×32` 浮点坐标系重绘全套工程图标——`overview`（三枚钢蓝圆角方块 + 右上 22° 倾斜暖橙方块）、`task`（蓝橙分离扇形饼图）、`info`（三道横向参数滑轨）、`environment`（等轴测三维立方体 + 右下暖橙指针）、`status`/`scan`（深色外环 + 翠绿扫描扇区）、`exit`（钢蓝门框 + 暖橙右向箭头），以及 `16×16` 动作按钮彩色图标（橙底白箭头 `refresh`、橙底白十字 `add`、蓝框上箭头 `update`、红框垃圾桶 `delete`）。
   - `src/shell/theme.{h,cpp}`、`src/shell/resources/theme.qss`：全面切换为清爽克制的 `#f6f7f9` 浅灰顶栏/底栏 + `#ffffff` 纯白表格/树面板，去除刺眼的大面积纯色色块按钮。
3. **五圆点分割抓手与三个内置工具“左树/表 + 右侧动作栏与真实网格表格”重构**
   - `src/shell/page_chrome.{h,cpp}`：实现带五圆点抓手（纵向 `⋮` / 横向 `.....`）的 `ui::makeSplitter`、白底彩色小图标按钮 `ui::makeActionButton` 与统一网格表格初始化器 `ui::configureDataTable`。
   - `src/tools/welcome/welcome_tool.cpp`：重构为左侧 `QTreeWidget` 模块/机制分类树 + 右侧刷新/跳转动作条 + 右对齐检索框 + 6 列 `QTableWidget`；“键盘操作”页升级为支持关键字检索的标准网格表格。
   - `src/tools/sysinfo/sysinfo_tool.cpp`：单页面全幅展示左侧两列统计表（`类型名 | 数量`）+ 右侧 10 项系统/内核/Qt/DPI/隔离目录实时明细网格表格。
   - `src/tools/task_demo/task_demo_tool.{h,cpp}`：重构为左侧执行方案树 + 右侧任务控制条、紧凑进度条与 10 步阶段状态实时跟踪 `QTableWidget`。
4. **常驻极简底部消息框与实时有效时长状态栏**
   - `src/shell/log_panel.{h,cpp}`：默认展开（`~112px`）并隐藏厚重工具头，呈现纯净白底细框 `[INFO][yyyy/MM/dd HH:mm:ss] ...` 实时输出区；支持右键菜单按需呼出搜索/级别筛选栏、导出或清空。
   - `src/shell/status_strip.{h,cpp}`：底部 26px 状态栏新增每秒刷新的 `用户:<user>  启动时间:yyyy-MM-dd HH:mm:ss  有效时长:HH:mm:ss` 遥测信息。
5. **测试与文档同步更新**
   - `tests/ui_tests.cpp`：适配默认常驻消息框行为，并新增单页面自动隐藏页签条与多页面显示页签条断言。
   - `README.md`、`docs/ARCHITECTURE.md`、`docs/ADDING_A_TOOL.md`、`docs/CHANGELOG.md`：同步更新架构图、配色说明与组件脚手架文档。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- `cmake --preset release && cmake --build --preset release`：`-Wall -Wextra -Wpedantic -Werror` 零警告编译通过。
- `QT_QPA_PLATFORM=offscreen ctest --preset release`：`core_tests`、`core_layering`、`ui_tests` 全量通过，并完成多分辨率/多状态 PNG 实机渲染截图比对。

---

## 2026-11-27 · 第三批改进（commit `eff212a`）

**主题**：生产构建规范化、RPM 打包脚本、页面级激活回调与受控跨模块跳转

### 修改前备份
- **本地 Git 备份分支**：`backup/pre-batch3-90d576f`
- **本地归档**：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_batch3_90d576f.{bundle,tar.gz}`
- **麒麟远程备份**：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_batch3_90d576f.tar.gz`

### 主要改动
1. **构建与分发规范化**
   - `CMakeLists.txt`、`src/app/CMakeLists.txt`：新增 `TENGWANGGE_PREFER_SYSTEM_QT=ON`（Linux 下优先自动绑定 `/usr/lib64/cmake/Qt5` 或 `/usr/lib` 系统 Qt 5.15）、`TENGWANGGE_ENABLE_SANITIZERS=ON`（ASan + UBSan）、跨编译器警告选项，以及 `CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF`（消除安装二进制中的构建机 RPATH 残留）。
   - `CMakePresets.json`：提供 `release` 与 `debug-asan` 一键构建/测试预设。
   - `packaging/tengwangge.spec`、`packaging/build_rpm.sh`：新增适配麒麟 KylinSec / openEuler 的 RPM 打包规范与一键打包脚本。
   - `packaging/github-actions-ci.yml`：提供 Release + 安装校验与 ASan/UBSan 双任务 CI 模板。
2. **生命周期与导航扩展**
   - `src/shell/i_tool.h`、`src/shell/tool_host.{h,cpp}`：新增 `ITool::pageActivated(const QString &pageId)` 回调，在工具激活或工具内切换页签时触发并兜底异常日志。
   - `src/core/platform_services.{h,cpp}`、`src/shell/main_window.cpp`：新增线程安全的 `ToolServices::requestNavigation(targetToolId, targetPageId)` 受控跳转服务，导航锁定期间自动拒绝并记入告警日志。
   - `src/tools/welcome/welcome_tool.{h,cpp}`：概览页新增“快捷进入”操作栏并重写 `pageActivated()`。
3. **测试与文档**
   - `tests/ui_tests.cpp`：新增 `host_pageActivatedFiresOnToolAndTabSwitch`、`window_requestNavigationHonoursGate`。
   - `README.md`、`docs/ADDING_A_TOOL.md`：更新预设构建、Sanitizer、RPM 打包与新钩子用法。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- `cmake --preset release && cmake --build --preset release`：自动匹配 `/usr/lib64/cmake/Qt5`，零警告通过。
- `ctest --preset release`：`core_tests` **48/48** 通过、`core_layering` 通过、`ui_tests` **41/41** 通过。
- `cmake --install` 与 `readelf -d` 核验：二进制、`.desktop`、`.svg` 图标安装完整，无残留 `RPATH/RUNPATH`，仅依赖系统原生 `libQt5*KS.so.5`。

---

## 2026-11-27 · 第二批改进（commit `90d576f`）

**主题**：业务接入准备能力增强（滚动日志落盘、日志筛选导出、隔离会话持久化、多线程任务器与通用页面组件）

### 修改前备份
- **本地 Git 备份分支**：`backup/pre-batch2-49f055a`
- **本地归档**：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_batch2_49f055a.{bundle,tar.gz}`
- **麒麟远程备份**：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_batch2_49f055a.tar.gz`

### 主要改动
1. **日志滚动落盘与面板升级**
   - `src/core/log_file_sink.{h,cpp}`、`src/core/platform_services.{h,cpp}`：新增 `LogFileSink`，自动将日志追加写入 `<root>/logs/tengwangge.log`（`0600` 权限），超限按 `.1 .. .3` 轮转。
   - `src/shell/log_panel.{h,cpp}`、`src/shell/resources/theme.qss`、`src/shell/icon_factory.cpp`：日志标题栏新增关键字实时搜索框、级别筛选下拉框（全部级别 / 警告与错误 / 仅错误）及 `[导出]` 按钮与 `exportToFile()` 接口。
2. **标准后台任务运行器与多任务锁展示**
   - `src/core/task_runner.{h,cpp}`：纯 Qt Core 实现 `TaskRunner` 与 `TaskControl`，提供独立工作线程、`QWaitCondition` 零延迟可中断休眠 `sleepMs()`、协作取消 `cancelAndWait()` / `abort()`、异常捕获与自动 RAII `NavigationLock` 管理。
   - `src/tools/task_demo/task_demo_tool.{h,cpp}`：迁移至真实多线程 `TaskRunner` 驱动。
   - `src/core/navigation_gate.{h,cpp}`、`src/shell/status_strip.{h,cpp}`、`src/shell/main_window.cpp`：新增 `NavigationGate::activeHolds()`，状态栏与关闭确认弹窗完整展示多任务并发锁定原因。
3. **隔离会话配置与通用 UI 脚手架**
   - `src/core/data_paths.{h,cpp}`、`src/core/shell_config.{h,cpp}`、`src/shell/main_window.{h,cpp}`、`src/app/main.cpp`：在 `<root>/config/shell.ini`（`0600`）持久化窗口尺寸、日志面板开关/高度与上次选中工具/页面，预留 `ToolServices::settingsFile()`。
   - `src/shell/page_chrome.{h,cpp}`：抽取 `tw::ui` 共用标签/工具条/卡片/表格行构造器，重构三个内置工具消除重复样板代码。
4. **测试与文档**
   - `tests/core_tests.cpp`、`tests/ui_tests.cpp`：新增磁盘滚动落盘与权限测试、`ShellConfig` 序列化测试、`TaskRunner` 正常/异常/秒级中断测试、日志多维筛选导出测试、跨重启会话恢复测试。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- `-DTENGWANGGE_WARNINGS_AS_ERRORS=ON` 零警告通过。
- `ctest`：`core_tests` **48/48** 通过、`core_layering` 通过、`ui_tests` **39/39** 通过。
- 实机 `stat` 核验 `<root>/logs/tengwangge.log` 权限为 `0600`；`900×560` 最小窗口下日志筛选导出栏排版完整无挤压。

---

## 2026-11-26 · 第一批改进（commit `49f055a`）

**主题**：核心/外壳底座安全与生命周期加固、对齐工业工作台设计稿的浅色 UI 重构

### 修改前备份
- **本地 Git 备份分支**：`backup/pre-batch1-7a9d11b`
- **本地归档**：`/Users/Apple/Desktop/work/code/gemini/backups/tengwangge_pre_batch1_7a9d11b.{bundle,tar.gz}`
- **麒麟远程备份**：`/home/nusp/TengWangge_backups/TengWangge_gemini_pre_batch1_7a9d11b.tar.gz`

### 主要改动
1. **核心层安全与线程加固**
   - `src/core/data_paths.{h,cpp}`：解析符号链接（`canonicalFilePath`）封堵指向系统目录的软链接绕过；无论根目录是否预建，均强制对根目录及 `tools/`、`tools/<toolId>`、`logs/` 收敛 `0700` 权限。
   - `src/core/platform_services.cpp`：`ToolServices` 的日志与状态接口支持工作线程安全调用（自动 `Qt::QueuedConnection` 投递回主线程）。
   - `src/core/status_board.{h,cpp}`：改用按 `toolId` 复用的 `QTimer`，消除高频状态更新堆积 `singleShot` 定时器。
2. **外壳生命周期与启动修复**
   - `src/shell/tool_host.{h,cpp}`：记录 `initialized` 标记，确保 `initialize()` 成功后即使 `createPages()` 失败退出时仍调用 `shutdown()`；检测并去重重复的 `QWidget*` 指针防 double-free；为 `activated/deactivated/shutdown` 异常补齐平台错误日志。
   - `src/shell/main_window.{h,cpp}`、`src/app/main.cpp`：支持构造期传入 `initialToolId`，消除 `--tool` 启动时提前实例化默认第一个工具及创建其数据目录的问题；开启 Qt 5.15 `AA_EnableHighDpiScaling` 与 `AA_UseHighDpiPixmaps`；无头平台下目录报错不再阻塞弹窗；补齐窗口图标与 `packaging/TengWangge.desktop` / `packaging/tengwangge.svg`。
3. **浅色工业工作台 UI 重构**
   - `src/shell/theme.{h,cpp}`、`src/shell/resources/theme.qss`、`src/shell/icon_factory.{h,cpp}`、`src/shell/tool_rail.{h,cpp}`、`src/shell/tool_view.{h,cpp}`、`src/shell/log_panel.cpp`、`src/shell/status_strip.{h,cpp}`：
     - 切换为浅灰外框 + 纯白表格面板 + 钢蓝选中态（`#2376b7`）+ 暖橙双色图标点缀（`#e67e22`）+ 青色/红色状态胶囊；
     - `IconFactory` 增加 `QPixmapCache` 缓存与双色绘制，修复 `ToolRailDelegate` 非像素字号与不可用工具视觉语义；
     - `ToolView` 外包无边框 `QScrollArea` 并自动同步选中页签反白图标；
     - 新增五圆点抓手分割条、框线消息日志与 `[平台]` 来源标记、状态栏用户/启动时间/版本摘要；
     - 修复 `TaskDemoPage` 进度条文字对比度问题，将内置工具页面改造为工业表格卡片风格。
4. **测试与文档**
   - `tests/core_tests.cpp`、`tests/ui_tests.cpp`：新增软链接拦截、`0700` 权限收敛、跨线程服务调用、重复控件防 double-free、`createPages` 失败后 `shutdown` 闭环、`initialToolId` 懒加载测试。

### 麒麟实机验证结果（KylinSec OS 3.5 / GCC 10.3.1 / Qt 5.15.2）
- 首次在真实麒麟 KylinSec OS 3.5 环境下零警告通过编译（链接系统原生 `libQt5*KS.so.5`，Release 产物 `444KB`）。
- `ctest`：`core_tests` **45/45** 通过、`core_layering` 通过、`ui_tests` **37/37** 通过。
- 实机核验 `--tool sysinfo` 不再误建 `tools/welcome`，目录权限全部为 `700`，所有 UI 场景截图渲染清晰无遮挡。
