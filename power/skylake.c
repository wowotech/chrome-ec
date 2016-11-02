/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Skylake IMVP8 / ROP PMIC chipset power control module for Chrome EC */

#include "board_config.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "ec_commands.h"
#include "espi.h"
#include "hooks.h"
#include "intel_x86.h"
#include "lpc.h"
#include "power_button.h"
#include "skylake.h"
#include "system.h"
#include "util.h"
#include "wireless.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

static int forcing_shutdown;  /* Forced shutdown in progress? */
static int power_s5_up;       /* Chipset is sequencing up or down */

enum sys_sleep_state {
	SYS_SLEEP_S5,
	SYS_SLEEP_S4,
	SYS_SLEEP_S3
};

/* Get system sleep state through GPIOs or VWs */
static int chipset_get_sleep_signal(enum sys_sleep_state state)
{
#ifdef CONFIG_ESPI_VW_SIGNALS
	if (state == SYS_SLEEP_S4)
		return espi_vw_get_wire(VW_SLP_S4_L);
	else if (state == SYS_SLEEP_S3)
		return espi_vw_get_wire(VW_SLP_S3_L);
#else
	if (state == SYS_SLEEP_S4)
		return gpio_get_level(GPIO_PCH_SLP_S4_L);
	else if (state == SYS_SLEEP_S3)
		return gpio_get_level(GPIO_PCH_SLP_S3_L);
#endif

	/* We should never run here */
	ASSERT(0);
	return 0;
}

void chipset_force_shutdown(void)
{
	CPRINTS("%s()", __func__);

	/*
	 * Force off. Sending a reset command to the PMIC will power off
	 * the EC, so simulate a long power button press instead. This
	 * condition will reset once the state machine transitions to G3.
	 * Consider reducing the latency here by changing the power off
	 * hold time on the PMIC.
	 */
	if (!chipset_in_state(CHIPSET_STATE_HARD_OFF)) {
		forcing_shutdown = 1;
		power_button_pch_press();
	}
}

__attribute__((weak)) void chipset_set_pmic_slp_sus_l(int level)
{
	gpio_set_level(GPIO_PMIC_SLP_SUS_L, level);
}

void chipset_force_g3(void)
{
	CPRINTS("Forcing fake G3.");

	chipset_set_pmic_slp_sus_l(0);
}

void chipset_reset(int cold_reset)
{
	CPRINTS("%s(%d)", __func__, cold_reset);

	if (cold_reset) {
		if (gpio_get_level(GPIO_SYS_RESET_L) == 0)
			return;
		gpio_set_level(GPIO_SYS_RESET_L, 0);
		/* Debounce time for SYS_RESET_L is 16 ms */
		udelay(20 * MSEC);
		gpio_set_level(GPIO_SYS_RESET_L, 1);
	} else {
		/*
		 * Send a RCIN_PCH_RCIN_L
		 * assert INIT# to the CPU without dropping power or asserting
		 * PLTRST# to reset the rest of the system.
		 */

		/* Pulse must be at least 16 PCI clocks long = 500 ns */
#ifdef CONFIG_ESPI_VW_SIGNALS
		lpc_host_reset();
#else
		gpio_set_level(GPIO_PCH_RCIN_L, 0);
		udelay(10);
		gpio_set_level(GPIO_PCH_RCIN_L, 1);
#endif
	}
}

void handle_rsmrst(enum power_state state)
{
	/*
	 * Pass through RSMRST asynchronously, as PCH may not react
	 * immediately to power changes.
	 */
	int rsmrst_in = gpio_get_level(GPIO_RSMRST_L_PGOOD);
	int rsmrst_out = gpio_get_level(GPIO_PCH_RSMRST_L);

	/* Nothing to do. */
	if (rsmrst_in == rsmrst_out)
		return;

#ifdef CONFIG_BOARD_HAS_BEFORE_RSMRST
	board_before_rsmrst(rsmrst_in);
#endif

	/*
	 * Wait at least 10ms between power signals going high
	 * and deasserting RSMRST to PCH.
	 */
	if (rsmrst_in)
		msleep(10);
	gpio_set_level(GPIO_PCH_RSMRST_L, rsmrst_in);
	CPRINTS("RSMRST: %d", rsmrst_in);
}

static void handle_slp_sus(enum power_state state)
{
	/* If we're down or going down don't do anythin with SLP_SUS_L. */
	if (state == POWER_G3 || state == POWER_S5G3)
		return;

	/* Always mimic PCH SLP_SUS request for all other states. */
	chipset_set_pmic_slp_sus_l(gpio_get_level(GPIO_PCH_SLP_SUS_L));
}

static enum power_state _power_handle_state(enum power_state state)
{
	int tries = 0;

