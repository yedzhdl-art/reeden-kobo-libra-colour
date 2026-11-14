# 致谢与引用说明

这份文档想认真说清楚一件事：**这个仓库里绝大部分东西不是我写的。**

我只是把 [SSNQ](https://github.com/SSNQ) 的 [reeden-kobo](https://github.com/SSNQ/reeden-kobo) v0.7.8 从 Kobo Clara Colour 搬到 Kobo Libra Colour 上。真正难的那些工作 —— 用 C 宿主接住 Flutter Embedder、把官方闭源核心原字节跑起来、eInk 的分区刷新与波形、电源与睡眠状态机、跟 Nickel 交接显示和输入、跨三套操作系统的一键安装器、485 文件哈希校验与每次启动的完整性复核、72 项回归测试、以及那份写得非常克制的合规声明 —— 全部是原作者已经做完并公开发布的。我新增的只有 4 个源文件，改动 15 个文件，做的事情本质上是「在另一台机器上把输入节点和屏幕尺寸对上」。

如果没有这个上游项目，就不会有这次适配。谢谢 SSNQ 把它开源出来，也谢谢他把许可边界写得那么清楚，让后来的人可以放心地改、也知道什么不该碰。

也特别想谢谢原作者在 v0.7.8 里对待问题的态度：针对「安装后少量文件校验失败、无法启动」的反馈，他没有把第三方环境的偶发现象含糊地宣布为「已修复」，而是在[发布说明](https://github.com/SSNQ/reeden-kobo/blob/v0.7.8/docs/releases/v0.7.8.md)里明确写出「本轮全新安装没有复现，故不将唯一根因宣称为已查明」，同时仍老老实实补上了受校验保护的资源恢复归档、每次启动的完整性复核和能列出具体失败文件的自检；并且清楚地标注了实机验证只覆盖 393 / 固件 4.46.23836，未测的固件不推断已覆盖。这种不夸大、不含糊的写法，对我们排查这台 390 上的一系列怪问题帮助很大 —— 我们后来所有「先看日志再说」的做法，某种意义上都是跟着这份说明学的。

---

## 一、直接基于：SSNQ/reeden-kobo v0.7.8

- 仓库：<https://github.com/SSNQ/reeden-kobo>
- 版本：`v0.7.8`，tag 对象 `2a4024be04b3a5cceac0c355e15cac587a767570`，对应 commit `677cc9f1b1e92df7a50f42e1d5ea951dd66b5946`（2026-11-05，*Document successful six-resource recovery and startup verification on Kobo*）
- 发布说明：[docs/releases/v0.7.8.md](https://github.com/SSNQ/reeden-kobo/blob/v0.7.8/docs/releases/v0.7.8.md)
- 许可：**MIT**，`Copyright (c) 2026 Reeden Kobo port contributors`，许可全文见本仓库 [`LICENSE`](LICENSE)，版权声明原样保留。

**直接沿用、未作改动的部分**（列举主要项，非穷尽）：`src/display.c` 与刷新策略、`src/power*.c`、`src/runtime*.c`、Flutter Embedder 接线、触摸协议 A/B 处理、`scripts/native-session.sh` / `power.sh` / `wifi.sh` 的主体、安装器整体架构与校验逻辑、`scripts/integrity.sh` 与 `tests/resource_recovery_test.py`（均为 v0.7.8 新增，原样保留）、`docs/` 里的兼容性记录与开发笔记、`tests/` 里绝大多数测试、一键安装/卸载脚本、以及 [README](https://github.com/SSNQ/reeden-kobo#readme) 中的合规与使用框架（本仓库说明文档的安装、卸载、排障章节在其基础上改写）。

本仓库的 `LICENSE` 文件末尾那句说明也来自上游，我们原样保留：

> *This license covers only the newly written adapter. Official Reeden binaries, Flutter headers, cJSON, fonts and runtime libraries retain their own licenses.*

**我们实际新增/修改的内容**见 [`CHANGES.md`](CHANGES.md)，主要是：`src/buttons.{c,h}`、`src/orientation.{c,h}`，以及对 `host.c` / `input.c` / `fb_guard.c` / 机型白名单 / 启动脚本的最小改动。上游 v0.7.8 相对 v0.7.7 的 13 处变更（资源恢复归档、每次启动完整性复核、自检显示实际版本、安装器 `os.sync()` 等）已完整合并进来，一处未丢。

---

## 二、官方与第三方组件（各自许可，多数不在本仓库分发）

| 组件 | 许可 / 归属 | 在本项目中的角色 |
| --- | --- | --- |
| **Reeden 官方 Kindle 核心 `1.43.2+791`** | Reeden 官方，闭源 | 运行时通过私有 loader **原字节加载**，未修改、未反编译、未随本仓库分发；由使用者自行从官网获取 |
| **Flutter Engine / Embedder API** | BSD-3-Clause，`Copyright 2013 The Flutter Authors` | 官方核心自带 `libflutter_engine.so`；`include/` 中的 embedder 头文件随上游提供 |
| **cJSON** | MIT | 上游适配层内部使用 |
| **Noto Sans CJK / KoboCJKMath 字体** | SIL Open Font License 1.1 | 上游打包于 `prebuilt/fonts/`，用于中文显示 |
| **Mozilla CA 根证书集合** | MPL-2.0 | 上游打包于 `certs/`，随安装包生成 |
| **Debian Bullseye armhf 运行库** | 各库原有许可 | 安装脚本从 Debian 官方镜像在本地组包，**不在本仓库分发** |
| **Zig 0.13.0** | MIT | 仅构建期用于 ARMHF 交叉编译，不随安装包分发；一键安装不需要 Zig |

上游的 [`docs/依赖安全维护.md`](docs/依赖安全维护.md) 说明了这些依赖的来源与维护边界，本适配未改变该策略（该文档随上游一并保留在本仓库）。

---

## 三、参考过的公开资料（仅参考，未复制代码）

适配过程中有几个关键判断，靠的是别人已经公开写下来的设备行为。这里逐条说明**借鉴了什么**，以便核查：

### 1. KOReader — <https://github.com/koreader/koreader>（AGPL-3.0）

- 参考了设备上 `frontend/device/kobo/device.lua` 的 `event_map`：Kobo 握持键 `193 = RPgBack`、`194 = RPgFwd`，据此确定从 `event0` 读取 `KEY_F23` / `KEY_F24`。
- 参考其 Kobo 机型定义，确认 Libra Colour 的翻页键与陀螺仪不在同一输入节点上。
- 参考 [koreader issue #12674](https://github.com/koreader/koreader/issues/12674) 中 NiLuJe 的说明：*`in the event0 input stream, EV_MSC:MSC_RAW:24 is UR and 23 is UD`*，与 NickelMenu 讨论里的取值互相印证。
- **没有复制任何 KOReader 代码**，只是读取公开的设备定义来确定键码与事件语义；KOReader 的 AGPL-3.0 义务不因本次参考而转移到本仓库。

> 顺带一提：最终 `KEY_F23 → 下一页` 的方向与 KOReader 的命名相反，这是在 390 实机上逐次按键试出来的，不是照抄映射表。这也说明参考不等于照搬。

### 2. NickelMenu — <https://github.com/pgaskin/NickelMenu>（MIT，`Copyright (c) 2020-2025 Patrick Gaskin`）

- 运行时依赖，由使用者自行安装，本仓库不分发其二进制。
- 菜单写法（`menu_item:main:...:cmd_spawn` / `cmd_output`、`cmd_output` 超时必须 `0 < ms < 10000`）参考其文档；这些约束让我们放弃过一次非法的 submenu 配置。
- **[issue #67「feature request - orientation toggle」](https://github.com/pgaskin/NickelMenu/issues/67)** 里 NiLuJe 于 2020-08-04 的评论，是整个自动旋转功能的钥匙：

  > *"For reference, on a device w/ a gyro, the gyro sends raw evdev events to a dedicated device (event2) but I don't think anyone consumes those in userspace. Instead, the ntx_io driver sends synthetic 'I've rotated!' events to event0."*

  同一条评论给出的常量表：`0x17 PORTRAIT_DOWN`、`0x18 PORTRAIT_UP`、`0x19 LANDSCAPE_RIGHT`、`0x1a LANDSCAPE_LEFT`、`0x1b BACK`、`0x1c FRONT`。

  正是这段话让我们明白：不该去 `event2`（`kx122-accel`）里等姿态，而应该在 `event0` 上听 `MSC_RAW`；`event2` 只作为合成事件静默时的后备。对应的实现见 [PR #70](https://github.com/pgaskin/NickelMenu/pull/70)（`nickel_orientation` action）。

### 3. 设备自身的一手记录

- `/proc/bus/input/devices` 快照（每次启动写入 `logs/input-devices.log`）：确认 390 上 `event0 = gpio-keys`（F23/F24 + MSC_RAW）、`event1 = Elan Touchscreen`、`event2 = kx122-accel`、`event3 = bd71828-pwrkey`。
- framebuffer `hwtcon 1264×1680 / 32bpp / stride 5056`、rotation 需在启动时归一化为 `3`，均来自实机日志。
- 多次 `logs/current.log` 中的 `[buttons] key code=...`、`[orient] gyro reports portrait ...`，用于校正按键方向与姿态语义。

这些是这台设备自己的数据，也是本适配与「纸上推测」之间唯一的差别。

---

## 四、未分发声明

本仓库**不包含**：

- Reeden 官方闭源核心（`Reeden-kindle.zip` 内的任何字节）；
- `libflutter_engine.so` 等官方引擎二进制；
- Debian 发行版运行库；
- NickelMenu、KOReader 的任何程序文件；
- 任何设备上的阅读数据、书籍、笔记或个人配置。

`prebuilt/` 里只有我们自己交叉编译出的适配层二进制（`reeden-kindle`、`fb-guard`、`libreeden_kobo_compat.so`）、上游的测试程序与开源字体，并附 `PREBUILT_SHA256SUMS`。

---

## 五、最后

这次适配是在一台真实的 Libra Colour 上，靠反复看日志、试按键、对比输入节点一点点调出来的；中间走过几次弯路（选错输入节点、把内核 fb rotate 写脏、USB 写入被 FAT 缓存回滚），能绕出来很大程度上是因为上游把日志和测试留得足够完整，也因为上面这些项目把设备行为公开写清楚了。

所以这份说明不想把它包装成一个「新项目」。它更像是给原作者那份工作补的一条机型备注。谢谢 SSNQ，也谢谢 Patrick Gaskin、NiLuJe 和 KOReader / NickelMenu / Flutter 社区的每一位贡献者。

若上游作者认为本仓库的名称、表述或分发方式有任何不妥，欢迎提出，我们会按其意见调整或撤下。
