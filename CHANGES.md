# 相对上游 v0.7.8 的改动

基线：[SSNQ/reeden-kobo](https://github.com/SSNQ/reeden-kobo) `v0.7.8`，tag 对象 `2a4024be04b3a5cceac0c355e15cac587a767570`，commit `677cc9f1b1e92df7a50f42e1d5ea951dd66b5946`（2026-11-05）。

统计：**新增 4 个源文件（414 行）**，**修改 15 个文件**，重新编译 `prebuilt/` 下的适配层二进制并更新校验清单。其余文件（含 `docs/`、`tests/` 的大部分、刷新与电源子系统、v0.7.8 新增的完整性复核）与上游一致。

> 相对上一版适配（基于 v0.7.7）：上游 v0.7.8 的 13 处变更已通过三方合并（共同祖先 v0.7.7）完整并入，只在 `install.py` 与 `scripts/assemble.py` 的版本号/机型文案两处发生重叠冲突并手工合并；`src/` 上游一个字节未动，因此三个编译产物与本适配 v0.7.7 版**字节完全一致**，翻页键与旋转的实现原样保留。

## 新增

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `src/buttons.h` / `src/buttons.c` | 28 / 211 | 动态扫描按键节点、不 `grab`、复用宿主 fd；`KEY_F23 → 下一页`、`KEY_F24 → 上一页`；合成页边距点击；支持 `REEDEN_BTN_PREV` / `REEDEN_BTN_NEXT` 覆盖与调试日志 |
| `src/orientation.h` / `src/orientation.c` | 34 / 141 | `EV_MSC/MSC_RAW` 姿态主路径（`0x18` → 0°、`0x17` → 180°，400 ms 去抖）+ `ABS_X/Y/Z` 后备路径（竖持稳定 1.5 s）；`REEDEN_ROTATE_FLIP` 对调语义；纯软件旋转，不写内核 fb rotate |

## 修改

| 文件 | 改动 |
| --- | --- |
| `src/host.c` | 主要改动处。`rotate180_frame()` + `present()` 内 180° 像素旋转；`send_tap()` / `on_page_turn()` 合成翻页点击（旋转时镜像 x）；`find_keys_device()` 按「翻页键优先」评分选 `event0`、`find_power_device()` 单独读 `event3`、`find_accel_device()` 后备；poll 扩到 5 个 fd；MSC_RAW 静默 4 s 后切原始加速度计、恢复后切回；启动时导出输入节点快照；`screen_rotation` 变化时触发整帧刷新 |
| `src/input.c` | 新增全局 `screen_rotation`，`send_pointer()` 在 `180` 时做 `x=w-1-x, y=h-1-y`，与画面旋转同步 |
| `src/kobo.h` | 声明 `extern int screen_rotation` |
| `src/fb_guard.c` | 新增幂等 `--rotate N`（`FBIOPUT_VSCREENINFO`）；几何检查增加 Libra 的 `1264×1680 / stride 5056` 分支 |
| `scripts/run.sh` | 日志重定向前置；每次启动写 `logs/input-devices.log`（`/proc/bus/input/devices` + fb rotate）；导出 `REEDEN_BUTTONS` / `REEDEN_ROTATE_AUTO`；390 启动时归一化 `fb-guard --rotate 3`；390 导出 `REEDEN_ROTATE_FLIP=1`。**上游 0.7.8 的「每次启动经 `integrity.sh repair` 复核全部文件」逻辑原样保留在我们的日志前置之后** |
| `scripts/nickelmenu.conf` | 启动项指向 `start.sh`（见下） |
| `install.py` / `scripts/assemble.py` | 额外生成一份与 `run.sh` 字节相同的 `start.sh`，绕开 Kobo 对已执行脚本的 FAT 缓存回滚；安装完成提示强制写明「必须彻底关机→开机」。**上游 0.7.8 的 `create_resource_recovery()`、脚本清单里的 `integrity.sh`、`os.sync()` 与 `port_version = 0.7.8-experimental` 均保留**；`build-info.json` 的 `port_version` 改为 0.7.8，`model` 改为 390 / 393 |
| `scripts/build.sh` | 编译源加入 `buttons.c` / `orientation.c` |
| `scripts/build-headless.sh` | 编译源同样加入 `buttons.c` / `orientation.c`（否则 headless 诊断主机会因 `buttons_probe()` / `orientation_*()` 未定义而链接失败）；与之配套，`src/host.c` 里只在硬件交接路径用到的 `find_keys_device()` / `find_power_device()` / `fd_reports_msc_raw()` 用 `#ifndef REEDEN_HEADLESS_DIAGNOSTIC` 包住，避免上游 CI 在 `-Werror` 下报 `unused function`。**此改动不改变正式产物**：三个预编译二进制的哈希逐字节不变 |
| `install.py` / `uninstall.py` / `scripts/install.sh` / `scripts/precheck.sh` / `scripts/wifi.sh` | 机型白名单 `390` / `393` 与对应提示文案（上游 0.7.8 在 `precheck.sh` 新增的「自检显示实际安装版本、列出具体失败文件」与 `install.sh` 的 `sync` 均保留） |
| `tests/uninstall_test.py` | 「未知机型应被拒绝」的断言改用 `999`（原用例用 `390`，与本适配冲突） |

## 上游 v0.7.8 原样保留的内容

以下由原作者新增，本适配只做合并、未改其判断逻辑：

- `scripts/integrity.sh` + 组包阶段生成的 `integrity/resources.tar.gz`：每次启动重新核对，仅补回缺失的 `core/data/` / `ui-assets/` 文件并验证原 SHA；已有文件内容变化、恢复归档损坏、程序文件缺失仍拒绝启动；不覆盖书库、笔记和设置，不重写 SHA 清单。
- `tests/resource_recovery_test.py`（17 项故障回归）、`docs/releases/v0.7.8.md`、`docs/资源缺失排查与恢复-20261105.md`。
- `install.py` 的 User-Agent 版本号、`scripts/precheck.sh` 的动态版本显示、`README.md` / `docs/安装与卸载.md` 的上游说明。

## 重新生成的产物

- `prebuilt/bin/reeden-kindle`（`4f9a077b…`）、`prebuilt/bin/fb-guard`（`6fc778c2…`）、`prebuilt/lib/libreeden_kobo_compat.so`（`9939c997…`）、`prebuilt/checks/*-test-armhf`：由 Zig 0.13.0 交叉编译（ARMHF / cortex_a7）。三个二进制与基于 v0.7.7 的适配版逐字节相同，因为上游 `src/` 未变。
- `prebuilt/PREBUILT_SHA256SUMS`：相应重算。
- `prebuilt/fonts/KoboCJKMath-Regular.ttf`：与上游相同，未修改。

## 未在源码仓库中、但实际设备安装时存在的文件

`start.sh` 与 `integrity/resources.tar.gz` 都由安装器在组包阶段生成（`start.sh` 内容等同 `run.sh`）；设备上的 `SHA256SUMS` 会包含它们。`start.sh` 是为了规避「USB 写入后 Kobo 把 `run.sh` 回滚成 0 字节，导致菜单点了没反应」的问题，详见 README 第三节。

## 验证情况

- **72 项** Python 回归测试通过（其中 17 项为上游 v0.7.8 新增的资源恢复故障回归）；
- 上游 CI 的 `verify` 步骤已在本地按同样顺序复现（不设 `REEDEN_OFFICIAL_CORE`、Zig 0.13.0）：`prebuilt` 清单校验、`build.sh`、`build-headless.sh` 全部通过，`cmp` 比对 3 个正式产物 + 7 个 ARMHF 检查程序逐字节一致；修复前 `build-headless.sh` 会因 3 个 `-Werror,-Wunused-function` 报错而中断；
- **485** 个安装文件 SHA-256 回读校验通过（v0.7.7 为 480）；
- 设备已完成 v0.7.8 升级：`build-info.json` 显示 `0.7.8-experimental`，`integrity.sh` / `integrity/resources.tar.gz` / `start.sh` 均就位，`run.sh` 同时含 0.7.8 完整性复核与 390 适配开关，全量 `sha256sum -c` 零失败，NickelMenu 中的 KOReader 项保留；
- 实机（Libra Colour 390 / 固件 4.46.23836）在 v0.7.7 适配版上已验证：触摸、显示、电源键睡眠/唤醒、两侧握持键翻页、倒持 180° 自动旋转且画面与点击同步，以及翻页方向与旋转方向校正。由于宿主二进制字节未变，这些行为在 v0.7.8 上应保持一致；**0.7.8 升级后的真机启动回归尚待完成**；
- 未做：393 上的适配路径、横屏旋转、长时间续航与残影专项测试。