	switch (state) {
	case POWER_G3:
		break;

	case POWER_S5:
		if (forcing_shutdown) {
			power_button_pch_release();
			forcing_shutdown = 0;
		}

#ifdef CONFIG_BOARD_HAS_RTC_RESET
		/* Wait for S5 exit and attempt RTC reset it supported */
		if (power_s5_up)
			return power_wait_s5_rtc_reset();
#endif
		if (chipset_get_sleep_signal(SYS_SLEEP_S4) == 1)
			return POWER_S5S3; /* Power up to next state */
		break;

	case POWER_S3:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S3S5;
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1) {
			/* Power up to next state */
			return POWER_S3S0;
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S4) == 0) {
			/* Power down to next state */
			return POWER_S3S5;
		}
		break;

	case POWER_S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			chipset_force_shutdown();
			return POWER_S0S3;
#ifdef CONFIG_POWER_S0IX
		} else if ((power_get_host_sleep_state() ==
			    HOST_SLEEP_EVENT_S0IX_SUSPEND) &&
			   (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1)) {
			return POWER_S0S0ix;
#endif
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S3) == 0) {
			/* Power down to next state */
			return POWER_S0S3;
		}

		break;

#ifdef CONFIG_POWER_S0IX
	case POWER_S0ix:
		/*
		 * TODO: add code for unexpected power loss
		 */
		if ((power_get_host_sleep_state() ==
		     HOST_SLEEP_EVENT_S0IX_RESUME) &&
		   (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1)) {
			return POWER_S0ixS0;
		}

		break;
#endif

	case POWER_G3S5:
		/* Call hooks to initialize PMIC */
		hook_notify(HOOK_CHIPSET_PRE_INIT);

		/*
		 * Allow up to 1s for charger to be initialized, in case
		 * we're trying to boot the AP with no battery.
		 */
		while (charge_prevent_power_on(0) &&
		       tries++ < CHARGER_INITIALIZED_TRIES) {
			msleep(CHARGER_INITIALIZED_DELAY_MS);
		}

		/* Return to G3 if battery level is too low */
		if (charge_want_shutdown() ||
		    tries > CHARGER_INITIALIZED_TRIES) {
			CPRINTS("power-up inhibited");
			chipset_force_shutdown();
			return POWER_G3;
		}

		if (power_wait_signals(IN_PCH_SLP_SUS_DEASSERTED)) {
			chipset_force_shutdown();
			return POWER_G3;
		}

		power_s5_up = 1;
		return POWER_S5;

	case POWER_S5S3:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S5G3;
		}

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_STARTUP);

#ifdef CONFIG_POWER_S0IX
		/*
		 * Clearing the S0ix flag on the path to S0
		 * to handle any reset conditions.
		 */
		power_reset_host_sleep_state(HOST_SLEEP_EVENT_S0IX_RESUME);
#endif
		return POWER_S3;

	case POWER_S3S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S3S5;
		}

		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 1);

		/* Enable wireless */
		wireless_set_state(WIRELESS_ON);

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		/*
		 * Throttle CPU if necessary.  This should only be asserted
		 * when +VCCP is powered (it is by now).
		 */
		gpio_set_level(GPIO_CPU_PROCHOT, 0);

		return POWER_S0;

	case POWER_S0S3:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 0);

		/* Suspend wireless */
		wireless_set_state(WIRELESS_SUSPEND);

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S3 or lower.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

#ifdef CONFIG_POWER_S0IX
		/* re-init S0ix flag */
		power_reset_host_sleep_state(HOST_SLEEP_EVENT_S0IX_RESUME);
#endif
		return POWER_S3;

#ifdef CONFIG_POWER_S0IX
	case POWER_S0S0ix:
		/* call hooks before standby */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		lpc_enable_wake_mask_for_lid_open();

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S0ix.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S0ix;


	case POWER_S0ixS0:
		lpc_disable_wake_mask_for_lid_open();

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S0;
#endif

	case POWER_S3S5:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN);

		/* Disable wireless */
		wireless_set_state(WIRELESS_OFF);

		/* Always enter into S5 state. The S5 state is required to
		 * correctly handle global resets which have a bit of delay
		 * while the SLP_Sx_L signals are asserted then deasserted. */
		power_s5_up = 0;
		return POWER_S5;

	case POWER_S5G3:
		chipset_force_g3();
		return POWER_G3;

	default:
		break;
	}

	return state;
}

enum power_state power_handle_state(enum power_state state)
{
	enum power_state new_state;

	/* Process RSMRST_L state changes. */
	handle_rsmrst(state);

	new_state = _power_handle_state(state);

	/* Process SLP_SUS_L state changes after a new state is decided. */
	handle_slp_sus(new_state);

	return new_state;
}
