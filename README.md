# unit60 ZMK Firmware (ZMKS Branch)

基于 [ph-design/zmks](https://github.com/ph-design/zmks) 分支的 unit60 键盘固件配置。

## 硬件规格

| 项目 | 规格 |
|------|------|
| MCU | nRF52840 (E73-2G4M08S1C 模块) |
| 布局 | 65% 62 键 |
| 无线 | 蓝牙 5.0 BLE |
| 有线 | USB-C |
| 电池管理 | BQ24075 充电芯片 |
| 电量计 | MAX17048 (I2C) |
| 晶振 | 外部 32.768KHz |
| RGB | WS2812 单颗 (经 SN74LV1T125 缓冲, P0.20) |
| 休眠 | 30 分钟无操作自动休眠 |

## 功能特性

### 已启用
- ✅ 62 键 65% 布局（含独立方向键区）
- ✅ 蓝牙双模（USB + BLE）
- ✅ ZMK Studio 支持（运行时键位编辑）
- ✅ WS2812 单颗全状态指示（11 级优先级）
- ✅ BQ24075 充电状态检测（充电中/充满/放电三态）
- ✅ MAX17048 电量监测
- ✅ DC/DC 功耗优化（REG0 高压 + REG1 内核）
- ✅ 外部 32.768KHz 晶振
- ✅ 组合键（Combos，需在 keymap 中手动定义）

### 已禁用
- ❌ EC11 编码器
- ❌ 蜂鸣器
- ❌ 三色 LED（已合并到 WS2812 单颗）

## 键位说明

### 基础层（Base）

| 行 | 键位（从左到右） |
|----|-----------------|
| 第1行（15键） | `Esc` `1` `2` `3` `4` `5` `6` `7` `8` `9` `0` `-` `=` `⌫ Backspace` `⌦ Delete` |
| 第2行（14键） | `Tab` `Q` `W` `E` `R` `T` `Y` `U` `I` `O` `P` `\` `[` `]` |
| 第3行（13键） | `Caps Lock` `A` `S` `D` `F` `G` `H` `J` `K` `L` `Enter` `;` `'` |
| 第4行（13键） | `LShift` `Z` `X` `C` `V` `B` `N` `M` `/` `RShift` `,` `.` `↑` |
| 第5行（7键） | `LCtrl` `LAlt` `Space` `FN` `←` `↓` `→` |

### F1 层（Function 1，按住 FN 触发）

| 行 | 键位（从左到右） |
|----|-----------------|
| 第1行（15键） | `sys_reset` `F1` `F2` `F3` `F4` `F5` `F6` `F7` `F8` `F9` `F10` `F11` `F12` `-` `Delete` |
| 第2行（14键） | `BT_CLR` `-` `↑` `BT0` `BT1` `BT2` `BT3` `BT4` `-` `-` `-` `-` `-` `-` |
| 第3行（13键） | `bootloader` `←` `↓` `→` `-` `-` `-` `-` `-` `-` `-` `-` `-` |
| 第4行（13键） | `studio_unlock` `-` `-` `-` `-` `-` `-` `-` `-` `-` `-` `-` `↑` |
| 第5行（7键） | `-` `-` `-` `-` `-` `-` `-` |

**F1 层特殊键说明：**
- `sys_reset`：软重启键盘
- `BT_CLR`：清除所有蓝牙绑定
- `BT0`~`BT4`：切换蓝牙配置文件 0-4
- `bootloader`：进入 UF2 刷机模式
- `studio_unlock`：解锁 ZMK Studio 配置权限

> F2、F3 层目前为全透明（`&trans`），预留未使用。

## WS2812 灯光指示

### 一、插线提示（charger 控制，仅插线后 3 秒）

| 场景 | 灯光 | 频率 | 持续时间 | 之后 |
|------|------|------|---------|------|
| 插线（正在充电，CHG=低） | 🟠 橙色慢闪 | 1Hz | 3 秒 | 熄灭，charger 停写，widget 接管 |
| 插线（已充满，CHG=高） | 🟢 绿色常亮 | - | 3 秒 | 熄灭，charger 停写，widget 接管 |

**设计原则：**
- charger 只在插线后的 3 秒内写 LED，之后完全停止
- charger 和 widget 永远不会同时写 LED，避免冲突闪烁
- 3 秒后所有状态（低电量/USB连接/FN层/Caps/蓝牙等）完全由 widget 控制

**时间线（充电中）：**
```
0s ────────────────── 3s
│                      │
│  🟠 橙色慢闪 (1Hz)   │  ⚫ 熄灭 → widget 接管
│  (充电指示)           │
└──────────────────────┘
```

**时间线（充满）：**
```
0s ────────────────── 3s
│                      │
│  🟢 绿色常亮          │  ⚫ 熄灭 → widget 接管
│  (充满/USB连接)       │
└──────────────────────┘
```

---

### 二、电量与其他状态（widget 自动控制，3 秒后生效）

插线提示结束后，所有状态完全由 `zmk-rgbled-widget` 自动控制：

| 状态 | 颜色 | 效果 | 说明 |
|------|------|------|------|
| 电量 >80% | 🟢 绿色 | 常亮（背景） | 高电量 |
| 电量 20-80% | 🟡 黄色 | 常亮（背景） | 中电量 |
| 电量 <20% | 🔴 红色 | 慢闪 1Hz | 低电量警告 |
| 上电启动 | 🟢 绿色 | 呼吸 1 次 | 约 1 秒 |
| 蓝牙配对中 | 🔵 蓝色 | 快闪 4Hz | 持续 |
| FN 层激活 | 🟡 黄色 | 常亮 | 持续（FN层激活期间） |
| 蓝牙已连接 | 🔵 蓝色 | 常亮 3 秒 | 连接提示 |
| 蓝牙未连接 | 🔵 蓝色 | 慢闪 1Hz | 持续 |
| Caps Lock 开启 | ⚪ 白色 | 常亮 | 持续（Caps开启期间） |
| 休眠 | ⚫ 熄灭 | - | 持续 |

**优先级（widget 内部）：** 上电 > 蓝牙配对 > FN层 > 低电量 > 蓝牙连接 > 蓝牙未连接 > Caps > 电量颜色 > 休眠

---

### 四、颜色定义

| 颜色 | 含义 |
|------|------|
| 🟠 橙色 | 充电中（仅插线提示） |
| 🟢 绿色 | 充满 / 上电 / 高电量 |
| 🔴 红色 | 低电量警告（<20%） |
| 🟡 黄色 | FN 功能层激活 |
| 🔵 蓝色 | 蓝牙相关（配对/连接/未连接） |
| ⚪ 白色 | Caps Lock |
| ⚫ 熄灭 | 休眠 / 无状态 |

---

### 五、关键规则

1. **插线提示 > 低电量 > Widget**：高优先级覆盖低优先级
2. **充满且插线 = 绿色持续常亮**：只要 USB 插着且电池充满，绿灯一直亮
3. **充电中插线 = 橙色 3 秒 → 绿色闪烁 3 秒 → 熄灭**：之后归还 widget
4. **电量 ≥20% 不显示电量颜色**：平时由 widget 控制其他状态
5. **充电过程中（6 秒后）不持续亮灯**：只有插线瞬间的 6 秒序列提示
6. **拔线时**：立即停止所有提示，然后检测低电量
7. **widget 的 USB 连接绿色 3 秒**：可能与插线提示重叠，但插线提示优先级更高

## 文件结构

```
zmk-unit60-zmks/
├── .github/workflows/build.yml    # GitHub Actions 构建配置
├── .gitignore
├── build.yaml                      # 构建矩阵（unit60@rev_e）
├── README.md                       # 本文件
└── config/
    ├── west.yml                    # ZMK 源码引用（ph-design/zmks main）
    ├── dts/bindings/
    │   ├── behaviors/              # WS2812 widget 行为绑定
    │   └── buzzer/                 # 蜂鸣器绑定（保留未使用）
    └── boards/Eason/unit60/
        ├── board.cmake
        ├── board.yml
        ├── CMakeLists.txt          # 编译配置（含本地 widget）
        ├── Kconfig.defconfig
        ├── Kconfig.unit60
        ├── pre_dt_board.cmake
        ├── revision.cmake
        ├── unit60.dts              # 板卡设备树（DC/DC 配置）
        ├── unit60.zmk.yml          # 板卡元数据
        ├── unit60_charger.c        # BQ24075 充电检测（自定义模块）
        ├── unit60_rev_e.overlay    # rev_e 覆盖（矩阵/引脚/外设）
        ├── unit60_rev_e.keymap     # 键位映射（62 键 + 组合键）
        ├── unit60_rev_e-layouts.dtsi  # 物理布局坐标
        ├── unit60_rev_e-transforms.dtsi # 矩阵映射
        ├── unit60_rev_e-pinctrl.dtsi   # 引脚控制
        ├── unit60_rev_e_defconfig      # Kconfig 配置
        └── rgbled_widget/           # 本地 widget 源码（已适配 zmks API）
            ├── Kconfig
            ├── include/zmk_rgbled_widget/widget.h
            ├── src/widget.c
            └── dts/bindings/behaviors/
```

## 自定义模块说明

### 1. unit60_charger.c（BQ24075 充电检测）

- 读取 P1.13（BQ24075 CHG，开漏低有效，内部上拉）
- VBUS 检测使用 nRF52840 硬件寄存器 `USBREGSTATUS`（不依赖 Zephyr USB API）
- 三态逻辑：充电中红常亮 / 充满绿 3 秒 / 放电不覆盖 widget

### 2. rgbled_widget（本地源码，已适配 zmks API）

基于 [hitsmaxft/zmk-rgbled-widget](https://github.com/hitsmaxft/zmk-rgbled-widget) fork，本地化并适配 ph-design/zmks 分支：

- `zmk_endpoint_get_selected()` → `zmk_endpoints_selected()`（2 处）
- `<zmk/battery.h>` → 手动声明 `zmk_battery_state_of_charge()`
- 添加 `zmk/app/include` 到 include 路径（zmks 分支默认不含）
- 不编译 `behavior_rgbled_widget.c`（未使用且 API 不兼容）

## 编译和刷入

### GitHub Actions 编译

1. 将本目录推送到 GitHub 仓库
2. Actions 自动触发构建
3. 构建完成后下载 artifact（zip）
4. 解压得到 `unit60_rev_e-zmk.uf2`

### 刷入固件

1. 键盘进入 bootloader 模式（连按两下 reset，或 FN+bootloader 键）
2. 电脑出现 U 盘
3. 将 UF2 文件拖入 U 盘
4. 自动重启完成刷入

### ZMK Studio 连接

1. 用 USB 线连接键盘
2. 打开 [zmks.phdesign.cc](https://zmks.phdesign.cc) 或 ZMK Studio 桌面端
3. 选择串口连接
4. 输入解锁码（默认无锁，直接连接）

## 引脚分配

### 矩阵（7 列 × 10 行，col2row）

| 列 | 引脚 | 行 | 引脚 |
|----|------|----|------|
| COL0 | P1.10 | ROW0 | P0.02 |
| COL1 | P0.03 | ROW1 | P0.29 |
| COL2 | P0.04 | ROW2 | P0.31 |
| COL3 | P0.28 | ROW3 | P1.00 |
| COL4 | P1.06 | ROW4 | P0.26 |
| COL5 | P0.10 | ROW5 | P0.22 |
| COL6 | P1.02 | ROW6 | P0.08 |
|      |      | ROW7 | P0.09 |
|      |      | ROW8 | P1.04 |
|      |      | ROW9 | P1.11 |

### 其他引脚

| 功能 | 引脚 | 说明 |
|------|------|------|
| WS2812 | P0.20 | SPI3 MOSI，经 SN74LV1T125 缓冲 |
| MAX17048 SDA | P0.05 | I2C0 |
| MAX17048 SCL | P1.09 | I2C0 |
| BQ24075 CHG | P1.13 | 充电状态，开漏低有效 |
| BQ24075 PGOOD | P0.06 | 电源好，预留未用 |
| EXT_POWER | P0.30 | WS2812 电源控制 |

## 版权

Copyright (c) 2025 Eason
