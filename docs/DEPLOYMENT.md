# 双架构免源码部署与发布规范（麒麟 x86_64 + ARM64）

> **长期发布约定**：滕王阁（TengWangge）今后每次功能迭代与版本发布，**默认同步产出两套免源码直接部署包**，分别适配：
> 1. **麒麟 `x86_64`**（如 KylinSec OS 3.5 `ky3_5.x86_64`）
> 2. **麒麟 `aarch64`（ARM64）**（如 KylinSec OS 3.5 `ky3_5.aarch64`，`glibc 2.34`）

所有部署包均**不包含任何源码**，仅含 `strip` 后的发布二进制、Qt 5.15 运行库与核心插件、启动脚本及桌面快捷方式安装器，解压即用、无需 root。

---

## 1. 标准产物清单

| 架构 | 产物文件名 | 体积参考 | 适用场景 |
| --- | --- | --- | --- |
| **x86_64 便携全量版** | `TengWangge-<ver>-kylin-x86_64.tar.gz` | ~23 MB | 任意同架构 Linux / 麒麟 x86_64（优先用系统 Qt5，缺失时自动回退包内 Qt5 运行库） |
| **x86_64 轻量版** | `TengWangge-<ver>-kylin-x86_64-lite.tar.gz` | ~312 KB | 已预装同版本系统 Qt5（`libQt5*KS.so.5`）的 KylinSec 3.5 x86_64 桌面机 |
| **ARM64 便携全量版** | `TengWangge-<ver>-kylin-aarch64.tar.gz` | ~27 MB | 麒麟 ARM64（`ky3_5.aarch64` / `openEuler 22.03` 及 `glibc >= 2.34` 的 ARM64 桌面机，内置 Qt 5.15 运行库与 `xcb` 插件） |

每个 `.tar.gz` 均附带同名 `.sha256` 校验文件，供现场传输后核验完整性。

---

## 2. 包内目录结构

```text
TengWangge-<ver>-kylin-<arch>/
├── TengWangge.sh                  # 启动脚本（自动配置 LD_LIBRARY_PATH 与 QT_PLUGIN_PATH）
├── install_desktop.sh             # 用户级桌面与应用菜单快捷方式安装器（无需 root）
├── DEPLOY.md                      # 现场快速部署说明
├── bin/
│   └── TengWangge                 # strip 后的 Release 可执行程序
├── lib/                           # Qt 5.15 核心运行库与非系统辅助库（轻量版不含）
├── plugins/                       # platforms(libqxcb/libqoffscreen)、imageformats 等插件（轻量版不含）
└── share/
    ├── applications/TengWangge.desktop
    └── icons/hicolor/scalable/apps/tengwangge.svg
```

---

## 3. 目标机部署步骤（x86_64 与 ARM64 通用）

```bash
# 1. 校验完整性（可选）
sha256sum -c TengWangge-0.1.0-kylin-<arch>.tar.gz.sha256

# 2. 解压到任意用户目录（无需 root）
tar -xzf TengWangge-0.1.0-kylin-<arch>.tar.gz
cd TengWangge-0.1.0-kylin-<arch>

# 3. 直接启动
./TengWangge.sh

# 或直接打开文件对比工具
./TengWangge.sh --tool compare

# 4. 可选：创建开始菜单与桌面图标（写入 ~/.local/share/applications 与 ~/Desktop）
./install_desktop.sh
```

> - 运行日志、会话记忆与对比历史均保存在 `~/.local/share/TengWangge/`（权限 `0700`），亦可通过 `TENGWANGGE_DATA_DIR=/自定义路径 ./TengWangge.sh` 指定隔离目录。
> - x86_64 便携全量版默认优先系统 Qt5，若需强制使用包内运行库，执行 `TENGWANGGE_USE_BUNDLED_QT=1 ./TengWangge.sh`。

---

## 4. 双架构标准构建与验证流程

### 4.1 麒麟 `x86_64` 构建与验证
- **构建环境**：麒麟 KylinSec OS 3.5 x86_64（GCC 10.3.1 / CMake 3.22 / Qt 5.15.2）。
- **步骤**：
  1. `cmake --preset release && cmake --build --preset release -j"$(nproc)"`
  2. `QT_QPA_PLATFORM=offscreen ctest --preset release --output-on-failure`（`core_tests`、`core_layering`、`ui_tests` 全部通过）
  3. 打包 `TengWangge-<ver>-kylin-x86_64.tar.gz` 与 `-lite.tar.gz`，生成 `.sha256`
  4. 洁净目录分别验证默认系统 Qt5 模式与 `TENGWANGGE_USE_BUNDLED_QT=1` 兜底模式

### 4.2 麒麟 `aarch64`（ARM64）构建与验证
- **构建环境**：Apple Silicon Mac 上的轻量 `colima` + `docker` 运行 `openeuler/openeuler:22.03-lts`（`--platform linux/arm64`，`glibc 2.34` / GCC 10.3.1 / Qt 5.15.2，与 `ky3_5.aarch64` ABI 同源）。
- **步骤**：
  1. 启动容器环境：`colima start --arch aarch64 --cpu 4 --memory 4`
  2. 在 `openeuler/openeuler:22.03-lts` 中原生编译并运行全量 `ctest`（`HOME=/tmp/tw_home XDG_DATA_HOME=/tmp/tw_home/.local/share QT_QPA_PLATFORM=offscreen ctest --preset release`）
  3. 剥离二进制符号并收集 `bin/TengWangge` 及 `plugins/platforms/libqxcb.so` 所需的非系统运行库（保留 `glibc`/`libstdc++`/`OpenGL`/`libX11`/`dbus` 使用目标机原生库），打包 `TengWangge-<ver>-kylin-aarch64.tar.gz` 与 `.sha256`
  4. **无 Qt5 洁净容器验证**：在新拉起的无 Qt5 裸 `openeuler/openeuler:22.03-lts` 容器中核验 `bin/TengWangge` 与 `libqxcb.so` 均 **0 missing libs**，且 `--version` 与 `--tool compare --screenshot` 正常执行
  5. 构建完成后执行 `colima stop` 释放宿主机内存

---

## 5. 当前发布记录（v0.1.0，基于 commit `4887a1d`）

| 产物 | 架构 | SHA-256 | 验证状态 |
| --- | --- | --- | --- |
| `TengWangge-0.1.0-kylin-x86_64.tar.gz` | x86_64（全量） | `150b86092be287facf2dce15ea28805c4a97089b7ece5f22d7d1a00a1c96958d` | KylinSec 3.5 双模式实测通过 |
| `TengWangge-0.1.0-kylin-x86_64-lite.tar.gz` | x86_64（轻量） | `6b28326a001188140ec9646400f12b93ea7ccb50231a150f0c38c4ac30a0d57f` | KylinSec 3.5 系统 Qt5 实测通过 |
| `TengWangge-0.1.0-kylin-aarch64.tar.gz` | ARM64（全量） | `e4c668fd5e456f6ddef6798f3a6dd42806b82f21f471fd3dbcfcea56a90673ab` | openEuler 22.03 ARM64 全量测试 + 无 Qt5 裸容器 0 缺库验证通过 |
