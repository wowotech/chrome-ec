/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* eSPI module for Chrome EC */

#ifndef __CROS_EC_ESPI_H
#define __CROS_EC_ESPI_H

#include "gpio_signal.h"

/* Signal through VW */
enum espi_vw_signal {
	VW_SIGNAL_BASE = GPIO_COUNT,
	VW_SLP_S3_L,			/* index 02h (In)  */
	VW_SLP_S4_L,
	VW_SLP_S5_L,
	VW_SUS_STAT_L,			/* index 03h (In)  */
	VW_PLTRST_L,
	VW_OOB_RST_WARN,
	VW_OOB_RST_ACK,			/* index 04h (Out) */
	VW_WAKE_L,
	VW_PME_L,
	VW_ERROR_FATAL,			/* index 05h (Out) */
	VW_ERROR_NON_FATAL,
	/* Merge bit 3/0 into one signal. Need to set them simultaneously */
	VW_SLAVE_BTLD_STATUS_DONE,
	VW_SCI_L,			/* index 06h (Out) */
	VW_SMI_L,
	VW_RCIN_L,
	VW_HOST_RST_ACK,
	VW_HOST_RST_WARN,		/* index 07h (In)  */
	VW_SUS_ACK,			/* index 40h (Out) */
	VW_SUS_WARN_L,			/* index 41h (In)  */
	VW_SUS_PWRDN_ACK_L,
	VW_SLP_A_L,
	VW_SLP_LAN,                     /* index 42h (In)  */
	VW_SLP_WLAN,
	VW_SIGNAL_BASE_END,
};

/**
 * Set eSPI Virtual-Wire signal to Host
 *
 * @param signal vw signal needs to set
 * @param level  level of vw signal
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_set_wire(enum espi_vw_signal signal, uint8_t level);

/**
 * Get eSPI Virtual-Wire signal from host
 *
 * @param signal vw signal needs to get
 * @return      1: set by host, otherwise: no signal
 */
int espi_vw_get_wire(enum espi_vw_signal signal);

/**
 * Enable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to enable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_enable_wire_int(enum espi_vw_signal signal);

/**
 * Disable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to disable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_disable_wire_int(enum espi_vw_signal signal);

#endif  /* __CROS_EC_ESPI_H */
