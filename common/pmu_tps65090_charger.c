/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TPS65090 PMU charging task.
 */

#include "battery_pack.h"
#include "board.h"
#include "clock.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "gpio.h"
#include "pmu_tpschrome.h"
#include "smart_battery.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "tsu6721.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_CHARGER, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

/* Charging and discharging alarms */
#define ALARM_DISCHARGING (ALARM_TERMINATE_DISCHARGE | ALARM_OVER_TEMP)
#define ALARM_CHARGED (ALARM_OVER_CHARGED | ALARM_TERMINATE_CHARGE)

/* Maximum time allowed to revive a extremely low charge battery */
#define PRE_CHARGING_TIMEOUT (15 * SECOND)

/*
 * Time delay in usec for idle, charging and discharging.  Defined in battery
 * charging flow.
 */
#define T1_OFF_USEC     (30 * SECOND)
#define T1_SUSPEND_USEC (30 * SECOND)
#define T1_USEC         (5  * SECOND)
#define T2_USEC         (10 * SECOND)
#define T3_USEC         (10 * SECOND)

#ifndef BATTERY_AP_OFF_LEVEL
#define BATTERY_AP_OFF_LEVEL 0
#endif

static const char * const state_list[] = {
	"idle",
	"reinit",
	"bad cond",
	"pre-charging",
	"charging",
	"charging error",
	"discharging"
};

/* States for throttling PMU task */
static timestamp_t last_waken; /* Initialized to 0 */
static int has_pending_event;

static enum charging_state current_state = ST_REINIT;

static void enable_charging(int enable)
{
	enable = enable ? 1 : 0;
	if (gpio_get_level(GPIO_CHARGER_EN) != enable)
		gpio_set_level(GPIO_CHARGER_EN, enable);
}

/*
 * TODO(rongchang): move battery vendor specific functions to battery pack
 * module
 */
static int battery_temperature_celsius(int t)
{
	return (t - 2731) / 10;
}

static int battery_start_charging_range(int t)
{
	t = battery_temperature_celsius(t);
	return (t >= 5 && t < 45);
}

static int battery_charging_range(int t)
{
	t = battery_temperature_celsius(t);
	return (t >= 5 && t < 60);
}

static int battery_discharging_range(int t)
{
	t = battery_temperature_celsius(t);
	return (t >= 0 && t < 100);
}

/*
 * Turn off the host application processor
 */
static int system_off(void)
{
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		CPUTS("[pmu] turn system off\n");
		/* TODO(rongchang): need chipset_force_hard_off(),
		 * and remove these gpio hack
		 */
		gpio_set_level(GPIO_EN_PP3300, 0);
		gpio_set_level(GPIO_EN_PP1350, 0);
		gpio_set_level(GPIO_PMIC_PWRON_L, 1);
		gpio_set_level(GPIO_EN_PP5000, 0);
	}

	return ST_REINIT;
}

/*
 * Notify the host when battery remaining charge is lower than 10%
 */
static int notify_battery_low(void)
{
	static timestamp_t last_notify_time;
	timestamp_t now;

	if (chipset_in_state(CHIPSET_STATE_ON)) {
		now = get_time();
		if (now.val - last_notify_time.val > MINUTE) {
			CPUTS("[pmu] notify battery low (< 10%)\n");
			last_notify_time = now;
			/* TODO(rongchang): notify AP ? */
		}
	}
	return ST_DISCHARGING;
}

/*
 * Calculate relative state of charge moving average
 *
 * @param state_of_charge         Current battery state of charge reading,
 *                                from 0 to 100. When state_of_charge < 0,
 *                                resets the moving average window
 * @return                        Average state of charge, rounded to nearest
 *                                integer.
 *                                -1 when window reset.
 *
 * The returned value will be rounded to the nearest integer, by set moving
 * average init value to (0.5 * window_size).
 *
 */
