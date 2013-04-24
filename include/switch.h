/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Switch module for Chrome EC */

#ifndef __CROS_EC_SWITCH_H
#define __CROS_EC_SWITCH_H

#include "common.h"
#include "gpio.h"

#ifdef HAS_TASK_SWITCH
/**
 * Interrupt handler for switch inputs.
 *
 * @param signal	Signal which triggered the interrupt.
 */
void switch_interrupt(enum gpio_signal signal);
#else
#define switch_interrupt NULL
#endif  /* HAS_TASK_SWITCH */

/**
 * Return non-zero if write protect signal is asserted.
 */
int switch_get_write_protect(void);

#endif  /* __CROS_EC_SWITCH_H */
