/*
 * Copyright (c) 2025 Eason
 *
 * SPDX-License-Identifier: MIT
 *
 * BQ24075 charging status + low battery monitor for unit60
 *
 * Hardware:
 *   BQ24075 CHG (open-drain, active-low) -> E73 pin 6 = P1.13, 10k series
 *   External pull-up required on CHG line
 *   MAX17048 fuel gauge (I2C) for battery percentage
 *
 * Behavior (WS2812 single LED, shared with zmk-rgbled-widget):
 *
 *   Priority 1 (highest): Plug-in indicators
 *
 *     A. Charging (CHG=low + USB present):
 *        - Orange slow blink (1Hz) for 3s  -> "charging in progress"
 *        - Green blink (1Hz) for 3s        -> "USB wired mode connected"
 *        - Then off, restore widget control
 *
 *     B. Full / USB powered (CHG=high + USB present):
 *        - Green solid CONTINUOUSLY        -> "fully charged / USB mode"
 *        - Stays on as long as USB is plugged in and battery is full
 *
 *   Priority 2: Low battery (< 20%)
 *     Red slow blink (1Hz) continuously, overrides widget.
 *     Only shown when no plug-in indicator is active.
 *     (Note: full state implies >20%, so no conflict with full green.)
 *
 *   Priority 3: Widget auto states
 *     All other times: do not write LED, widget controls (FN/Caps/BLE/etc).
 *     Normal battery colors (>20%) are NOT shown.
 *
 *   On USB unplug: immediately stop any indicator, then check low battery.
 *
 * Poll interval: 100ms (1 tick = 100ms, 30 ticks = 3s)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

/* nRF52840 USBD USBREGSTATUS register - hardware VBUS detection.
 * Bit 0 (VBUSDETECT): 1 = VBUS present (USB cable plugged in).
 * No Zephyr USB API dependency, works on Zephyr 4.x.
 */
#define NRF_USBD_BASE            0x40027000UL
#define USBD_USBREGSTATUS_OFF    0x438UL
#define USBD_USBREGSTATUS        (*(volatile uint32_t *)(NRF_USBD_BASE + USBD_USBREGSTATUS_OFF))
#define USBD_VBUSDETECT_Pos      0
#define USBD_VBUSDETECT_Msk      (1UL << USBD_VBUSDETECT_Pos)

static bool usb_vbus_present(void)
{
	return (USBD_USBREGSTATUS & USBD_VBUSDETECT_Msk) != 0;
}

/* ZMK battery percentage API (manually declared to avoid private header path).
 * Returns 0-100. Defined in zmk/app/src/battery.c.
 */
extern uint8_t zmk_battery_state_of_charge(void);

LOG_MODULE_REGISTER(unit60_charger, LOG_LEVEL_INF);

/* ---- BQ24075 CHG pin: P1.13, active-low open-drain ---- */
static const struct gpio_dt_spec chg_pin = {
	.port = DEVICE_DT_GET(DT_NODELABEL(gpio1)),
	.pin = 13,
	.dt_flags = GPIO_ACTIVE_LOW,
};

/* ---- WS2812 LED strip (alias status-ws2812 in overlay) ---- */
#define LED_STRIP_NODE DT_ALIAS(status_ws2812)
static const struct device *led_strip;

/* ---- State machine ---- */
enum charger_state {
	STATE_IDLE,             /* no active indicator, widget or low-battery controls */
	STATE_CHARGING_ORANGE,  /* orange slow blink = charging (first 3s after plug-in) */
	STATE_USB_GREEN_BLINK,  /* green blink = USB wired mode (next 3s after charging) */
	STATE_FULL_GREEN,        /* green solid CONTINUOUSLY = full + USB plugged in */
};

static enum charger_state current_state = STATE_IDLE;
static int state_ticks = 0;        /* 1 tick = 100ms */
static bool prev_usb_powered = false;
static bool initialized = false;

/* Low battery threshold */
#define LOW_BATTERY_THRESHOLD  20   /* percent */

/* Timing constants (in 100ms ticks) */
#define DURATION_3S            30   /* 3 seconds */
#define BLINK_HALF_PERIOD      5    /* 500ms on / 500ms off = 1Hz */

/* Write a single RGB pixel to the WS2812 strip (overrides widget) */
static void set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
	if (!device_is_ready(led_strip)) {
		return;
	}
	struct led_rgb pixel = { .r = r, .g = g, .b = b };

	led_strip_update_rgb(led_strip, &pixel, 1);
}

/* Transition to IDLE and turn off LED override */
static void go_idle(void)
{
	set_led_color(0, 0, 0);
	current_state = STATE_IDLE;
	state_ticks = 0;
}