static int rsoc_moving_average(int state_of_charge)
{
	static uint8_t rsoc[4];
	static int8_t index = -1;
	int moving_average = ARRAY_SIZE(rsoc) / 2;
	int i;

	if (state_of_charge < 0) {
		index = -1;
		return -1;
	}

	if (index < 0) {
		for (i = 0; i < ARRAY_SIZE(rsoc); i++)
			rsoc[i] = (uint8_t)state_of_charge;
		index = 0;
		return state_of_charge;
	}

	rsoc[index] = (uint8_t)state_of_charge;
	index++;
	index %= sizeof(rsoc);

	for (i = 0; i < ARRAY_SIZE(rsoc); i++)
		moving_average += (int)rsoc[i];
	moving_average /= ARRAY_SIZE(rsoc);

	return moving_average;
}


static int config_low_current_charging(int charge)
{
	/* Disable low current termination */
	if (charge < 40)
		return pmu_low_current_charging(1);

	/* Enable low current termination */
	if (charge > 60)
		return pmu_low_current_charging(0);

	return EC_SUCCESS;
}

static int calc_next_state(int state)
{
	int batt_temp, alarm, capacity, charge;

	switch (state) {
	case ST_REINIT:
	case ST_BAD_COND:
	case ST_IDLE:
		/* Check AC and chiset state */
		if (!board_get_ac()) {
			if (chipset_in_state(CHIPSET_STATE_ON))
				return ST_DISCHARGING;
			return ST_IDLE;
		}

		if (battery_is_cut_off())
			return ST_BAD_COND;

		/* Stay in idle mode if charger overtemp */
		if (pmu_is_charger_alarm())
			return ST_BAD_COND;

		/* Enable charging when battery doesn't respond */
		if (battery_temperature(&batt_temp)) {
			if (config_low_current_charging(0))
				return ST_BAD_COND;
			return ST_PRE_CHARGING;
		}

		/* Turn off charger when battery temperature is out
		 * of the start charging range.
		 */
		if (!battery_start_charging_range(batt_temp))
			return ST_BAD_COND;

		/* Turn off charger on battery over temperature alarm */
		if (battery_status(&alarm) || (alarm & ALARM_OVER_TEMP))
			return ST_BAD_COND;

		/* Stop charging if the battery says it's charged */
		if (alarm & ALARM_CHARGED)
			return ST_IDLE;

		/* Start charging only when battery charge lower than 100% */
		if (!battery_state_of_charge(&charge)) {
			config_low_current_charging(charge);
			if (charge < 100)
				return ST_CHARGING;
		}

		return ST_IDLE;

	case ST_PRE_CHARGING:
		if (!board_get_ac())
			return ST_REINIT;

		/* If the battery goes online after enable the charger,
		 * go into charging state.
		 */
		if (battery_temperature(&batt_temp) == EC_SUCCESS) {
			if (!battery_start_charging_range(batt_temp))
				return ST_REINIT;
			if (!battery_state_of_charge(&charge)) {
				config_low_current_charging(charge);
				if (charge >= 100)
					return ST_REINIT;
			}
			return ST_CHARGING;
		}

		return ST_PRE_CHARGING;

	case ST_CHARGING:
		/* Go back to idle state when AC is unplugged */
		if (!board_get_ac())
			return ST_REINIT;

		/*
		 * Disable charging on battery access error, or charging
		 * temperature out of range.
		 */
		if (battery_temperature(&batt_temp)) {
			CPUTS("[pmu] charging: unable to get battery "
			      "temperature\n");
			return ST_REINIT;
		} else if (!battery_charging_range(batt_temp)) {
			CPRINTF("[pmu] charging: temperature out of range "
				"%dC\n",
				battery_temperature_celsius(batt_temp));
			return ST_CHARGING_ERROR;
		}

		/*
		 * Disable charging on battery alarm events or access error:
		 *   - over temperature
		 *   - over current
		 */
		if (battery_status(&alarm))
			return ST_REINIT;

		if (alarm & ALARM_OVER_TEMP) {
			CPUTS("[pmu] charging: battery over temp\n");
			return ST_CHARGING_ERROR;
		}

		/* Go to idle state if battery is fully charged */
		if (alarm & ALARM_CHARGED)
			return ST_IDLE;

		/*
		 * Disable charging on charger alarm events:
		 *   - charger over current
		 *   - charger over temperature
		 */
		if (pmu_is_charger_alarm()) {
			CPUTS("[pmu] charging: charger alarm\n");
			return ST_REINIT;
		}

#ifdef BOARD_spring
		/* Re-init on charger timeout. */
		if (pmu_is_charge_timeout()) {
			CPUTS("[pmu] charging: timeout\n");
			return ST_REINIT;
		}
#endif

		return ST_CHARGING;

	case ST_CHARGING_ERROR:
		/*
		 * This state indicates AC is plugged but the battery is not
		 * charging. The conditions to exit this state:
		 *   - battery detected
		 *   - battery temperature is in start charging range
		 *   - no battery alarm
		 */
		if (board_get_ac()) {
			if (battery_status(&alarm))
				return ST_CHARGING_ERROR;

			if (alarm & ALARM_OVER_TEMP)
				return ST_CHARGING_ERROR;

			if (battery_temperature(&batt_temp))
				return ST_CHARGING_ERROR;

			if (!battery_charging_range(batt_temp))
				return ST_CHARGING_ERROR;

			return ST_CHARGING;
		}

		return ST_REINIT;


	case ST_DISCHARGING:
		/* Go back to idle state when AC is plugged */
		if (board_get_ac())
			return ST_REINIT;

		/* Prepare EC sleep after system stopped discharging */
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			return ST_REINIT;

		/* Check battery discharging temperature range */
		if (battery_temperature(&batt_temp) == 0) {
			if (!battery_discharging_range(batt_temp)) {
				CPRINTF("[pmu] discharging: temperature out of"
					"range %dC\n",
					battery_temperature_celsius(batt_temp));
				return system_off();
			}
		}
		/* Check discharging alarm */
		if (!battery_status(&alarm) && (alarm & ALARM_DISCHARGING)) {
			CPRINTF("[pmu] discharging: battery alarm %016b\n",
					alarm);
			return system_off();
		}
		/* Check remaining charge % */
		if (battery_state_of_charge(&capacity) == 0) {
			/*
			 * Shutdown AP when state of charge < 2.5%.
			 * Moving average is rounded to integer.
			 */
			if (rsoc_moving_average(capacity) < 3) {
				return system_off();
			} else if (capacity < 10) {
				notify_battery_low();
			}
		}

		return ST_DISCHARGING;
	}

	return ST_REINIT;
}

