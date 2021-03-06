/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Verify and jump to a RW image if power supply is not sufficient.
 */

#include "battery.h"
#include "charge_manager.h"
#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "rsa.h"
#include "rwsig.h"
#include "sha256.h"
#include "shared_mem.h"
#include "system.h"
#include "usb_pd.h"
#include "vboot.h"
#include "vb21_struct.h"

#define CPRINTS(format, args...) cprints(CC_VBOOT,"VB " format, ## args)
#define CPRINTF(format, args...) cprintf(CC_VBOOT,"VB " format, ## args)

static int has_matrix_keyboard(void)
{
	return 0;
}

static int is_efs_supported(void)
{
#ifdef CONFIG_VBOOT_EFS
	return 1;
#else
	return 0;
#endif
}

static int is_low_power_ap_boot_supported(void)
{
	return 0;
}

static int verify_slot(enum system_image_copy_t slot)
{
	const struct vb21_packed_key *vb21_key;
	const struct vb21_signature *vb21_sig;
	const struct rsa_public_key *key;
	const uint8_t *sig;
	const uint8_t *data;
	int len;
	int rv;

	CPRINTS("Verifying %s", system_image_copy_t_to_string(slot));

	vb21_key = (const struct vb21_packed_key *)(
			CONFIG_MAPPED_STORAGE_BASE +
			CONFIG_EC_PROTECTED_STORAGE_OFF +
			CONFIG_RO_PUBKEY_STORAGE_OFF);
	rv = vb21_is_packed_key_valid(vb21_key);
	if (rv) {
		CPRINTS("Invalid key (%d)", rv);
		return EC_ERROR_VBOOT_KEY;
	}
	key = (const struct rsa_public_key *)
		((const uint8_t *)vb21_key + vb21_key->key_offset);

	if (slot == SYSTEM_IMAGE_RW_A) {
		data = (const uint8_t *)(CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_A_STORAGE_OFF);
		vb21_sig = (const struct vb21_signature *)(
				CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_A_SIGN_STORAGE_OFF);
	} else {
		data = (const uint8_t *)(CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_B_STORAGE_OFF);
		vb21_sig = (const struct vb21_signature *)(
				CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_B_SIGN_STORAGE_OFF);
	}

	rv = vb21_is_signature_valid(vb21_sig, vb21_key);
	if (rv) {
		CPRINTS("Invalid signature (%d)", rv);
		return EC_ERROR_INVAL;
	}
	sig = (const uint8_t *)vb21_sig + vb21_sig->sig_offset;
	len = vb21_sig->data_size;

	if (vboot_is_padding_valid(data, len,
				   CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE)) {
		CPRINTS("Invalid padding");
		return EC_ERROR_INVAL;
	}

	rv = vboot_verify(data, len, key, sig);
	if (rv) {
		CPRINTS("Invalid data (%d)", rv);
		return EC_ERROR_INVAL;
	}

	CPRINTS("Verified %s", system_image_copy_t_to_string(slot));

	return EC_SUCCESS;
}

static int verify_and_jump(void)
{
	enum system_image_copy_t slot;
	int rv;

	/* 1. Decide which slot to try */
	slot = system_get_active_copy();

	/* 2. Verify the slot */
	rv = verify_slot(slot);
	if (rv) {
		if (rv == EC_ERROR_VBOOT_KEY)
			/* Key error. The other slot isn't worth trying. */
			return rv;
		slot = system_get_update_copy();
		/* TODO(chromium:767050): Skip reading key again. */
		rv = verify_slot(slot);
		if (rv)
			/* Both slots failed */
			return rv;

		/* Proceed with the other slot. If this slot isn't expected, AP
		 * will catch it and request recovery after a few attempts. */
		if (system_set_active_copy(slot))
			CPRINTS("Failed to activate %s",
				system_image_copy_t_to_string(slot));
	}

	/* 3. Jump (and reboot) */
	rv = system_run_image_copy(slot);
	CPRINTS("Failed to jump (%d)", rv);

	return rv;
}

/* Request more power: charging battery or more powerful AC adapter */
static void request_power(void)
{
	CPRINTS("%s", __func__);
}

static void request_recovery(void)
{
	CPRINTS("%s", __func__);
	led_critical();
}

static int is_manual_recovery(void)
{
	return host_is_event_set(EC_HOST_EVENT_KEYBOARD_RECOVERY);
}

static void vboot_main(void);
DECLARE_DEFERRED(vboot_main);
static void vboot_main(void)
{
	const int check_charge_manager_frequency_usec = 10 * MSEC;
	int port = charge_manager_get_active_charge_port();

	if (port == CHARGE_PORT_NONE) {
		/* We loop here until charge manager is ready */
		hook_call_deferred(&vboot_main_data,
				   check_charge_manager_frequency_usec);
		return;
	}

	CPRINTS("Checking power");

	if (system_can_boot_ap()) {
		/*
		 * We are here for the two cases:
		 * 1. Booting on RO with a barrel jack adapter. We can continue
		 *    to boot AP with EC-RO. We'll jump later in softsync.
		 * 2. Booting on RW with a type-c charger. PD negotiation is
		 *    done and we can boot AP.
		 */
		CPRINTS("Got enough power");
		return;
	}

	if (system_is_in_rw() || !system_is_locked()) {
		/*
		 * If we're here, it means PD negotiation was attempted but
		 * we didn't get enough power to boot AP. This happens on RW
		 * or unlocked RO.
		 *
		 * This could be caused by a weak type-c charger. If that's
		 * the case, users need to plug a better charger. We could
		 * also be here because PD negotiation is still taking place.
		 * If so, we'll briefly show request power sign but it will
		 * be immediately corrected.
		 *
		 * We can also get here because we called system_can_boot_ap too
		 * early. Power will be requested but it should be cancelled by
		 * board_set_charge_limit as soon as a PD contract is made.
		 */
		request_power();
		return;
	}

	CPRINTS("Booting RO on weak battery/charger");

	if (is_manual_recovery()) {
		CPRINTS("Manual recovery requested");
		if (battery_is_present() || has_matrix_keyboard()) {
			request_power();
			return;
		}
		/* We don't request_power because we don't want to assume all
		 * devices support a non type-c charger. We open up a security
		 * hole by allowing EC-RO to do PD negotiation but attackers
		 * don't gain meaningful advantage on devices without a matrix
		 * keyboard */
		CPRINTS("Enable C%d PD comm", port);
		pd_comm_enable(port, 1);
		return;
	}

	if (!is_efs_supported()) {
		if (is_low_power_ap_boot_supported())
			/* If a device supports this feature, AP's boot power
			 * threshold should be set low. That will let EC-RO
			 * boot AP and softsync take care of RW verification. */
			return;
		request_power();
		return;
	}

	/* If successful, this won't return. */
	verify_and_jump();

	/* Failed to jump. Need recovery. */
	request_recovery();
}

DECLARE_HOOK(HOOK_INIT, vboot_main, HOOK_PRIO_DEFAULT);
