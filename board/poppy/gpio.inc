/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(USB_C0_PD_INT_ODL,	PIN(3, 7), GPIO_INT_FALLING,			tcpc_alert_event)
GPIO_INT(USB_C1_PD_INT_ODL,	PIN(C, 5), GPIO_INT_FALLING,			tcpc_alert_event)
/* Use VW signals instead of GPIOs */
#ifndef CONFIG_ESPI_VW_SIGNALS
GPIO_INT(PCH_SLP_S3_L,		PIN(7, 3), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(PCH_SLP_S4_L,		PIN(8, 6), GPIO_INT_BOTH,			power_signal_interrupt)
#endif
GPIO_INT(PCH_SLP_SUS_L,		PIN(6, 2), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(RSMRST_L_PGOOD,	PIN(B, 0), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(PMIC_DPWROK,		PIN(C, 7), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(POWER_BUTTON_L,	PIN(0, 4), GPIO_INT_BOTH | GPIO_PULL_UP,	power_button_interrupt)
GPIO_INT(LID_OPEN,		PIN(6, 7), GPIO_INT_BOTH,			lid_interrupt)
/* TODO(b/35585396): Make use of reverse dock signal. */
GPIO_INT(REVERSE_DOCK,		PIN(B, 7), GPIO_INT_BOTH,			lid_interrupt)
GPIO_INT(VOLUME_DOWN_L,		PIN(8, 3), GPIO_INT_BOTH | GPIO_PULL_UP,	button_interrupt)
GPIO_INT(VOLUME_UP_L,		PIN(8, 2), GPIO_INT_BOTH | GPIO_PULL_UP,	button_interrupt)
GPIO_INT(WP_L,			PIN(4, 0), GPIO_INT_BOTH,			switch_interrupt)
GPIO_INT(AC_PRESENT,		PIN(C, 1), GPIO_INT_BOTH,			extpower_interrupt)
GPIO_INT(USB_C0_VBUS_WAKE_L,	PIN(9, 3), GPIO_INT_BOTH | GPIO_PULL_UP,	vbus0_evt)
GPIO_INT(USB_C1_VBUS_WAKE_L,	PIN(9, 7), GPIO_INT_BOTH | GPIO_PULL_UP,	vbus1_evt)
GPIO_INT(USB_C0_BC12_INT_L,	PIN(D, 3), GPIO_INT_FALLING,			usb0_evt)
GPIO_INT(USB_C1_BC12_INT_L,	PIN(3, 3), GPIO_INT_FALLING,			usb1_evt)
/* TODO(b/35585396): Invert logic after rev1, then deprecate rev0 and remove. */
#ifdef POPPY_REV1
GPIO_INT(ACCELGYRO3_INT_L,	PIN(3, 6), GPIO_INT_FALLING,			bmi160_interrupt)
#else
GPIO_INT(ACCELGYRO3_INT_L,	PIN(3, 0), GPIO_INT_FALLING,			bmi160_interrupt)
#endif
GPIO_INT(BASE_DET_A,		PIN(4, 5), GPIO_INT_BOTH,			base_detect_interrupt)
GPIO_INT(USB_C0_CABLE_DET,      PIN(D, 2), GPIO_INT_RISING,			anx74xx_cable_det_interrupt)

GPIO(PCH_RTCRST,		PIN(E, 7), GPIO_OUT_LOW)			/* RTCRST# to SOC */
GPIO(ENABLE_BACKLIGHT,		PIN(2, 6), GPIO_OUT_LOW)			/* Enable Backlight */
GPIO(WLAN_OFF_L,		PIN(7, 2), GPIO_OUT_LOW)			/* Disable WLAN */
GPIO(PP3300_DX_WLAN,		PIN(A, 7), GPIO_OUT_LOW)			/* Enable WLAN 3.3V Power */
GPIO(CPU_PROCHOT,		PIN(8, 1), GPIO_OUT_HIGH)			/* PROCHOT# to SOC */
GPIO(PCH_ACOK,			PIN(5, 0), GPIO_ODR_LOW)			/* ACOK to SOC */
GPIO(PCH_WAKE_L,		PIN(A, 3), GPIO_ODR_HIGH)			/* Wake SOC */
GPIO(PCH_RSMRST_L,		PIN(7, 0), GPIO_OUT_LOW)			/* RSMRST# to SOC */
GPIO(PCH_PWRBTN_L,		PIN(4, 1), GPIO_ODR_HIGH)			/* Power Button to SOC */
GPIO(EC_PLATFORM_RST,		PIN(A, 6), GPIO_OUT_LOW)			/* EC Reset to LDO_EN */
GPIO(SYS_RESET_L,		PIN(6, 1), GPIO_ODR_HIGH)			/* Cold Reset to SOC */
GPIO(PMIC_SLP_SUS_L,		PIN(8, 5), GPIO_OUT_LOW)			/* SLP_SUS# to PMIC */
GPIO(BATTERY_PRESENT_L,		PIN(3, 4), GPIO_INPUT)				/* Battery Present */
GPIO(CCD_MODE_ODL,		PIN(6, 3), GPIO_INPUT)				/* Case Closed Debug Mode */
GPIO(EC_HAVEN_RESET_ODL,	PIN(0, 2), GPIO_ODR_HIGH)			/* H1 Reset */
GPIO(ENTERING_RW,		PIN(7, 6), GPIO_OUTPUT)				/* EC Entering RW */
GPIO(PMIC_INT_L,		PIN(6, 0), GPIO_INPUT)				/* PMIC interrupt */
GPIO(PCH_SLP_S0_L,		PIN(7, 5), GPIO_INPUT)

/* Sensor interrupts, not implemented yet */
GPIO(ALS_INT_L,			PIN(2, 5), GPIO_INPUT)
GPIO(FP_INT_L,			PIN(5, 6), GPIO_INPUT)

/* TODO(b/35585396): Make use of these GPIOs */
GPIO(PP1800_DX_SENSOR,		PIN(1, 4), GPIO_OUTPUT)
GPIO(PP3300_DX_SENSOR,		PIN(2, 1), GPIO_OUTPUT)
GPIO(PP3300_USB_PD,		PIN(2, 0), GPIO_OUTPUT)

#ifdef POPPY_REV0
GPIO(PP5000_DX_NFC,		PIN(1, 5), GPIO_OUTPUT)
GPIO(PP3300_DX_CAM,		PIN(1, 0), GPIO_OUT_HIGH)
GPIO(CAM_PMIC_RST_L,		PIN(0, 7), GPIO_INPUT)
#endif

GPIO(WLAN_PE_RST,		PIN(1, 2), GPIO_OUTPUT)
GPIO(PP3300_DX_LTE,		PIN(0, 5), GPIO_OUT_LOW)
GPIO(LTE_GPS_OFF_L,		PIN(0, 0), GPIO_OUTPUT)
GPIO(LTE_BODY_SAR_L,		PIN(0, 1), GPIO_OUTPUT)
GPIO(LTE_WAKE_L,		PIN(7, 1), GPIO_INPUT)
GPIO(LTE_OFF_ODL,		PIN(8, 0), GPIO_ODR_LOW)
/* end of TODO */

GPIO(PP3300_DX_BASE,		PIN(1, 1), GPIO_OUT_LOW)

/* I2C pins - these will be reconfigured for alternate function below */
GPIO(I2C0_0_SCL,		PIN(B, 5), GPIO_INPUT)				/* EC_I2C0_0_USBC_3V3_SCL */
GPIO(I2C0_0_SDA,		PIN(B, 4), GPIO_INPUT)				/* EC_I2C0_0_USBC_3V3_SDA */
GPIO(I2C0_1_SCL,		PIN(B, 3), GPIO_INPUT)				/* EC_I2C0_1_3V3_SCL */
GPIO(I2C0_1_SDA,		PIN(B, 2), GPIO_INPUT)				/* EC_I2C0_1_3V3_SDA */
GPIO(I2C1_SCL,			PIN(9, 0), GPIO_INPUT)				/* EC_I2C1_3V3_SCL */
GPIO(I2C1_SDA,			PIN(8, 7), GPIO_INPUT)				/* EC_I2C1_3V3_SDA */
GPIO(I2C2_SCL,			PIN(9, 2), GPIO_INPUT)				/* EC_I2C2_PMIC_3V3_SCL */
GPIO(I2C2_SDA,			PIN(9, 1), GPIO_INPUT)				/* EC_I2C2_PMIC_3V3_SDA */
GPIO(I2C3_SCL,			PIN(D, 1), GPIO_INPUT)				/* EC_I2C3_SENSOR_1V8_SCL */
GPIO(I2C3_SDA,			PIN(D, 0), GPIO_INPUT)				/* EC_I2C3_SENSOR_1V8_SDA */

/* rev0: 5V enables: INPUT=1.5A, OUT_LOW=OFF, OUT_HIGH=3A */
GPIO(USB_C0_5V_EN,		PIN(4, 2), GPIO_OUT_LOW)			/* C0 5V Enable */
GPIO(USB_C0_3A_EN,		PIN(6, 6), GPIO_OUT_LOW)			/* C0 Enable 3A */
GPIO(USB_C0_CHARGE_L,		PIN(C, 0), GPIO_OUT_LOW)			/* C0 Charge enable */
GPIO(USB_C1_5V_EN,		PIN(B, 1), GPIO_OUT_LOW)			/* C1 5V Enable */
GPIO(USB_C1_3A_EN,		PIN(3, 5), GPIO_OUT_LOW)			/* C1 3A Enable */
GPIO(USB_C1_CHARGE_L,		PIN(C, 3), GPIO_OUT_LOW)			/* C1 Charge enable */
GPIO(USB_C0_PD_RST_L,		PIN(0, 3), GPIO_OUT_LOW)			/* C0 PD Reset */
GPIO(USB_C1_PD_RST_L,		PIN(7, 4), GPIO_OUT_LOW)			/* C1 PD Reset */
GPIO(USB_C0_DP_HPD,		PIN(9, 4), GPIO_INPUT)				/* C0 DP Hotplug Detect */
GPIO(USB_C1_DP_HPD,		PIN(A, 5), GPIO_INPUT)				/* C1 DP Hotplug Detect */
GPIO(USB_C0_TCPC_PWR,		PIN(8, 4), GPIO_OUT_LOW)			/* Enable C0 TCPC Power */
GPIO(USB2_OTG_ID,		PIN(A, 1), GPIO_ODR_LOW)			/* OTG ID */
GPIO(USB2_OTG_VBUSSENSE,	PIN(9, 5), GPIO_OUT_LOW)			/* OTG VBUS Sense */

#ifdef BOARD_SORAKA
/* LEDs (2 colors on each port) */
/* TODO(b/35585396): Implement. */
GPIO(LED_YELLOW_C0,		PIN(3, 2), GPIO_OUT_LOW)
GPIO(LED_WHITE_C0,		PIN(C, 6), GPIO_OUT_LOW)
GPIO(LED_YELLOW_C1,		PIN(3, 1), GPIO_OUT_LOW)
GPIO(LED_WHITE_C1,		PIN(3, 0), GPIO_OUT_LOW)
#endif

/* Board ID */
GPIO(BOARD_VERSION1,		PIN(C, 4), GPIO_INPUT)				/* Board ID bit0 */
GPIO(BOARD_VERSION2,		PIN(C, 2), GPIO_INPUT)				/* Board ID bit1 */
GPIO(BOARD_VERSION3,		PIN(1, 3), GPIO_INPUT)				/* Board ID bit2 */

/* Alternate functions GPIO definitions */
ALTERNATE(PIN_MASK(6, 0x30), 1, MODULE_UART, 0)		/* GPIO64-65 */		/* UART from EC to Servo */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, 0)		/* GPIO87 */		/* EC_I2C1_3V3_SDA */
ALTERNATE(PIN_MASK(9, 0x01), 1, MODULE_I2C, 0)		/* GPIO90 */		/* EC_I2C1_3V3_SCL */
ALTERNATE(PIN_MASK(9, 0x06), 1, MODULE_I2C, 0)		/* GPIO91-92 */		/* EC_I2C2_PMIC_3V3_SDA/SCL */
ALTERNATE(PIN_MASK(B, 0x30), 1, MODULE_I2C, 0)		/* GPIOB4-B5 */		/* EC_I2C0_0_USBC_3V3_SDA/SCL */
ALTERNATE(PIN_MASK(B, 0x0C), 1, MODULE_I2C, 0)		/* GPOPB2-B3 */		/* EC_I2C0_1_3V3_SDA/SCL */
ALTERNATE(PIN_MASK(D, 0x03), 1, MODULE_I2C, 0)		/* GPIOD0-D1 */		/* EC_I2C3_SENSOR_1V8_SDA/SCL */