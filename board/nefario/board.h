/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Configuration for Nefario */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Optional modules */
#define CONFIG_ADC
#define CONFIG_CHIPSET_RK3399
#define CONFIG_CMD_RTC
#define CONFIG_HOSTCMD_RTC
#define CONFIG_HOSTCMD_SPS
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_I2C_VIRTUAL_BATTERY
#define CONFIG_I2C_PASSTHRU_RESTRICTED
#define CONFIG_LED_COMMON
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_POWER_COMMON
#define CONFIG_PWM
#define CONFIG_PWM_DISPLIGHT
#define CONFIG_SPI
#define CONFIG_SPI_MASTER
#define CONFIG_SPI_FLASH_GD25LQ40
#define CONFIG_SPI_FLASH_REGS

#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands for testing */

/*
 * We are code space-constrained on nefario, so take 10K that is normally used
 * as data RAM (was 30K, now 22K) and use it for code RAM (was 96K, now 104K)
 */
#define RAM_SHIFT_SIZE (8 * 1024)
#undef  CONFIG_RO_SIZE
#define CONFIG_RO_SIZE (NPCX_PROGRAM_MEMORY_SIZE + RAM_SHIFT_SIZE)
#undef  CONFIG_RAM_BASE
#define CONFIG_RAM_BASE (0x200C0000 + RAM_SHIFT_SIZE)
#undef  CONFIG_RAM_SIZE
#define CONFIG_RAM_SIZE (0x00008000 - 0x800 - RAM_SHIFT_SIZE)
/* Region sizes are no longer a power of 2 so we can't enable MPU */
#undef  CONFIG_MPU

/* Enable a different power-on sequence than the one on gru */
#undef CONFIG_CHIPSET_POWER_SEQ_VERSION
#define CONFIG_CHIPSET_POWER_SEQ_VERSION 3

/* Optional features */
#define CONFIG_BOARD_VERSION
#define CONFIG_BOARD_SPECIFIC_VERSION
#define CONFIG_FLASH_SIZE          0x00080000 /* 512KB spi flash */
#define CONFIG_HOST_COMMAND_STATUS
/* By default, set hcdebug to off */
#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF
#define CONFIG_KEYBOARD_BOARD_CONFIG
#define CONFIG_KEYBOARD_COL2_INVERTED
#define CONFIG_KEYBOARD_PROTOCOL_MKBP /* Instead of 8042 protocol of keyboard */
#define CONFIG_KEYBOARD_PWRBTN_ASSERTS_KSI2
#define CONFIG_LTO
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_TRACK_HOST_SLEEP_STATE
#define CONFIG_SOFTWARE_PANIC
#define CONFIG_VBOOT_HASH
#define CONFIG_VOLUME_BUTTONS

#define CONFIG_CHARGER
#define CONFIG_CHARGER_BD9995X
#define CONFIG_CHARGER_INPUT_CURRENT 512
#define CONFIG_CHARGER_MAINTAIN_VBAT
#define CONFIG_CHARGER_V2
#define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON 2
#define CONFIG_CHARGER_LIMIT_POWER_THRESH_BAT_PCT 2
#define CONFIG_CHARGER_LIMIT_POWER_THRESH_CHG_MW 15000
#define CONFIG_CHARGER_PROFILE_OVERRIDE
#define CONFIG_USB_CHARGER
#define CONFIG_USB_MUX_VIRTUAL

/* Increase tx buffer size, as we'd like to stream EC log to AP. */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* Motion Sensors */
#define CONFIG_ACCEL_BMA255
#define CONFIG_ACCEL_KX022
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT TASK_EVENT_CUSTOM(4)
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_INVALID_CHECK
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE    BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID     LID_ACCEL
/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO 256
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO / 3)

/*
 * Sensor internal FIFO is enabled for BMI160, but not for BMA255.
 */
#define CONFIG_ACCEL_FORCE_MODE_MASK (1 << LID_ACCEL)

/* USB PD config */
#define CONFIG_CHARGE_MANAGER
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DISCHARGE
#define CONFIG_USB_PD_DISCHARGE_GPIO
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_PORT_COUNT 2
#define CONFIG_USB_PD_TCPM_FUSB302
#define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0
#define CONFIG_USB_PD_VBUS_DETECT_CHARGER
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_USB_PD_COMM_LOCKED

