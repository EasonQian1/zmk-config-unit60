# unit60 ZMK Firmware

基于 [zmkfirmware/zmk](https://github.com/zmkfirmware/zmk) 官方 main 分支的 unit60 键盘固件配置。

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
- ✅ 蓝牙双模（USB + BLE，5 通道切换）
- ✅ ZMK Studio 支持（运行时键位编辑）
- ✅ WS2812 单颗全状态指示（charger 插线提示 + widget 自动状态）
- ✅ BQ24075 充电状态检测（充电中/充满/放电三态）
- ✅ MAX17048 电量监测（硬件算法自动校准，无需手动配置映射）
- ✅ DC/DC 功耗优化（REG0 高压 + REG1 内核）
- ✅ 外部 32.768KHz 晶振
- ✅ 组合键（Combos，需在 keymap 中手动定义后编译）

### 已禁用
- ❌ EC11 编码器
- ❌ 蜂鸣器
- ❌ 三色 LED（已合并到 WS2812 单颗）
- ❌ widget USB 连接提示（由 charger 插线提示替代）
- ❌ 高/中电量背景颜色（仅低电量红色显示）

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

> 基于实际代码（unit60_charger.c + 本地化 rgbled_widget）整理。
> 单颗 WS2812，所有状态共享 LED 索引 0。

### 一、控制权划分

| 阶段 | 控制者 | 时间范围 |
|------|--------|---------|
| 插线提示 | **unit60_charger.c** | 插线后前 3 秒 |
| 其余所有状态 | **rgbled_widget** | 3 秒后 / 未插线时 |

> 核心原则：charger 和 widget **永远不会同时写 LED**，避免冲突闪烁。

---

### 二、插线提示（unit60_charger.c 控制，仅插线后 3 秒）

#### 触发条件
检测到 **USB 插入事件**（VBUS 从无到有），通过 nRF52840 硬件寄存器 `USBREGSTATUS` bit0 检测，不依赖任何 Zephyr USB API。

#### 两种状态

| 插线时电池状态 | CHG 引脚 (P1.13) | 灯光 | RGB 值 | 频率 | 持续 |
|---------------|-------------------|------|--------|------|------|
| **充电中** | 低（有效） | 🟠 橙色慢闪 | (255,140,0) | 1Hz（500ms亮/500ms灭） | 3 秒 |
| **已充满/只插线** | 高（无效） | 🟢 绿色常亮 | (0,200,0) | - | 3 秒 |

#### 时序常量
- `DURATION_3S = 30` ticks × 100ms = **3 秒**
- `BLINK_HALF_PERIOD = 5` ticks × 100ms = **500ms**
- 轮询周期：每 100ms 一次

#### 3 秒后 / 拔线
- `go_idle()`：熄灭 LED → 进入 `STATE_IDLE`
- **charger 完全停止写 LED**
- 所有控制权交给 widget（蓝牙/层/Caps/低电量等）

---

### 三、Widget 自动状态（3 秒后 / 未插线时）

#### 1. 上电启动流程（顺序执行）

```
上电 → 等待 200ms → 电池指示（2秒）→ 等待完成 → 连接状态 → 层颜色
```

| 步骤 | 动作 | 说明 |
|------|------|------|
| 1 | 等待 200ms | 系统稳定 |
| 2 | 指示电池状态 | 正常电量(>20%)：黑色不亮；低电量(<20%)：红色闪烁 |
| 3 | 等待 2.5 秒 | 等电池指示完成 |
| 4 | 指示连接状态 | 显示当前蓝牙/USB 连接状态 |
| 5 | 设置初始层颜色 | 显示当前激活层颜色 |

---

#### 2. 蓝牙连接状态（全部通道统一蓝色 🔵）

| 状态 | 灯光 | 动画类型 | 周期 | 持续时间 |
|------|------|---------|------|---------|
| **已连接** | 🔵 蓝色常亮 | ANIM_STATIC | - | 3 秒 |
| **搜索/配对中**（广告） | 🔵 蓝色呼吸脉冲 | ANIM_PULSE | 500ms | **30 秒** |
| **断开**（未广播） | 🔵 蓝色闪烁 | ANIM_BLINK | 500ms | 3 秒 |

> - 所有 5 个蓝牙通道（BT0-BT4）颜色都是蓝色（4），不再按通道区分
> - USB 连接提示已禁用（`CONN_SHOW_USB=n`），由 charger 插线提示替代
> - 切换通道（FN+1~5）后自动进入广告状态，呼吸 30 秒

---

#### 3. 层状态（FN 层黄色 🟡）

| 层 | 灯光 | 模式 |
|----|------|------|
| Base 层（层 0） | ⚫ 熄灭（颜色 0） | `SHOW_LAYER_COLORS` 常亮模式 |
| FN1 层（层 1） | 🟡 黄色常亮（颜色 3） | 层激活期间持续常亮 |
| FN2 层（层 2） | 🟡 黄色常亮（颜色 3） | 同上 |
| FN3 层（层 3） | 🟡 黄色常亮（颜色 3） | 同上 |

---

#### 4. 电量状态（仅低电量显示红色 🔴）

| 电量范围 | 灯光 | 颜色 |
|---------|------|------|
| >80%（高电量） | ⚫ 不显示（颜色 0） | 已禁用背景颜色 |
| 20-80%（中电量） | ⚫ 不显示（颜色 0） | 已禁用背景颜色 |
| <20%（低电量） | 🔴 红色（颜色 1） | 慢闪/常亮 |
| <5%（临界电量） | 🔴 红色（颜色 1） | 同上 |

> 高/中电量背景颜色已设为黑色（0），平时不显示电量颜色，避免干扰其他状态指示。

---

#### 5. Caps Lock（白色 ⚪）

| 状态 | 灯光 |
|------|------|
| Caps Lock 开启 | ⚪ 白色常亮（颜色 7） |
| Caps Lock 关闭 | ⚫ 熄灭 |

---

#### 6. 休眠

| 状态 | 灯光 |
|------|------|
| 30 分钟无操作进入休眠 | ⚫ 熄灭 |
| 按键唤醒 | 恢复对应状态指示 |

---

### 四、优先级（从高到低）

```
1. 插线提示（charger，前 3 秒，直接覆盖 widget）
2. Caps Lock（widget，PRIORITY_CAPSLOCK=1）
3. 蓝牙连接变化（widget，PRIORITY_CONNECTION_CHANGE=2）
4. 层变化（widget，PRIORITY_LAYER_CHANGE=3）
5. 低电量（widget，电池状态）
6. 休眠（熄灭）
```

---

### 五、颜色定义总览

| 颜色 | RGB / 索引 | 含义 | 控制者 |
|------|-----------|------|--------|
| 🟠 橙色 | (255,140,0) | 充电中（仅插线前 3 秒） | charger |
| 🟢 绿色 | (0,200,0) | 充满（仅插线前 3 秒） | charger |
| 🔴 红色 | 索引 1 | 低电量警告（<20%） | widget |
| 🟡 黄色 | 索引 3 | FN 功能层激活 | widget |
| 🔵 蓝色 | 索引 4 | 蓝牙相关（配对/连接/断开） | widget |
| ⚪ 白色 | 索引 7 | Caps Lock 开启 | widget |
| ⚫ 熄灭 | 索引 0 | 休眠 / 无状态 / 正常电量 | - |

---

### 六、关键规则

1. **charger 只在插线后 3 秒内写 LED**，之后完全停止，不再写
2. **charger 和 widget 永远不会同时写 LED**，避免冲突闪烁
3. **3 秒后所有状态完全由 widget 自动控制**（含低电量/FN层/Caps/蓝牙等）
4. **所有状态共享单颗 LED**（`BATTERY/CONN/CAPSLOCK/LAYER_LED_INDEX=0`）
5. **电量 ≥20% 不显示电量背景颜色**（高/中电量颜色设为黑色）
6. **所有蓝牙通道统一蓝色**（BT0-BT4 颜色均为 4），不再按通道区分
7. **USB 连接提示已禁用**（`CONN_SHOW_USB=n`），由 charger 插线提示替代
8. **不插电池只插 USB**：CHG=高 → 绿色常亮 3 秒 → widget 接管

---

### 七、关键配置确认

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `LED_COUNT` | 1 | 单颗 LED |
| `BATTERY_LED_INDEX` | 0 | 电量写到第 1 颗 |
| `CONN_LED_INDEX` | 0 | 蓝牙写到第 1 颗 |
| `CAPSLOCK_LED_INDEX` | 0 | Caps 写到第 1 颗 |
| `LAYER_LED_INDEX` | 0 | 层写到第 1 颗 |
| `LED_SHARING` | y | 所有状态共享同一颗 LED |
| `BRIGHTNESS` | 128 | 亮度 50% |
| `ANIMATIONS` | y | 启用动画（呼吸/闪烁） |
| `SPATIAL_MAPPING` | y | 启用空间映射（让 LED_INDEX 配置可见） |
| `CONN_SHOW_USB` | n | 禁用 widget USB 连接提示（由 charger 替代） |
| `CONN_ADV_DURATION_MS` | 30000 | 蓝牙广告/配对呼吸 30 秒 |
| `CONN_CONNECTED_DURATION_MS` | 3000 | 蓝牙已连接常亮 3 秒 |
| `CONN_DISCONNECTED_DURATION_MS` | 3000 | 蓝牙断开闪烁 3 秒 |

---

### 八、典型场景示例

#### 场景 1：正常使用（蓝牙已连接）
```
上电 → 电池指示（不亮，电量正常）→ 蓝牙已连接（蓝色常亮3秒）→ 熄灭
→ 按 FN → 黄色常亮 → 松 FN → 熄灭
→ 按 Caps → 白色常亮 → 再按 Caps → 熄灭
→ 30分钟无操作 → 休眠熄灭
```

#### 场景 2：切换蓝牙通道配对
```
按 FN+2 → 切换到通道2 → 蓝色呼吸脉冲（30秒）
→ 配对成功 → 蓝色常亮（3秒）→ 熄灭
→ 配对失败/超时 → 30秒后自动停止呼吸 → 熄灭
```

#### 场景 3：插线充电
```
插线（电池未充满）→ 橙色慢闪（3秒）→ 熄灭 → widget 接管
→ 3秒后如果蓝牙未连接 → 蓝色呼吸（30秒）→ 熄灭
→ 充满后拔线再插 → 绿色常亮（3秒）→ 熄灭 → widget 接管
```

#### 场景 4：低电量
```
正常使用中电量降到 <20% → 红色（低电量指示）
→ 插线充电 → 橙色慢闪覆盖（3秒）→ 熄灭 → 恢复低电量红色
→ 充满后 → 低电量指示消失
```

---

## 文件结构

```
zmk-eason60-rev_e/
├── .github/workflows/build.yml    # GitHub Actions 构建配置
├── .gitignore
├── build.yaml                      # 构建矩阵（unit60@rev_e）
├── README.md                       # 本文件
└── config/
    ├── west.yml                    # ZMK 源码引用（zmkfirmware/zmk main，widget 已本地化）
    └── boards/Eason/unit60/
        ├── board.cmake
        ├── board.yml
        ├── CMakeLists.txt          # 编译配置（含本地 widget）
        ├── Kconfig.defconfig
        ├── Kconfig.unit60          # 板卡配置（含合并的 widget Kconfig）
        ├── pre_dt_board.cmake
        ├── revision.cmake
        ├── unit60.dts              # 板卡设备树（DC/DC 配置）
        ├── unit60.zmk.yml          # 板卡元数据
        ├── unit60_charger.c        # BQ24075 充电检测（自定义模块）
        ├── unit60_rev_e.overlay    # rev_e 覆盖（矩阵/引脚/外设）
        ├── unit60_rev_e.keymap     # 键位映射（62 键）
        ├── unit60_rev_e-layouts.dtsi  # 物理布局坐标
        ├── unit60_rev_e-transforms.dtsi # 矩阵映射
        ├── unit60_rev_e-pinctrl.dtsi   # 引脚控制
        ├── unit60_rev_e_defconfig      # Kconfig 配置（含 widget 全部配置）
        └── rgbled_widget/           # 本地 widget 源码（已修复 ZMK main API 兼容）
            ├── include/zmk_rgbled_widget/widget.h
            └── src/widget.c
```

## 自定义模块说明

### 1. unit60_charger.c（BQ24075 充电检测）

- 读取 P1.13（BQ24075 CHG，开漏低有效，内部上拉）
- VBUS 检测使用 nRF52840 硬件寄存器 `USBREGSTATUS`（不依赖 Zephyr USB API）
- 插线后 3 秒内写 LED（充电中橙色慢闪 / 充满绿色常亮），之后完全停写归还 widget
- 拔线立即停写

### 2. rgbled_widget（本地源码，已修复 ZMK main API 兼容）

基于 [hitsmaxft/zmk-rgbled-widget](https://github.com/hitsmaxft/zmk-rgbled-widget) fork（支持 WS2812），已本地化到 `config/boards/Eason/unit60/rgbled_widget/`：

**修复内容：**
- `zmk_endpoint_get_selected()` → `zmk_endpoints_selected()`（2 处，ZMK main 已改名，新 API 返回指针）
- 移除 west.yml 外部模块引用，改为本地编译，避免 API 不兼容导致蓝牙指示失效

**功能：**
- 支持 WS2812 SPI 驱动（单颗 LED，所有状态共享索引 0）
- 支持动画（呼吸脉冲/闪烁/常亮）
- 蓝牙 5 通道统一蓝色，广告/配对状态呼吸 30 秒
- FN 层黄色常亮，Caps Lock 白色常亮
- 低电量红色，高/中电量背景颜色已禁用

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
2. 打开 [ZMK Studio](https://zmk.studio/) 网页端或桌面端
3. 选择串口连接
4. 输入解锁码（默认无锁，直接连接）

> 注意：官方 ZMK Studio 不支持运行时创建宏/组合键，需在 keymap 文件中手动定义后编译烧录。

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