/* Main poll worker: runs every 100ms */
static void charger_work_handler(struct k_work *work)
{
	if (!initialized) {
		return;
	}

	/* gpio_pin_get_dt with GPIO_ACTIVE_LOW:
	 * physical low  (charging)  -> returns 1
	 * physical high (not chg)   -> returns 0
	 */
	int val = gpio_pin_get_dt(&chg_pin);
	bool chg_active = (val == 1);
	bool usb_powered = usb_vbus_present();

	/* ---- Detect USB plug-in event (transition no-USB -> USB) ---- */
	if (usb_powered && !prev_usb_powered) {
		LOG_INF("USB plugged in (CHG_phys=%d, charging=%s)",
			val, chg_active ? "yes" : "no");

		if (chg_active) {
			/* Battery charging: orange blink 3s -> green blink 3s */
			current_state = STATE_CHARGING_ORANGE;
			state_ticks = 0;
			LOG_INF("State: CHARGING_ORANGE (3s)");
		} else {
			/* Battery full: green solid continuously */
			current_state = STATE_FULL_GREEN;
			state_ticks = 0;
			LOG_INF("State: FULL_GREEN (continuous)");
		}
	}

	/* ---- Detect USB unplug event ---- */
	if (!usb_powered && prev_usb_powered) {
		LOG_INF("USB unplugged, going idle");
		go_idle();
	}

	prev_usb_powered = usb_powered;

	/* ---- State machine (Priority 1: plug-in indicators) ---- */
	bool indicator_active = (current_state != STATE_IDLE);

	switch (current_state) {
	case STATE_CHARGING_ORANGE:
		/* Orange slow blink (1Hz) for 3 seconds = charging in progress */
		if ((state_ticks % (BLINK_HALF_PERIOD * 2)) < BLINK_HALF_PERIOD) {
			set_led_color(255, 140, 0); /* orange */
		} else {
			set_led_color(0, 0, 0);     /* off */
		}
		state_ticks++;
		if (state_ticks >= DURATION_3S) {
			current_state = STATE_USB_GREEN_BLINK;
			state_ticks = 0;
			set_led_color(0, 0, 0); /* brief off transition */
			LOG_INF("State: USB_GREEN_BLINK (3s)");
		}
		break;

	case STATE_USB_GREEN_BLINK:
		/* Green blink (1Hz) for 3 seconds = USB wired mode */
		if ((state_ticks % (BLINK_HALF_PERIOD * 2)) < BLINK_HALF_PERIOD) {
			set_led_color(0, 200, 0); /* green */
		} else {
			set_led_color(0, 0, 0);
		}
		state_ticks++;
		if (state_ticks >= DURATION_3S) {
			LOG_INF("USB indicator complete, going idle");
			go_idle();
		}
		break;

	case STATE_FULL_GREEN:
		/* Green solid CONTINUOUSLY = fully charged + USB plugged in */
		set_led_color(0, 200, 0);
		/* Stay in this state as long as USB is plugged in and CHG=high.
		 * If CHG goes low (battery starts draining despite USB?), go idle.
		 */
		if (chg_active) {
			LOG_INF("CHG went low while in FULL state, going idle");
			go_idle();
		}
		break;

	case STATE_IDLE:
	default:
		/* Do not write LED here — handled below by low battery check */
		break;
	}

	/* ---- Low battery indicator (Priority 2) ----
	 * Only active when no plug-in indicator is running.
	 * Red slow blink (1Hz) continuously when battery < 20%.
	 * Note: FULL_GREEN implies battery >20%, so no conflict.
	 */
	if (!indicator_active) {
		uint8_t battery = zmk_battery_state_of_charge();

		if (battery < LOW_BATTERY_THRESHOLD) {
			/* Red slow blink (1Hz) */
			int tick_100ms = k_uptime_get_32() / 100;
			if ((tick_100ms % (BLINK_HALF_PERIOD * 2)) < BLINK_HALF_PERIOD) {
				set_led_color(200, 0, 0); /* red */
			} else {
				set_led_color(0, 0, 0);   /* off */
			}
		}
		/* else: battery >= 20%, do NOT show battery color.
		 * Widget controls all other states (FN/Caps/BLE/etc).
		 */
	}
}

K_WORK_DEFINE(charger_work, charger_work_handler);

static void charger_timer_handler(struct k_timer *timer)
{
	k_work_submit(&charger_work);
}

K_TIMER_DEFINE(charger_timer, charger_timer_handler, NULL);

static int unit60_charger_init(void)
{
	/* Get LED strip */
	led_strip = DEVICE_DT_GET(LED_STRIP_NODE);
	if (!device_is_ready(led_strip)) {
		LOG_ERR("LED strip (status-ws2812) not ready");
		return -ENODEV;
	}

	/* Configure CHG pin: input + internal pull-up (BQ24075 is open-drain) */
	if (!gpio_is_ready_dt(&chg_pin)) {
		LOG_ERR("CHG GPIO port (gpio1) not ready");
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&chg_pin, GPIO_INPUT | GPIO_PULL_UP);

	if (ret != 0) {
		LOG_ERR("Failed to configure CHG pin P1.13: %d", ret);
		return ret;
	}

	initialized = true;

	/* Start polling: first check after 500ms, then every 100ms */
	k_timer_start(&charger_timer, K_MSEC(500), K_MSEC(100));

	LOG_INF("BQ24075 charger + low battery monitor initialized "
		"(CHG=P1.13, low_bat_threshold=%d%%, full=continuous green)",
		LOW_BATTERY_THRESHOLD);
	return 0;
}

SYS_INIT(unit60_charger_init, APPLICATION, 90);
