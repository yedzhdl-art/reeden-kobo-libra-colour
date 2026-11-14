# 相对上游 v0.7.7 的改动

基线：[SSNQ/reeden-kobo](https://github.com/SSNQ/reeden-kobo) `v0.7.7`，commit `a6f2c9d5b8d3f3fa6782fd68658c3e0964228dc6`。

统计：**新增 4 个源文件（414 行）**，**修改 14 个文件**，重新编译 `prebuilt/` 下的适配层二进制并更新校验清单。其余文件（含 `docs/`、`tests/` 的大部分、刷新与电源子系统）与上游一致。

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
| `scripts/run.sh` | 日志重定向前置；每次启动写 `logs/input-devices.log`（`/proc/bus/input/devices` + fb rotate）；导出 `REEDEN_BUTTONS` / `REEDEN_ROTATE_AUTO`；390 启动时归一化 `fb-guard --rotate 3`；390 导出 `REEDEN_ROTATE_FLIP=1` |
| `scripts/nickelmenu.conf` | 启动项指向 `start.sh`（见下） |
| `install.py` / `scripts/assemble.py` | 额外生成一份与 `run.sh` 字节相同的 `start.sh`，绕开 Kobo 对已执行脚本的 FAT 缓存回滚；安装完成提示强制写明「必须彻底关机→开机」 |
| `scripts/build.sh` | 编译源加入 `buttons.c` / `orientation.c` |
| `install.py` / `uninstall.py` / `scripts/install.sh` / `scripts/precheck.sh` / `scripts/wifi.sh` | 机型白名单 `390` / `393` 与对应提示文案 |
| `tests/uninstall_test.py` | 「未知机型应被拒绝」的断言改用 `999`（原用例用 `390`，与本适配冲突） |

## 重新生成的产物

- `prebuilt/bin/reeden-kindle`（`4f9a077b…`）、`prebuilt/bin/fb-guard`、`prebuilt/lib/libreeden_kobo_compat.so`、`prebuilt/checks/*-test-armhf`：由 Zig 0.13.0 交叉编译（ARMHF / cortex_a7）。
- `prebuilt/PREBUILT_SHA256SUMS`：相应重算。
- `prebuilt/fonts/KoboCJKMath-Regular.ttf`：与上游相同，未修改。

## 未在源码仓库中、但实际设备安装时存在的文件

`start.sh` 由安装器在组包阶段生成（内容等同 `run.sh`）；设备上的 `SHA256SUMS` 会包含它。这是为了规避「USB 写入后 Kobo 把 `run.sh` 回滚成 0 字节，导致菜单点了没反应」的问题，详见 README 第三节。

## 验证情况

- 55 项 Python 回归测试通过；
- 480 个安装文件 SHA-256 回读校验通过；
- 实机（Libra Colour 390 / 固件 4.46.23836）验证：触摸、显示、电源键睡眠/唤醒、两侧握持键翻页、倒持 180° 自动旋转且画面与点击同步；
- 未做：393 上的适配路径、横屏旋转、长时间续航与残影专项测试。
