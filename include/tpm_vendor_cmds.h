/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __INCLUDE_TPM_VENDOR_CMDS_H
#define __INCLUDE_TPM_VENDOR_CMDS_H

/*
 * This file includes definitions of extended/vendor TPM2 commands and their
 * return codes. The definitions are shared between the embedded code and the
 * gsctool utility running on the host.
 */

/* Extension and vendor commands. */
enum vendor_cmd_cc {
	/* Original extension commands */
	EXTENSION_AES = 0,
	EXTENSION_HASH = 1,
	EXTENSION_RSA = 2,
	EXTENSION_ECC = 3,
	EXTENSION_FW_UPGRADE = 4,
	EXTENSION_HKDF = 5,
	EXTENSION_ECIES = 6,
	EXTENSION_POST_RESET = 7,

	LAST_EXTENSION_COMMAND = 15,

	/* Our TPMv2 vendor-specific command codes. 16 bits available. */
	VENDOR_CC_GET_LOCK = 16,
	VENDOR_CC_SET_LOCK = 17,
	VENDOR_CC_SYSINFO = 18,
	VENDOR_CC_IMMEDIATE_RESET = 19,
	VENDOR_CC_INVALIDATE_INACTIVE_RW = 20,
	VENDOR_CC_COMMIT_NVMEM = 21,

	/* A gap left for the deep sleep control command. */

	VENDOR_CC_REPORT_TPM_STATE = 23,
	VENDOR_CC_TURN_UPDATE_ON = 24,
	VENDOR_CC_GET_BOARD_ID = 25,
	VENDOR_CC_SET_BOARD_ID = 26,
	VENDOR_CC_U2F_APDU = 27,
	VENDOR_CC_POP_LOG_ENTRY = 28,
	VENDOR_CC_GET_REC_BTN = 29,
	VENDOR_CC_RMA_CHALLENGE_RESPONSE = 30,
	VENDOR_CC_CCD_PASSWORD = 31,
	VENDOR_CC_DISABLE_RMA = 32,
	VENDOR_CC_MANAGE_CCD_PWD = 33,

	LAST_VENDOR_COMMAND = 65535,
};

/* Error codes reported by extension and vendor commands. */
enum vendor_cmd_rc {
	/* EXTENSION_HASH error codes */
	/* Attempt to start a session on an active handle. */
	EXC_HASH_DUPLICATED_HANDLE = 1,
	EXC_HASH_TOO_MANY_HANDLES = 2,  /* No room to allocate a new context. */
	/* Continuation/finish on unknown context. */
	EXC_HASH_UNKNOWN_CONTEXT = 3,

	/* Our TPMv2 vendor-specific response codes. */
	VENDOR_RC_SUCCESS = 0,
	VENDOR_RC_BOGUS_ARGS = 1,
	VENDOR_RC_READ_FLASH_FAIL = 2,
	VENDOR_RC_WRITE_FLASH_FAIL = 3,
	VENDOR_RC_REQUEST_TOO_BIG = 4,
	VENDOR_RC_RESPONSE_TOO_BIG = 5,
	VENDOR_RC_INTERNAL_ERROR = 6,
	VENDOR_RC_NOT_ALLOWED = 7,
	/* Only 7 bits available; max is 127 */
	VENDOR_RC_NO_SUCH_COMMAND = 127,
};

/*
 * The TPMv2 Spec mandates that vendor-specific command codes have bit 29 set,
 * while bits 15-0 indicate the command. All other bits should be zero.
 * We will define one of those 16-bit command values for Cr50 purposes, and use
 * the subcommand_code in struct tpm_cmd_header to further distinguish the
 * desired operation.
 */
#define TPM_CC_VENDOR_BIT_MASK 0x20000000
#define VENDOR_CC_MASK         0x0000ffff
/* Our vendor-specific command codes go here */
#define TPM_CC_VENDOR_CR50         0x0000

/*
 * The TPM response code is all zero for success.
 * Errors are a little complicated:
 *
 *   Bits 31:12 must be zero.
 *
 *   Bit 11     S=0   Error
 *   Bit 10     T=1   Vendor defined response code
 *   Bit  9     r=0   reserved
 *   Bit  8     V=1   Conforms to TPMv2 spec
 *   Bit  7     F=0   Confirms to Table 14, Format-Zero Response Codes
 *   Bits 6:0   num   128 possible failure reasons
 */
#define VENDOR_RC_ERR 0x00000500


#endif /* __INCLUDE_TPM_VENDOR_CMDS_H */
