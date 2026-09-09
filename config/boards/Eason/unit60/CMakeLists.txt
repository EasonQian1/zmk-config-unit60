# 三色 LED 已移除，原 unit60_led.c（蓝牙 profile 颜色指示）不再编译
# 全部状态指示由 WS2812 单颗承担（本地 rgbled_widget + 自定义充电检测）

# BQ24075 充电状态检测（CHG → P1.13）
zephyr_library_sources(unit60_charger.c)

# 本地 rgbled_widget（已适配 ph-design/zmks 分支 API）
zephyr_library_sources(
    rgbled_widget/src/widget.c
    rgbled_widget/src/behaviors/behavior_rgbled_widget.c
)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rgbled_widget/include)
