/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TI SN5S330 USB-C Power Path Controller */

/*
 * PP1 : Sourcing power path.
 * PP2 : Sinking power path.
 */

#include "common.h"
#include "console.h"
#include "driver/ppc/sn5s330.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static int read_reg(uint8_t port, int reg, int *regval)
{
	return i2c_read8(ppc_chips[port].i2c_port,
			 ppc_chips[port].i2c_addr,
			 reg,
			 regval);
}

static int write_reg(uint8_t port, int reg, int regval)
{
	return i2c_write8(ppc_chips[port].i2c_port,
			  ppc_chips[port].i2c_addr,
			  reg,
			  regval);
}

#ifdef CONFIG_CMD_PPC_DUMP
static int sn5s330_dump(int port)
{
	int i;
	int data;
	const int i2c_port = ppc_chips[port].i2c_port;
	const int i2c_addr = ppc_chips[port].i2c_addr;

	for (i = SN5S330_FUNC_SET1; i <= SN5S330_FUNC_SET12; i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("FUNC_SET%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_FUNC_SET1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_STATUS_REG1; i <= SN5S330_INT_STATUS_REG4; i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("INT_STATUS_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_STATUS_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_TRIP_RISE_REG1; i <= SN5S330_INT_TRIP_RISE_REG3;
	     i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("INT_TRIP_RISE_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_TRIP_RISE_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_TRIP_FALL_REG1; i <= SN5S330_INT_TRIP_FALL_REG3;
	     i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("INT_TRIP_FALL_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_TRIP_FALL_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_MASK_RISE_REG1; i <= SN5S330_INT_MASK_RISE_REG3;
	     i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("INT_MASK_RISE_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_MASK_RISE_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_MASK_FALL_REG1; i <= SN5S330_INT_MASK_FALL_REG3;
	     i++) {
		i2c_read8(i2c_port, i2c_addr, i, &data);
		ccprintf("INT_MASK_FALL_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_MASK_FALL_REG1 + 1,
			 i,
			 data);
	}

	return EC_SUCCESS;
}
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

static int get_func_set3(uint8_t port, int *regval)
{
	int status;

	status = read_reg(port, SN5S330_FUNC_SET3, regval);
	if (status)
		CPRINTS("Failed to read FUNC_SET3!");

	return status;
}

static int sn5s330_is_pp_fet_enabled(uint8_t port, enum sn5s330_pp_idx pp,
			     int *is_enabled)
{
	int pp_bit;
	int status;
	int regval;

	if (pp == SN5S330_PP1)
		pp_bit = SN5S330_PP1_EN;
	else if (pp == SN5S330_PP2)
		pp_bit = SN5S330_PP2_EN;
	else
		return EC_ERROR_INVAL;

	status = get_func_set3(port, &regval);
	if (status)
		return status;

	*is_enabled = !!(pp_bit & regval);

	return EC_SUCCESS;
}

static int sn5s330_pp_fet_enable(uint8_t port, enum sn5s330_pp_idx pp,
				 int enable)
{
	int regval;
	int status;
	int pp_bit;

	if (pp == SN5S330_PP1)
		pp_bit = SN5S330_PP1_EN;
	else if (pp == SN5S330_PP2)
		pp_bit = SN5S330_PP2_EN;
	else
		return EC_ERROR_INVAL;

	status = get_func_set3(port, &regval);
	if (status)
		return status;

	if (enable)
		regval |= pp_bit;
	else
		regval &= ~pp_bit;

	status = write_reg(port, SN5S330_FUNC_SET3, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET3!");
		return status;
	}

	return EC_SUCCESS;
}

static int sn5s330_init(int port)
{
	int regval;
	int status;
	int retries;
	int reg;
	const int i2c_port  = ppc_chips[port].i2c_port;
	const int i2c_addr = ppc_chips[port].i2c_addr;

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	/* Set the sourcing current limit value. */
	switch (CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) {
	case TYPEC_RP_3A0:
		/* Set current limit to ~3A. */
		regval = SN5S330_ILIM_3_06;
		break;

	case TYPEC_RP_1A5:
	default:
		/* Set current limit to ~1.5A. */
		regval = SN5S330_ILIM_1_62;
		break;
	}
#else /* !defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */
	/* Default SRC current limit to ~1.5A. */
	regval = SN5S330_ILIM_1_62;
#endif /* defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */

	/*
	 * It seems that sometimes setting the FUNC_SET1 register fails
	 * initially.  Therefore, we'll retry a couple of times.
	 */
	retries = 0;
	do {
		status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET1,
				    regval);
		if (status) {
			CPRINTS("Failed to set FUNC_SET1! Retrying...");
			retries++;
			msleep(1);
		} else {
			break;
		}
	} while (retries < 10);

	/* Set Vbus OVP threshold to ~22.325V. */
	regval = 0x37;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET5, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET5!");
		return status;
	}

	/* Set Vbus UVP threshold to ~2.75V. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET6, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET6!");
		return status;
	}
	regval &= ~0x3F;
	regval |= 1;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET6, regval);
	if (status) {
		CPRINTS("Failed to write FUNC_SET6!");
		return status;
	}

	/* Enable SBU Fets and set PP2 current limit to ~3A. */
	regval = SN5S330_SBU_EN | 0xf;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET2, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET2!");
		return status;
	}

	/* TODO(aaboagye): What about Vconn? */

	/*
	 * Indicate we are using PP2 configuration 2 and enable OVP comparator
	 * for CC lines.
	 */
	regval = SN5S330_OVP_EN_CC | SN5S330_PP2_CONFIG;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET9, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET9!");
		return status;
	}

	/* Set analog current limit delay to 200 us for both PP1 & PP2. */
	regval = (PPX_ILIM_DEGLITCH_0_US_200 << 3) | PPX_ILIM_DEGLITCH_0_US_200;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET11,
			    regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET11");
		return status;
	}

	/* Turn off dead battery resistors and turn on CC FETs. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET4, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET4!");
		return status;
	}
	regval |= SN5S330_CC_EN;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET4, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET4!");
		return status;
	}

	/* Set ideal diode mode for both PP1 and PP2. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET3, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET3!");
		return status;
	}
	regval |= SN5S330_SET_RCP_MODE_PP1 | SN5S330_SET_RCP_MODE_PP2;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET3, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET3!");
		return status;
	}

	/* Turn off PP1 FET. */
	status = sn5s330_pp_fet_enable(port, SN5S330_PP1, 0);
	if (status) {
		CPRINTS("Failed to turn off PP1 FET!");
	}

	/*
	 * Don't proceed with the rest of initialization if we're sysjumping.
	 * We would have already done this before.
	 */
	if (system_jumped_to_this_image())
		return EC_SUCCESS;

	/* Clear the digital reset bit. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_INT_STATUS_REG4,
			   &regval);
	if (status) {
		CPRINTS("Failed to read INT_STATUS_REG4!");
		return status;
	}
	regval |= SN5S330_DIG_RES;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_STATUS_REG4,
			    regval);
	if (status) {
		CPRINTS("Failed to write INT_STATUS_REG4!");
		return status;
	}

	/*
	 * Before turning on the PP2 FET, let's mask off all interrupts except
	 * for the PP1 overcurrent condition and then clear all pending
	 * interrupts.
	 *
	 * TODO(aaboagye): Unmask fast-role swap events once fast-role swap is
	 * implemented in the PD stack.
	 */

	regval = ~SN5S330_ILIM_PP1_MASK;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_RISE_REG1,
			    regval);
	if (status) {
		CPRINTS("Failed to write INT_MASK_RISE1!");
		return status;
	}

	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_FALL_REG1,
			    regval);
	if (status) {
		CPRINTS("Failed to write INT_MASK_FALL1!");
		return status;
	}

	/* Now mask all the other interrupts. */
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_RISE_REG2,
			    0xFF);
	if (status) {
		CPRINTS("Failed to write INT_MASK_RISE2!");
		return status;
	}

	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_FALL_REG2,
			    0xFF);
	if (status) {
		CPRINTS("Failed to write INT_MASK_FALL2!");
		return status;
	}

	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_RISE_REG3,
			    0xFF);
	if (status) {
		CPRINTS("Failed to write INT_MASK_RISE3!");
		return status;
	}

	status = i2c_write8(i2c_port, i2c_addr, SN5S330_INT_MASK_FALL_REG3,
			    0xFF);
	if (status) {
		CPRINTS("Failed to write INT_MASK_FALL3!");
		return status;
	}

	/* Now clear any pending interrupts. */
	for (reg = SN5S330_INT_TRIP_RISE_REG1;
	     reg <= SN5S330_INT_TRIP_FALL_REG3;
	     reg++) {
		status = i2c_write8(i2c_port, i2c_addr, reg, 0xFF);
		if (status) {
			CPRINTS("Failed to write reg 0x%2x!");
			return status;
		}
	}


	/*
	 * For PP2, check to see if we booted in dead battery mode.  If we
	 * booted in dead battery mode, the PP2 FET will already be enabled.
	 */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_INT_STATUS_REG4,
			   &regval);
	if (status) {
		CPRINTS("Failed to read INT_STATUS_REG4!");
		return status;
	}

	if (regval & SN5S330_DB_BOOT) {
		/* Clear the bit. */
		i2c_write8(i2c_port, i2c_addr, SN5S330_INT_STATUS_REG4,
			   SN5S330_DB_BOOT);

		/* Turn on PP2 FET. */
		status = sn5s330_pp_fet_enable(port, SN5S330_PP2, 1);
		if (status) {
			CPRINTS("Failed to turn on PP2 FET!");
			return status;
		}
	}

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
static int sn5s330_is_vbus_present(int port, int *vbus_present)
{
	int regval;
	int rv;

	rv = read_reg(port, SN5S330_INT_STATUS_REG3, &regval);
	if (!rv)
		*vbus_present = !!(regval & SN5S330_VBUS_GOOD);

	return rv;
}
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

static int sn5s330_is_sourcing_vbus(int port)
{
	int is_sourcing_vbus = 0;
	int rv;

	rv = sn5s330_is_pp_fet_enabled(port, SN5S330_PP1, &is_sourcing_vbus);
	if (rv) {
		CPRINTS("C%d: Failed to determine source FET status! (%d)",
			port, rv);
		return 0;
	}

	return is_sourcing_vbus;
}

static int sn5s330_vbus_sink_enable(int port, int enable)
{
	return sn5s330_pp_fet_enable(port, SN5S330_PP2, !!enable);
}

static int sn5s330_vbus_source_enable(int port, int enable)
{
	return sn5s330_pp_fet_enable(port, SN5S330_PP1, !!enable);
}

const struct ppc_drv sn5s330_drv = {
	.init = &sn5s330_init,
	.is_sourcing_vbus = &sn5s330_is_sourcing_vbus,
	.vbus_sink_enable = &sn5s330_vbus_sink_enable,
	.vbus_source_enable = &sn5s330_vbus_source_enable,
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &sn5s330_dump,
#endif /* defined(CONFIG_CMD_PPC_DUMP) */
#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &sn5s330_is_vbus_present,
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */
};