#define CONFIG_BATTERY_CUT_OFF
#define CONFIG_BATTERY_PRESENT_GPIO GPIO_EC_BATT_PRES_L
#define CONFIG_BATTERY_REVIVE_DISCONNECT
#define CONFIG_BATTERY_SMART

#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     20000

#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 50000  /* us */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

/* Optional features for test commands */
#define CONFIG_CMD_CHARGER_PSYS

/* Set PSYS gain for 50W max measurement */
#define BD9995X_PSYS_GAIN_SELECT \
		BD9995X_CMD_PMON_IOUT_CTRL_SET_PMON_GAIN_SET_08UAW

#define CONFIG_UART_HOST                0

/* Optional feature - used by nuvoton */
#define NPCX_UART_MODULE2    1 /* 0:GPIO10/11 1:GPIO64/65 as UART */
#define NPCX_JTAG_MODULE2    0 /* 0:GPIO21/17/16/20 1:GPIOD5/E2/D4/E5 as JTAG*/
#define NPCX_TACH_SEL2       0 /* 0:GPIO40/A4 1:GPIO93/D3 as TACH */
/* Enable SHI PU on transition to S0. Disable the PU otherwise for leakage. */
#define NPCX_SHI_CS_PU
#define NPCX_SHI_BYPASS_OVER_256B

/* Optional for testing */
#undef  CONFIG_PECI
#undef  CONFIG_PSTORE

/* Modules we want to exclude */
#undef CONFIG_CMD_BATTFAKE
#undef CONFIG_CMD_CHARGER_PSYS
#undef CONFIG_CMD_FLASH
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_MD
#undef CONFIG_CMD_POWERINDEBUG
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CONSOLE_CMDHELP
#undef CONFIG_CONSOLE_HISTORY
/*
 * Remove task profiling to improve SHI interrupt latency.
 * TODO(crosbug.com/p/55710): Re-define once interrupt latency is within
 * tolerance.
 */
#undef CONFIG_TASK_PROFILING

#define I2C_PORT_TCPC0    NPCX_I2C_PORT0_0
#define I2C_PORT_TCPC1    NPCX_I2C_PORT0_1
#define I2C_PORT_ACCEL    NPCX_I2C_PORT1
#define I2C_PORT_CHARGER  NPCX_I2C_PORT2
#define I2C_PORT_BATTERY  NPCX_I2C_PORT3
#define I2C_PORT_VIRTUAL_BATTERY I2C_PORT_BATTERY

/* Enable Accel over SPI */
#define CONFIG_SPI_ACCEL_PORT    0  /* SPI master port (SPIP) form BMI160 */

#define CONFIG_MKBP_EVENT
/* Define the MKBP events which are allowed to wakeup AP in S3. */
#define CONFIG_MKBP_WAKEUP_MASK \
		(EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN) |\
		 EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON) |\
		 EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEY_PRESSED) |\
		 EC_HOST_EVENT_MASK(EC_HOST_EVENT_RTC))

#ifndef __ASSEMBLER__

enum adc_channel {
	/* Real ADC channels begin here */
	ADC_BOARD_ID = 0,
	ADC_PP900_AP,
	ADC_PPVAR_BIGCPU,
	ADC_PPVAR_CLOGIC,
	ADC_PPVAR_LOGIC,
	ADC_CH_COUNT
};

enum pwm_channel {
/* don't change the order or add anything between, this is ABI to kernel dts! */
	PWM_CH_LED_RED,
	PWM_CH_DISPLIGHT,
	PWM_CH_LED_GREEN,
	/* Number of PWM channels */
	PWM_CH_COUNT
};

/* power signal definitions */
enum power_signal {
	PP5000_PWR_GOOD = 0,
	SYS_PWR_GOOD,
	AP_PWR_GOOD,
	SUSPEND_DEASSERTED,

	/* Number of signals */
	POWER_SIGNAL_COUNT,
};

/* Motion sensors */
enum sensor_id {
	BASE_ACCEL = 0,
	BASE_GYRO,
	LID_ACCEL,
};

#include "gpio_signal.h"
#include "registers.h"

void board_reset_pd_mcu(void);
int board_get_version(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
