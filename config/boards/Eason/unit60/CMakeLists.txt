# 三色 LED 已移除，原 unit60_led.c（蓝牙 profile 颜色指示）不再编译
# 全部状态指示由 WS2812 单颗承担（zmk-rgbled-widget + 自定义充电检测）

# BQ24075 充电状态检测（CHG → P1.13）
zephyr_library_sources(unit60_charger.c)
