/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"

/* Battery info for proto */
static const struct battery_info info = {
#ifdef BOARD_ZOOMBINI
	.voltage_max		= 13200,
	.voltage_normal		= 11250,
	.voltage_min		= 9000,
	.precharge_current	= 189,
	.start_charging_min_c	= 0,
	.start_charging_max_c	= 60,
	.charging_min_c		= 0,
	.charging_max_c		= 60,
	.discharging_min_c	= -20,
	.discharging_max_c	= 60,
#else /* !defined(BOARD_ZOOMBINI) */
	/* Meowth battery info. */
#if 0 /* planned pack settings */
	.voltage_max		= 8780,
	.voltage_normal		= 7700,
	.voltage_min		= 6000,
	.precharge_current	= 160,
	.start_charging_min_c	= 0,
	.start_charging_max_c	= 45,
	.charging_min_c		= 0,
	.charging_max_c		= 45,
	.discharging_min_c	= -20,
	.discharging_max_c	= 60,
#endif /* 0 */
	/* Borrowed eve batteries for the time being. */
	.voltage_max		= TARGET_WITH_MARGIN(8800, 5), /* mV */
	.voltage_normal		= 7700,
	.voltage_min		= 6100, /* Add 100mV for charger accuracy */
	.precharge_current	= 256,	/* mA */
	.start_charging_min_c	= 0,
	.start_charging_max_c	= 46,
	.charging_min_c		= 10,
	.charging_max_c		= 50,
	.discharging_min_c	= 0,
	.discharging_max_c	= 60,
#endif /* defined(BOARD_ZOOMBINI) */
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}