/* TODO: Merge charge_state.h and unify charge interface */
enum charging_state charge_get_state(void)
{
	return current_state;
}

int __board_has_high_power_ac(void)
{
	return 0;
}

int board_has_high_power_ac(void)
	__attribute__((weak, alias("__board_has_high_power_ac")));

int charge_keep_power_off(void)
{
	int charge;

#ifdef CONFIG_BATTERY_SPRING
	int blk_opstatus;
	int ret;
	/*
	 * bq30z55 specific: reads OperationStatus register
	 * and checks the status of the discharging FET (DSG FET)
	 * if it is disabled, we cannot use the battery as a buffer
	 * whatever charge level it is returning.
	 */
	ret = sb_write(SB_MANUFACTURER_ACCESS, 0x54 /* OperationStatus */);
	ret |= sb_read(SB_MANUFACTURER_DATA, &blk_opstatus);
	/* DSG FET status is <bit 1> of OperationStatus register
	 * but this register is accessed using an SMBUS block transfer
	 * so the first byte is the size of the transfer rather than
	 * the first byte of data.
	 * if we can access OperationStatus register AND DSG FET is disabled,
	 * we stay off.
	 */
	if (!ret && !(blk_opstatus & (1 << (1 + 8))))
		return 1;
#endif
	if (battery_cell_in_undervoltage())
		return 1;

	if (BATTERY_AP_OFF_LEVEL == 0)
		return 0;

	if (board_has_high_power_ac())
		return 0;

	if (battery_remaining_capacity(&charge))
		return current_state != ST_CHARGING_ERROR;

	return charge <= BATTERY_AP_OFF_LEVEL;
}

