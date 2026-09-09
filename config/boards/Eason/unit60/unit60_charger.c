/*
 * Copyright (c) 2025 Eason
 *
 * SPDX-License-Identifier: MIT
 *
 * BQ24075 charging status indicator for unit60
 *
 * Hardware:
 *   BQ24075 CHG (open-drain, active-low) -> E73 pin 6 = P1.13, 10k series
 *   External pull-up required on CHG line
 *
 * Behavior (WS2812 single LED, shared with zmk-rgbled-widget):
 *
 *   USB plug-in event (transition no-USB -> USB):
 *     If charging (CHG=low):
 *       Orange slow blink (1Hz) for 3s -> "charging"
 *       Then off, charger stops writing LED, widget takes over.
 *     If full (CHG=high):
 *       Green solid for 3s -> "full / USB connected"
 *       Then off, charger stops writing LED, widget takes over.
 *
 *   After the 3s indicator: charger does NOT write LED at all.
 *   All other states (low battery, FN layer, Caps, BLE, etc.)
 *   are handled entirely by zmk-rgbled-widget.
 *
 *   On USB unplug: immediately stop any indicator, charger goes idle.
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

/* ---- Simple state machine ---- */
enum charger_state {
	STATE_IDLE,             /* charger not writing LED, widget controls */
	STATE_CHARGING_ORANGE,  /* orange slow blink = charging (3s after plug-in) */
	STATE_FULL_GREEN,        /* green solid = full / USB connected (3s after plug-in) */
};

static enum charger_state current_state = STATE_IDLE;
static int state_ticks = 0;        /* 1 tick = 100ms */
static bool prev_usb_powered = false;
static bool initialized = false;

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

/* Transition to IDLE: turn off LED and stop writing (widget takes over) */
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
			/* Battery charging: orange blink 3s */
			current_state = STATE_CHARGING_ORANGE;
			state_ticks = 0;
			LOG_INF("Indicator: charging orange blink (3s)");
		} else {
			/* Battery full / USB only: green solid 3s */
			current_state = STATE_FULL_GREEN;
			state_ticks = 0;
			LOG_INF("Indicator: full green solid (3s)");
		}
	}

	/* ---- Detect USB unplug event ---- */
	if (!usb_powered && prev_usb_powered) {
		LOG_INF("USB unplugged, charger going idle");
		go_idle();
	}

	prev_usb_powered = usb_powered;

	/* ---- State machine (charger only writes LED during 3s indicator) ---- */
	switch (current_state) {
	case STATE_CHARGING_ORANGE:
		/* Orange slow blink (1Hz) for 3 seconds = charging */
		if ((state_ticks % (BLINK_HALF_PERIOD * 2)) < BLINK_HALF_PERIOD) {
			set_led_color(255, 140, 0); /* orange */
		} else {
			set_led_color(0, 0, 0);     /* off */
		}
		state_ticks++;
		if (state_ticks >= DURATION_3S) {
			LOG_INF("Charging indicator complete, charger going idle");
			go_idle();
		}
		break;

	case STATE_FULL_GREEN:
		/* Green solid for 3 seconds = full / USB connected */
		set_led_color(0, 200, 0);
		state_ticks++;
		if (state_ticks >= DURATION_3S) {
			LOG_INF("Full indicator complete, charger going idle");
			go_idle();
		}
		break;

	case STATE_IDLE:
	default:
		/* Do NOT write LED here. Widget has full control. */
		break;
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

	LOG_INF("BQ24075 charger indicator initialized (CHG=P1.13, 3s plug-in indicator only)");
	return 0;
}

SYS_INIT(unit60_charger_init, APPLICATION, 90);
