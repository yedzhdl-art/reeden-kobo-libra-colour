# Reeden Kobo · Libra Colour 适配版

> **这是一个派生作品，不是独立项目。**
> 本仓库的全部基础 —— C 宿主、Flutter Embedder 接线、framebuffer 与刷新策略、电源与网络管理、安装器、SHA-256 校验与每次启动的完整性复核、72 项测试，以及大部分文档 —— 都来自 **[SSNQ/reeden-kobo](https://github.com/SSNQ/reeden-kobo) v0.7.8**（commit `677cc9f`）。我们做的只是「让这套已经很完整的兼容层在另一台机器上跑起来」：新增 4 个源文件、修改 15 个文件。没有原作者的工作，就不会有这个适配。完整的致谢与我们逐条参考过的资料，请见 **[NOTICE.md](NOTICE.md)**；相对上游的具体改动见 **[CHANGES.md](CHANGES.md)**。

> **非官方 / Experimental**：这是社区兼容层，不是 Reeden 或 Kobo 官方产品，也不代表官方支持或背书。运行时会接管屏幕、触摸、电源与网络，并以 root 权限运行。开始前请备份书籍和笔记；不能保证断电、设备损坏或所有配置都可恢复。

> **合规说明**：本仓库**不包含任何 Reeden 官方闭源二进制**，也不包含 Debian 发行版运行库。官方核心 `Reeden-kindle.zip` 由你自己在本地提供（原字节不改），安装脚本结合 Debian 官方镜像完成组包。MIT 仅覆盖新写的适配层代码；官方核心、Flutter 引擎、cJSON、字体与运行库各自保留原许可。

---

## 一、这是什么

在 **Kobo Libra Colour（产品码 390）** 上，原字节运行官方 **Reeden Kindle `1.43.2+791`** 核心。上游原本只支持 Clara Colour（393），两者屏幕尺寸、触摸变换、输入节点布局和陀螺仪行为都不同，因此做了最小范围的适配。

| | 上游 v0.7.8 | 本适配版 |
| --- | --- | --- |
| 实测设备 | Clara Colour `393` | **Libra Colour `390`**（固件 `4.46.23836`） |
| 屏幕 | 1072×1448 | **1264×1680，32bpp，stride 5056** |
| 实体翻页键 | — | **支持**（`KEY_F23` / `KEY_F24`，方向按实机校正） |
| 自动旋转 | — | **支持**（竖屏 0° / 180°，纯软件实现） |
| 官方核心 | `1.43.2+791` / `1.43.1+780` | 相同，**原字节不改** |
| 安装方式 | 一键安装脚本 / `install.py` | 相同 |

**只在 390 上做过实机验证。** 393 仍保留在机型白名单里，但本适配版的翻页键、旋转与按键方向未在该机型上验证；其他机型一律拒绝安装和启动。

---

## 二、功能

### 2.1 继承自上游

- 以私有 glibc loader 原字节加载官方 Flutter 核心，不修改任何官方文件；
- eInk 分区刷新、彩色/灰度波形、夜间模式、电源键睡眠与唤醒、Wi-Fi 桥接；
- 与 Nickel 的显示/输入交接、软恢复 `recover.sh`、会话守护、断电恢复；
- NickelMenu 集成（启动 / 完整性检查 / 返回主界面）；
- 安装前预检、485 文件 SHA-256 回读校验、升级保留旧副本与阅读数据、一键卸载；
- **v0.7.8 新增**：每次启动重新核对完整性，缺失的 `core/data/` / `ui-assets/` 资源可从本地生成并已校验的归档里逐项恢复；自检显示实际版本并列出具体失败文件。文件内容被改动、恢复归档损坏或程序文件缺失仍会拒绝启动。本适配原样保留这套机制，未改动其判断逻辑；
- 退出手势、SSH 诊断工具（默认不开启）等，详见上游 [README](https://github.com/SSNQ/reeden-kobo#readme) 与 `docs/`。

### 2.2 本适配新增

**实体翻页键**

Libra Colour 的四个握持键通过 `gpio-keys`（`event0`）上报 `KEY_F23(193)` / `KEY_F24(194)`。程序把按键翻译成页边距合成点击：

| 键码 | 动作 | 点击位置 |
| --- | --- | --- |
| `KEY_F23` (193) | 下一页 | 右侧页边距（约 92% 宽，垂直居中） |
| `KEY_F24` (194) | 上一页 | 左侧页边距（约 8% 宽，垂直居中） |

> 这个方向与 KOReader 的 `RPgBack/RPgFwd` 命名相反，是在 390 实机上逐次按键验证后定下来的。旋转 180° 时合成点击的横坐标会同步镜像，按键与画面始终一致。

按键设备动态扫描、**不做 `EVIOCGRAB`**，与电源键、姿态事件共用一个 fd，不会影响系统的睡眠/唤醒。

**自动旋转**

- 主路径读 `ntx_io` 发到 `event0` 的 `EV_MSC/MSC_RAW` 合成姿态：`0x18` = 竖屏正立 → 0°，`0x17` = 竖屏倒置 → 180°，400 ms 去抖；
- 若 4 秒内收不到合成事件（结束 Nickel 后驱动可能静默），自动打开 `event2`（`kx122-accel`）读 `ABS_X/Y/Z` 作后备，竖持稳定约 1.5 秒后翻转；合成事件恢复后自动切回；
- 画面在 `present()` 里做 180° 像素旋转，触摸坐标在 `send_pointer()` 里同步做 `x=w-1-x, y=h-1-y`，两者永远对齐；
- **不写内核 fb rotate**（不调用 `FBIOPUT_VSCREENINFO`，不改 `/sys/class/graphics/fb0/rotate`），避免崩溃后留下污染状态导致触摸全反。仅在启动时把 rotation 归一化为 `3`；
- 390 的陀螺仪芯片相对面板装了 180°，因此默认启用 `REEDEN_ROTATE_FLIP=1` 对调姿态语义；
- 只处理竖屏 0° / 180°，横屏与平放状态保持原方向。

**机型与显示适配**

机型白名单 `390` / `393`；`fb-guard` 增加 Libra 的 `1264×1680 / stride 5056` 几何检查；触摸仍用上游的 `swap + mirrorX` 变换。

**启动入口加固**

USB 写入后 Kobo 的 FAT 缓存可能把已执行过的 `run.sh` 回滚（甚至截断为 0 字节），导致点菜单毫无反应。安装器现在会额外生成一份字节相同的 `start.sh`，NickelMenu 指向这个全新文件名的入口；`run.sh` 同时保留为兜底。

---

## 三、使用说明

### 3.1 前提

| 项目 | 要求 |
| --- | --- |
| 设备 | Kobo Libra Colour（产品码 `390`），实测固件 `4.46.23836` |
| 插件 | 已安装 [NickelMenu](https://pgaskin.net/NickelMenu/)（存在 `/mnt/onboard/.adds/nm/`） |
| 官方核心 | 自行从 Reeden 官网下载 `Reeden-kindle.zip`（`1.43.2+791`，原字节不改） |
| 电脑 | macOS / Windows / Linux，**Python 3.10+**（仅标准库；安装无需 Zig 或 C 编译器） |
| 空间 | 升级会保留旧副本，请留足空间 |

### 3.2 安装

- **一键安装**：连接 Kobo USB，双击对应系统的「一键安装」脚本，按提示选择官方核心压缩包。
- **命令行**：

  ```sh
  python3 install.py \
      --official-core /path/to/reeden/reeden \
      --kobo-mount /Volumes/KOBOeReader \
      --upgrade
  ```

- 安装完成后**整盘弹出**（macOS 用 `diskutil eject`，不是 `unmount`），Kobo 才会收到断开通知回到主界面。

> **⚠️ 每次 USB 写入后，必须长按电源键「完全关机 → 再开机」，不是睡眠唤醒。**
> 安装器升级会重命名目录，而 Kobo 在 USB 大容量存储之后仍缓存旧目录项；只弹出不重启，设备可能继续运行升级前的旧程序，日志也会写进旧目录。这一条是多次「功能没生效 / 点不进去」的根因，务必照做。

### 3.3 启动、退出与恢复

| 操作 | 方法 |
| --- | --- |
| 启动 | 原生主界面 → 右下角 NickelMenu → **Reeden Kobo (test)** |
| 正常退出 | 同时按住屏幕左上角与右上角约 2 秒 |
| 软件恢复 | NickelMenu → **Reeden Kobo - Return Home**；有 SSH 时可执行 `sh /mnt/onboard/.adds/reeden-kobo/recover.sh` |
| 完整性检查 | NickelMenu → **Reeden Kobo - Check**（v0.7.8 起显示实际安装版本与具体失败文件） |
| 完全无响应 | 长按电源键强制关机/开机，必要时先接 USB 充电 |
| 卸载 | 先退出 Reeden → 连接 USB → 双击「一键卸载」脚本（默认保留完整备份） |

### 3.4 翻页与旋转

- 打开一本书，按两侧握持键即可前后翻页；
- 把机器倒过来握稳 1–2 秒，画面与触摸会一起翻正，翻页键方向同步保持正确；
- 功能开关（写在 `start.sh` / `run.sh` 的 `export` 区，改完需重新开机）：

| 变量 | 默认 | 说明 |
| --- | --- | --- |
| `REEDEN_BUTTONS` | `1` | `0` 关闭实体翻页键 |
| `REEDEN_BTN_PREV` / `REEDEN_BTN_NEXT` | `194` / `193` | 覆盖键码，十进制或 `0x` 十六进制，逗号分隔多个 |
| `REEDEN_PAGE_TAP` | `1` | `0` 关闭合成页边距点击 |
| `REEDEN_BTN_DEBUG` | 未设 | 设置后打印每次按键的键码 |
| `REEDEN_ROTATE_AUTO` | `1` | `0` 关闭自动旋转 |
| `REEDEN_ROTATE_FLIP` | 390 上 `1` | 对调姿态语义。**存在即生效**，设 `=0` 无效，要删除该行 |
| `REEDEN_ROTATE_HOLD_MS` | `1500` | 加速度计后备路径的稳定时长 |
| `REEDEN_ROTATE_DEBOUNCE_MS` | `400` | MSC_RAW 去抖时长 |
| `REEDEN_ACCEL_DEBUG` | 未设 | 设置后打印姿态样本 |

### 3.5 日志与排障

所有日志在 `.adds/reeden-kobo/logs/`：

| 文件 | 内容 |
| --- | --- |
| `launcher.log` | 启动流程、机型、framebuffer、交接与恢复 |
| `current.log` / `previous.log` | 宿主运行日志（按键、旋转、刷新） |
| `input-devices.log` | 每次启动的 `/proc/bus/input/devices` 快照与 fb rotate |
| `install-verification.log` | v0.7.8 起每次启动的完整性复核结果 |
| `guardian.log` / `power.log` | 守护与电源状态 |

| 现象 | 先查 |
| --- | --- |
| 点菜单完全没反应 | `run.sh` / `start.sh` 是否被 FAT 缓存回滚成空文件；用 `sha256sum -c SHA256SUMS` 核对 |
| 升级后功能没变化 | 是否做了「完全关机 → 开机」；日志是否写进了 `reeden-kobo-backup-*` 旧目录 |
| 提示完整性校验失败 | `install-verification.log` 里列出的具体文件；v0.7.8 会自动恢复缺失资源，程序文件缺失仍需重装 |
| 翻页键没反应 | `current.log` 里有没有 `[buttons] key code=...` 与 `turn=next/prev` |
| 自动旋转没反应 | `[orient] gyro reports portrait ...`、`[orient] rotation=...` |
| 触摸与画面上下左右相反 | `input-devices.log` 里 fb rotate 是否被归一化为 `3` |
| 会话卡住进不去 | `state/active`、`state/asleep` 残留；用 **Return Home** 或删除这些状态文件后重启 |

> 排障时请保留日志再插 USB —— 设备上的一手记录比任何猜测都准。

---

## 四、已知限制

- 只在 **Libra Colour 390 / 固件 4.46.23836** 上实机验证；393 保留了白名单但本适配路径未验证，其他机型明确拒绝。
- 自动旋转只做竖屏 0° / 180°，不处理横屏；依赖 `ntx_io` 合成事件，驱动静默时改用原始加速度计（灵敏度较低）。
- v0.7.8 的完整性复核会在**每次启动**核对全部安装文件，启动比 v0.7.7 略慢；资源恢复只补回缺失文件，不覆盖已有内容。
- 文字残影、夜间彩色刷新、动画等沿用上游状态，未在本轮重新验证。
- 需要 root 并接管硬件；升级、断电、强制关机都可能影响阅读数据，请自行备份。
- 这不是对上游的改进建议，也未向上游提交合并；如原作者认为不妥，我们愿意按其意见调整或撤下。

---

## 五、许可与来源

- 适配层代码沿用上游 **MIT**（`LICENSE`，`Copyright (c) 2026 Reeden Kobo port contributors`），版权声明与许可全文已完整保留。
- 官方 Reeden 核心、Flutter 引擎、cJSON、字体、Mozilla CA、Debian 运行库**各自保留原许可**，且大多不在本仓库分发。
- 逐条致谢与引用：**[NOTICE.md](NOTICE.md)**；相对上游的改动：**[CHANGES.md](CHANGES.md)**。
- 上游原仓库：<https://github.com/SSNQ/reeden-kobo>　上游 v0.7.8 发布页：<https://github.com/SSNQ/reeden-kobo/releases/tag/v0.7.8>
