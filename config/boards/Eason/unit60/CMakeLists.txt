# 三色 LED 已移除，原 unit60_led.c（蓝牙 profile 颜色指示）不再编译
# 全部状态指示由 WS2812 单颗承担（本地 rgbled_widget + 自定义充电检测）

# BQ24075 充电状态检测（CHG → P1.13）
zephyr_library_sources(unit60_charger.c)

# 本地 rgbled_widget（基于 hitsmaxft fork，已修复 ZMK main API 兼容）
# 修复内容：zmk_endpoint_get_selected() -> zmk_endpoints_selected()（返回指针）
zephyr_library_sources(rgbled_widget/src/widget.c)
zephyr_library_include_directories(rgbled_widget/include)