void pmu_charger_task(void)
{
	int next_state;
	int wait_time = T1_USEC;
	timestamp_t pre_chg_start = get_time();

	pmu_init();
	/*
	 * EC STOP mode support
	 *   The charging loop can be stopped in idle state with AC unplugged.
	 *   Charging loop will be resumed by TPSCHROME interrupt.
	 */
	enable_charging(0);
	disable_sleep(SLEEP_MASK_CHARGING);

#ifdef CONFIG_TSU6721
	board_pwm_init_limit();

	/*
	 * Somehow TSU6721 comes up slowly. Let's wait for a moment before
	 * accessing it.
	 * TODO(victoryang): Investigate slow init issue.
	 */
	msleep(500);

	tsu6721_reset(); /* Init here until we can do with HOOK_INIT */
	gpio_enable_interrupt(GPIO_USB_CHG_INT);
	msleep(100); /* TSU6721 doesn't work properly right away. */
	board_usb_charge_update(1);
#endif

	while (1) {
		last_waken = get_time();
		pmu_clear_irq();

#ifdef CONFIG_TSU6721
		board_usb_charge_update(0);
#endif

		battery_check_cut_off();

		/*
		 * When battery is extremely low, the internal voltage can not
		 * power on its gas guage IC. Charging loop will enable the
		 * charger and turn on trickle charging. For safty reason,
		 * charger should be disabled if the communication to battery
		 * failed.
		 */
		if (current_state == ST_PRE_CHARGING &&
		    get_time().val - pre_chg_start.val >= PRE_CHARGING_TIMEOUT)
			next_state = ST_CHARGING_ERROR;
		else
			next_state = calc_next_state(current_state);

		if (next_state != current_state) {
			/* Reset state of charge moving average window */
			rsoc_moving_average(-1);

			CPRINTF("[batt] state %s -> %s\n",
				state_list[current_state],
				state_list[next_state]);

			current_state = next_state;

			switch (current_state) {
			case ST_PRE_CHARGING:
				pre_chg_start = get_time();
				/* Fall through */
			case ST_CHARGING:
				enable_charging(1);
				break;
			case ST_IDLE:
			case ST_REINIT:
			case ST_BAD_COND:
			case ST_DISCHARGING:
			case ST_CHARGING_ERROR:
				enable_charging(0);
				break;
			}
		}

		switch (current_state) {
		case ST_CHARGING:
		case ST_CHARGING_ERROR:
			wait_time = T2_USEC;
			break;
		case ST_DISCHARGING:
			wait_time = T3_USEC;
			break;
		case ST_PRE_CHARGING:
			wait_time = T1_USEC;
			if (get_time().val - pre_chg_start.val >=
			    PRE_CHARGING_TIMEOUT)
				enable_charging(0);
			break;
		default:
			if (board_get_ac()) {
				wait_time = T1_USEC;
				break;
			} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
				wait_time = T1_OFF_USEC;
				enable_sleep(SLEEP_MASK_CHARGING);
			} else if (chipset_in_state(CHIPSET_STATE_SUSPEND)) {
				wait_time = T1_SUSPEND_USEC;
			} else {
				wait_time = T1_USEC;
			}
		}

#ifdef CONFIG_TSU6721
		has_pending_event |= tsu6721_peek_interrupts();
#endif

		if (!has_pending_event) {
			task_wait_event(wait_time);
			disable_sleep(SLEEP_MASK_CHARGING);
		} else {
			has_pending_event = 0;
		}
	}
}

void pmu_task_throttled_wake(void)
{
	timestamp_t now = get_time();
	if (now.val - last_waken.val >= HOOK_TICK_INTERVAL) {
		has_pending_event = 0;
		task_wake(TASK_ID_PMU_TPS65090_CHARGER);
	} else {
		has_pending_event = 1;
	}
}

static void wake_pmu_task_if_necessary(void)
{
	if (has_pending_event) {
		has_pending_event = 0;
		task_wake(TASK_ID_PMU_TPS65090_CHARGER);
	}
}
DECLARE_HOOK(HOOK_TICK, wake_pmu_task_if_necessary, HOOK_PRIO_DEFAULT);

/* Wake charging task on chipset events */
static void pmu_chipset_events(void)
{
	pmu_task_throttled_wake();
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, pmu_chipset_events, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, pmu_chipset_events, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, pmu_chipset_events, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, pmu_chipset_events, HOOK_PRIO_DEFAULT);
