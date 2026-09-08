/*
 * Copyright (c) 2025 Eason
 *
 * SPDX-License-Identifier: MIT
 *
 * BQ24075 charging status monitor for unit60
 *
 * Hardware:
 *   BQ24075 CHG (open-drain, active-low) → E73 pin 6 = P1.13, 10k series
 *   External pull-up required on CHG line
 *
 * Behavior (WS2812 single LED, shared with zmk-rgbled-widget):
 *   Charging   (CHG=low):            red solid   — override widget continuously
 *   Full       (CHG=high + USB in):  green solid — 3s, then restore widget
 *   Discharging(CHG=high, no USB):   —           — no override, widget controls
 *
 * Priority: charging red > widget auto states (matches status plan level 4)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <zmk/usb.h>

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

/* ---- Charge states ---- */
enum charge_state {
	STATE_DISCHARGING,
	STATE_CHARGING,
	STATE_FULL,
};

static enum charge_state current_state = STATE_DISCHARGING;
static bool full_indicator_active = false;
static bool initialized = false;

/* Write a single RGB pixel to the WS2812 strip (overrides widget) */
static void set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
	if (!device_is_ready(led_strip)) {
		return;
	}
	struct led_rgb pixel = { .r = r, .g = g, .b = b };

	led_strip_update_rgb(led_strip, &pixel, 1);
}

/* Called 3s after entering FULL state: stop green override */
static void full_indicator_off(struct k_work *work)
{
	full_indicator_active = false;
	LOG_INF("Full charge indicator ended, restoring widget control");
}
K_WORK_DELAYABLE_DEFINE(full_work, full_indicator_off);

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
	bool usb_powered = zmk_usb_is_powered();

	enum charge_state new_state;

	if (chg_active) {
		new_state = STATE_CHARGING;
	} else if (usb_powered) {
		new_state = STATE_FULL;
	} else {
		new_state = STATE_DISCHARGING;
	}

	if (new_state != current_state) {
		current_state = new_state;
		LOG_INF("Charge state -> %s (CHG_phys=%d usb=%d)",
			new_state == STATE_CHARGING ? "CHARGING" :
			new_state == STATE_FULL ? "FULL" : "DISCHARGING",
			val, usb_powered);

		if (new_state == STATE_FULL) {
			/* Green solid for 3 seconds, then restore widget */
			full_indicator_active = true;
			k_work_reschedule(&full_work, K_SECONDS(3));
		} else if (new_state == STATE_CHARGING) {
			/* Cancel any pending full-indicator timeout */
			k_work_cancel_delayable(&full_work);
			full_indicator_active = false;
		}
	}

	/* Continuously override LED while charging (widget tick is 125ms,
	 * we write every 100ms so red always wins).
	 */
	if (current_state == STATE_CHARGING) {
		set_led_color(200, 0, 0); /* red */
	} else if (full_indicator_active) {
		set_led_color(0, 200, 0); /* green */
	}
	/* else: do not write, widget resumes control */
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

	LOG_INF("BQ24075 charger monitor initialized (CHG=P1.13, active-low)");
	return 0;
}

SYS_INIT(unit60_charger, APPLICATION, 90);
