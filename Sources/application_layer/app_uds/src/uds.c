/**
  ******************************************************************************
  * @attention
  * Copyright (c) - All Rights Reserved.
  * Unauthorized copying of this file, via any medium is strictly prohibited.
  * Proprietary and confidential.
  ******************************************************************************
  * @file           : bms_app.c
  * @brief          : This file contains the common helper functions to encode and decode bytes.
  *
  * @author         : Adilahmed Bagwan 
  * @created        : 17-July-2023
  * @lastmodified   : 17-July-2023
  * ******************************************************************************
  * */

#include "application_layer/app_uds/inc/uds.h"
#include "middle_layer/communication_stack/inc/uds_can_transport.h"
#include "middle_layer/communication_stack/inc/uds_timer_lib.h"
#include "middle_layer/services/inc/service_iflash.h"
#include "core_layer/drivers/inc/rr_memory.h"
#include "application_layer/app_uds/inc/boot_metadata.h"
#include <assert.h>
#include "system_S32K144.h"
#include "application_layer/app_uds/inc/uds_bootloader_config.h"
#include "middle_layer/communication_stack/inc/uds_utils.h"

/* ---------------------------------------------------------------------------
 * Feature flags
 *
 * RR_TRNG_ENABLED  — set to 1 when R_RSIP is configured in FSP to replace the
 *                    timer-XOR entropy source in 0x27 seed generation.
 *
 * RR_ECDSA_VERIFY_ENABLED — set to 1 when RSIP ECDSA-P256 is configured.
 *   Implement firmware_ecdsa_verify() below and embed the product public key.
 *   When enabled, 0x37 RequestTransferExit with 64 signature bytes appended
 *   (r[32]||s[32], big-endian) is verified before accepting the image.
 * --------------------------------------------------------------------------- */
#ifndef RR_TRNG_ENABLED
#define RR_TRNG_ENABLED          0
#endif
#ifndef RR_ECDSA_VERIFY_ENABLED
#define RR_ECDSA_VERIFY_ENABLED  0
#endif
#define RR_ECDSA_SIG_LEN         (64U)   /* 32-byte r || 32-byte s */

#if RR_ECDSA_VERIFY_ENABLED
/* TODO: replace with the real ECDSA-P256 public key (uncompressed X||Y, 64 bytes BE). */
static const U8 s_ecdsa_pub_key[64] = { 0 };

/* Compute SHA-256 and call R_RSIP ECDSA-P256 verify.
 * Returns true if signature is valid.
 *
 * Integration steps (FSP 5.x):
 *   1. Enable RSIP module in FSP configuration.
 *   2. Call R_RSIP_Open() during system init.
 *   3. Replace the TODOs below with:
 *        r_rsip_err_t e = R_RSIP_SHA256_Compute((U8 *)fw_addr, fw_size, hash);
 *        e = R_RSIP_ECDSA_P256_Verify(s_ecdsa_pub_key, hash, sig);
 *        return (e == RSIP_ERR_PASS);                                          */
static BOOL firmware_ecdsa_verify(U32 fw_addr, U32 fw_size,
                                   const U8 *sig)
{
    U8 hash[32] = {0};
    (void)fw_addr; (void)fw_size; (void)sig; (void)hash;
    /* TODO: R_RSIP_SHA256_Compute + R_RSIP_ECDSA_P256_Verify */
    return false;   /* safe default: reject until implemented */
}
#endif /* RR_ECDSA_VERIFY_ENABLED */

static void ResetTransfer( void );

static UDSServer_t uds_srvr;

/* Programming fingerprint — written via 0x2E DID 0xF184, read via 0x22 */
#define UDS_FINGERPRINT_LEN  (16U)
static U8 uds_fingerprint[UDS_FINGERPRINT_LEN];

/* ECU software version string reported via 0x22 DID 0xF189 */
static const U8 uds_sw_version[] = "BL_V1.0.0";

typedef U8 (*UDSService)( UDSReq_t *r );
static inline void NoResponse(UDSReq_t *r) { r->send_len = 0; }

static U8 NegativeResponse (UDSReq_t *r, U8 response_code)
{
	r->send_buf[0] = UDS_NEGATIVE_SERVICE_ID;
	r->send_buf[1] = r->recv_buf[0];
	r->send_buf[2] = response_code;
    r->send_len = UDS_NEG_RESP_LEN;
	return response_code;
}

static U8 _0x10_DiagnosticSessionControl( UDSReq_t *r )
{
    if(r->recv_len < UDS_0X10_REQ_LEN)
    {
		return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
	}

	uds_srvr.sessionType = r->recv_buf[1] & 0x4F;
	
	switch (uds_srvr.sessionType)
	{
		case kDefaultSession:
		{
		  	uds_timer_lib_start_and_stop_timer(0,UDS_TIMER_TYPE_S3TIMER);
		  	uds_timer_lib_start_and_stop_timer(0,UDS_TIMER_TYPE_P2TIMER);
		}
		  	break;
		case kProgrammingSession:
		case kExtendedDiagnostic:
		{
		  	uds_timer_lib_start_and_stop_timer(1,UDS_TIMER_TYPE_S3TIMER);
		}
		  	break;
    	default:
		  break;	  
	}
	r->send_buf[r->send_len++] = UDS_RESPONSE_SID_OF(kSID_DIAGNOSTIC_SESSION_CONTROL);
	r->send_buf[r->send_len++] = uds_srvr.sessionType;
	// UDS-1-2013: Table 29
	// resolution: 1ms
	r->send_buf[r->send_len++] = UDS_CLIENT_DEFAULT_P2_MS >> 8;
	r->send_buf[r->send_len++]= UDS_CLIENT_DEFAULT_P2_MS;
	// resolution: 10ms
	r->send_buf[r->send_len++] = (UDS_CLIENT_DEFAULT_P2_STAR_MS / 10) >> 8;
	r->send_buf[r->send_len++] = UDS_CLIENT_DEFAULT_P2_STAR_MS / 10;
	

	return kPositiveResponse;
}

static U8 _0x11_ECUReset(  UDSReq_t *r )
{
    uds_srvr.ecuResetScheduled = r->recv_buf[1] & 0x3F;

    if (r->recv_len < UDS_0X11_REQ_MIN_LEN)
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }
    
	uds_srvr.notReadyToReceive = true;
	
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ECU_RESET);
    r->send_buf[1] = uds_srvr.ecuResetScheduled;
	
	uds_timer_lib_start_and_stop_timer(1,UDS_TIMER_TYPE_ECURESET);

    if (uds_srvr.ecuResetScheduled == kEnableRapidPowerShutDown ){
        U32 powerDownTime = UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS / 1000;
        if (powerDownTime > 255)
        {
            powerDownTime = 255;
        }
        r->send_buf[2] = (U8)powerDownTime;
        r->send_len= UDS_0X11_RESP_BASE_LEN + 1;
    }
    else
    {
        r->send_len = UDS_0X11_RESP_BASE_LEN;
    }
	
    return kPositiveResponse;
}

static U8 _0x28_CommunicationControl(UDSReq_t *r)
{
    U8 controlType = r->recv_buf[1] & 0x7F;
    U8 commType    = r->recv_buf[2] & 0x03u;

    if (r->recv_len < UDS_0X28_REQ_BASE_LEN)
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (commType == 0x01u || commType == 0x03u) 
    {   /* affects normal msgs */
        /* 0x01 enableRx+disableTx, 0x03 disableRx+disableTx -> stop normal Tx */
    }
    else if(commType == 0x02)
    {
        ResetTransfer();
    }
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_COMMUNICATION_CONTROL);
    r->send_buf[1] = controlType;
    r->send_len = UDS_0X28_RESP_LEN;
    return kPositiveResponse;
}

static U8 _0x22_ReadDataByIdentifier(UDSReq_t *r)
{
    U16 dataId;
    U8  i;

    if (r->recv_len < (U16)(UDS_0X22_RESP_BASE_LEN + 2U))
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    dataId = (U16)(((U16)r->recv_buf[1] << 8U) | (U16)r->recv_buf[2]);

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_DATA_BY_IDENTIFIER);
    r->send_buf[1] = r->recv_buf[1];
    r->send_buf[2] = r->recv_buf[2];
    r->send_len    = 3U;

    switch (dataId)
    {
        case 0xF189U: /* ECU Software Version Number */
        {
            for (i = 0U; i < (U8)sizeof(uds_sw_version); i++)
            {
                r->send_buf[r->send_len++] = uds_sw_version[i];
            }
            break;
        }
        case 0xF190U: /* Vehicle Identification Number (VIN) — 17 bytes */
        {
            for (i = 0U; i < 17U; i++)
            {
                r->send_buf[r->send_len++] = 0x00U;
            }
            break;
        }
        case 0xF184U: /* Programming Fingerprint */
        {
            for (i = 0U; i < UDS_FINGERPRINT_LEN; i++)
            {
                r->send_buf[r->send_len++] = uds_fingerprint[i];
            }
            break;
        }
        case 0xF18BU: /* Active application bank: 0 = Bank A, 1 = Bank B */
        {
            // r->send_buf[r->send_len++] = Read_ActiveSlotForApplication();
            break;
        }
        default:
        {
            return NegativeResponse(r, kRequestOutOfRange);
        }
    }
    return kPositiveResponse;
}

static U8 _0x2E_WriteDataByIdentifier(UDSReq_t *r)
{
    U16 dataId;
    U16 dataLen;
    U16 i;

    if (r->recv_len < UDS_0X2E_REQ_MIN_LEN)
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    dataId  = (U16)(((U16)r->recv_buf[1] << 8U) | (U16)r->recv_buf[2]);
    dataLen = (U16)(r->recv_len - UDS_0X2E_REQ_BASE_LEN);

    switch (dataId)
    {
        case 0xF184U: /* Programming Fingerprint (16 bytes) */
        {
            if (dataLen != UDS_FINGERPRINT_LEN)
            {
                return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
            }
            for (i = 0U; i < UDS_FINGERPRINT_LEN; i++)
            {
                uds_fingerprint[i] = r->recv_buf[3U + i];
            }
            break;
        }
        default:
        {
            return NegativeResponse(r, kRequestOutOfRange);
        }
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_WRITE_DATA_BY_IDENTIFIER);
    r->send_buf[1] = (U8)(dataId >> 8U);
    r->send_buf[2] = (U8)(dataId & 0x00FFU);
    r->send_len    = UDS_0X2E_RESP_LEN;
    return kPositiveResponse;
}

static U8 VerifyChecksum(U32 expected_crc32)
{
    /* Compare the expected CRC-32 supplied by the client via 0x31/0x0202 against
     * the CRC-32 finalized at RequestTransferExit. xfer_crc32_run itself has
     * already been reset to 0xFFFFFFFF by ResetTransfer() by the time this
     * routine runs, since VerifyChecksum always arrives as a separate request
     * after RequestTransferExit. */
    return (uds_srvr.xfer_crc32_final == expected_crc32) ? 1U : 0U;
}

static U8 _0x27_SecurityAccess(UDSReq_t *r)
{
    if (r->recv_len < UDS_0X27_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    U8 sub_fn = r->recv_buf[1] & 0x7FU;

    boot_metadata_t meta;
    if (!boot_metadata_read(&meta)) {
        /* Metadata not yet initialised / unreadable (e.g. blank flash): fall back to a
         * default record so security access still works.  stored_secret matches
         * boot_metadata_init()'s default — provision a per-unit secret in production. */
        (void)rr_memory_set_u8((U8 *)&meta, 0, sizeof(meta));
        meta.magic         = BOOT_METADATA_MAGIC;
        meta.stored_secret = 0xC3A5691EUL;
    }

    if (meta.sec_access_fail_count >= MAX_ERRORS_TIMES) {
        return NegativeResponse(r, kExceedNumberOfAttempts);
    }

    if (sub_fn == kRequestSeed) {
        /* Generate a 32-bit seed mixed with the device-specific secret.
         * When RR_TRNG_ENABLED=1 the RSIP hardware RNG is used (preferred).
         * Otherwise: XOR of two running UDS timers (weak but functional). */
#if RR_TRNG_ENABLED
        /* TODO: call R_RSIP_RandomNumberGenerate(g_rsip_ctrl, (U8 *)&raw, 4U); */
        U32 raw = 0U;  /* replace with RSIP RNG output */
#else
        U32 t1 = uds_timer_lib_get_timer_msec(UDS_TIMER_TYPE_S3TIMER);
        U32 t2 = uds_timer_lib_get_timer_msec(UDS_TIMER_TYPE_P2TIMER);
        U32 raw = (t1 ^ 0xA5B4C3D2UL) + (t2 * 0x6B5C4D3EUL);
#endif
        uds_srvr.current_seed = crc32_running(meta.stored_secret, (const U8 *)&raw, 4U);

        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS);
        r->send_buf[1] = sub_fn;
        r->send_buf[2] = (U8)(uds_srvr.current_seed >> 24);
        r->send_buf[3] = (U8)(uds_srvr.current_seed >> 16);
        r->send_buf[4] = (U8)(uds_srvr.current_seed >>  8);
        r->send_buf[5] = (U8)(uds_srvr.current_seed);
        r->send_len    = 6U;
        return kPositiveResponse;
    }

    if (sub_fn == kSendKey) {
        if (r->recv_len < (U16)(UDS_0X27_REQ_BASE_LEN + 4U)) {
            return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
        }

        /* Valid key = CRC-32 of the issued seed, seeded with the stored secret */
        U32 expected_key = crc32_compute((const U8 *)&uds_srvr.current_seed, 4U);
        expected_key ^= meta.stored_secret;

        U32 received_key = ((U32)r->recv_buf[2] << 24U)
                              | ((U32)r->recv_buf[3] << 16U)
                              | ((U32)r->recv_buf[4] <<  8U)
                              |  (U32)r->recv_buf[5];

        if (received_key == expected_key) {
            meta.sec_access_fail_count = 0U;
            boot_metadata_write(&meta);
            uds_srvr.securityLevel = sub_fn;
            r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_SECURITY_ACCESS);
            r->send_buf[1] = sub_fn;
            r->send_len    = 2U;
            return kPositiveResponse;
        }

        meta.sec_access_fail_count++;
        boot_metadata_write(&meta);
        return NegativeResponse(r, kInvalidKey);
    }

    return NegativeResponse(r, kSubFunctionNotSupported);
}

U32 size_test = 0;

static U8 _0x31_RoutineControl(UDSReq_t *r)
{
	U16 routineIdentifier;
	U8 routineControlType;
	
    if (r->recv_len < UDS_0X31_REQ_MIN_LEN)
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    routineControlType = r->recv_buf[1] & 0x7F;
    routineIdentifier = (U16)((r->recv_buf[2] << 8) + r->recv_buf[3]);
	
    switch (routineControlType)
    {
    case kStartRoutine:
    case kStopRoutine:
    case kRequestRoutineResults:
    {
		if(routineIdentifier == ROUTINE_ERASE_MEMORY){
			if (uds_srvr.sessionType != kProgrammingSession) {
				return NegativeResponse(r, kServiceNotSupportedInActiveSession);
			}
			if (uds_srvr.securityLevel == 0U) {
				return NegativeResponse(r, kSecurityAccessDenied);
			}
			U8 addressBytes = (r->recv_buf[4] >> 4) & 0x0FU;
    		U8 lengthBytes  = r->recv_buf[4] & 0x0FU;
			// Extract address based on the number of bytes
			U32 address = 0;
            U8 i = 0;
			for (i = 0; i < addressBytes; i++)
			{
				address = (address << 8) | r->recv_buf[5 + i];
			}
			// Extract length based on the number of bytes
			size_test = 0;
			for (i = 0; i < lengthBytes; i++)
			{
				size_test = (size_test << 8) | r->recv_buf[5 + addressBytes + i];
			}

            if (check_AddressRangeValid(address) == 0)
            {
            return NegativeResponse(r,kRequestOutOfRange);// Request out of range
            }
            if(EraseFlashMemory(address, 307456)!= COM_HDR_RET_OK)
            {
                return NegativeResponse(r, kConditionsNotCorrect);
            }

		}
		else if(routineIdentifier == ROUTINE_VERIFY_CHECKSUM)
		{
			/* Client supplies expected CRC-32 as 4 bytes (big-endian) */
			U32 expected_crc32 = ((U32)r->recv_buf[4] << 24U)
			                        | ((U32)r->recv_buf[5] << 16U)
			                        | ((U32)r->recv_buf[6] <<  8U)
			                        |  (U32)r->recv_buf[7];
			if(!VerifyChecksum(expected_crc32))
			{
				return NegativeResponse(r, kConditionsNotCorrect);
			}
		}
        else if (routineIdentifier == ROUTINE_SWAP_BANK) /* Assume 0xFF01 */
		{
			/* Ensure we are in an authorized session and have security access */
			if ((uds_srvr.sessionType != kExtendedDiagnostic) && (uds_srvr.sessionType != kProgrammingSession)) {
				return NegativeResponse(r, kServiceNotSupportedInActiveSession);
			}
			if (uds_srvr.securityLevel == 0U) {
				return NegativeResponse(r, kSecurityAccessDenied);
			}

			/* Call the helper to safely validate the other bank and swap */
			// if (!boot_metadata_swap_active_bank()) {
			// 	return NegativeResponse(r, kConditionsNotCorrect); /* Fails if empty or corrupted */
			// }
		}
		break;
	}

    default:
        return NegativeResponse(r, kRequestOutOfRange);
    }
    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_ROUTINE_CONTROL);
    r->send_buf[1] = routineControlType;
    r->send_buf[2] = (U8)(routineIdentifier >> 8);
    r->send_buf[3] = (U8)(routineIdentifier);
    r->send_buf[4] = 0x00;
    r->send_len = UDS_0X31_RESP_MIN_LEN+1;
	
    return kPositiveResponse;
}

static void ResetTransfer( void )
{
    uds_srvr.xferBlockSequenceCounter = 1;
    uds_srvr.xferByteCounter   = 0;
    uds_srvr.xferTotalBytes    = 0;
    uds_srvr.xferBlockAddress  = 0U;
    uds_srvr.xferIsActive      = false;
    uds_srvr.xferIsUpload      = false;
    uds_srvr.xfer_crc32_run    = 0xFFFFFFFFUL;
}


static U8 _0x34_RequestDownload( UDSReq_t *r)
{
    U8 addressBytes = (r->recv_buf[2] >> 4) & 0x0FU;
    U8 lengthBytes  = r->recv_buf[2] & 0x0FU;
    U8 i = 0;

    if (uds_srvr.sessionType != kProgrammingSession) {
        return NegativeResponse(r, kServiceNotSupportedInActiveSession);
    }
    if (uds_srvr.securityLevel == 0U) {
        return NegativeResponse(r, kSecurityAccessDenied);
    }
    if (uds_srvr.xferIsActive) {
        return NegativeResponse(r, kConditionsNotCorrect);
    }
    if (r->recv_len < UDS_0X34_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    ResetTransfer();
	for (i = 0; i < addressBytes; i++) {
		uds_srvr.xferBlockAddress = (uds_srvr.xferBlockAddress << 8) | r->recv_buf[3 + i];
	}
	if(uds_srvr.xferBlockAddress == ETX_APP_BASE_ADDRESS)
	{
		uds_srvr.is_active_slot = 0;
	}else{
		uds_srvr.is_active_slot = 1;
	}
	// Extract length based on the number of bytes
	for (i = 0; i < lengthBytes; i++) {
		uds_srvr.xferTotalBytes = (uds_srvr.xferTotalBytes << 8) | r->recv_buf[3 + addressBytes + i];
	}
    uds_srvr.xferIsActive = true;
    uds_srvr.xferBlockLength = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH;
    if (uds_srvr.xferBlockLength > UDS_TP_MTU) {
        uds_srvr.xferBlockLength = UDS_TP_MTU;
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_DOWNLOAD);
    r->send_buf[1] = (U8)(sizeof(uds_srvr.xferBlockLength) << 4);
    r->send_len = UDS_0X34_RESP_BASE_LEN;
	r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength & 0xFF000000)>> 24);
	r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength & 0x00FF0000)>> 16);
	r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength & 0x0000FF00)>> 8);
	r->send_buf[r->send_len++] = (U8)(uds_srvr.xferBlockLength & 0x000000FF);
    return kPositiveResponse;
}

static U8 _0x36_TransferData( UDSReq_t *r)
{
    U8  err = kPositiveResponse;
    U16 request_data_len = r->recv_len - UDS_0X36_REQ_BASE_LEN;
    U8  blockSequenceCounter = 0U;
    U8 *transferData = NULL;

    if (uds_srvr.sessionType != kProgrammingSession) {
        return NegativeResponse(r, kServiceNotSupportedInActiveSession);
    }
    if (uds_srvr.securityLevel == 0U) {
        return NegativeResponse(r, kSecurityAccessDenied);
    }
    if (!uds_srvr.xferIsActive)
    {
        return NegativeResponse(r, kUploadDownloadNotAccepted);
    }

    if (r->recv_len < UDS_0X36_REQ_BASE_LEN)
    {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    blockSequenceCounter = r->recv_buf[1];
	transferData = &r->recv_buf[2];

    if (!uds_srvr.RCRRP)
    {
        if (blockSequenceCounter != uds_srvr.xferBlockSequenceCounter)
        {
            err = kRequestSequenceError;
            goto fail;
        }
        else
        {
            uds_srvr.xferBlockSequenceCounter++;
        }
    }

    /* ── Upload path (0x35 direction): read flash → send to tester ────── */
    if (uds_srvr.xferIsUpload)
    {
        U32 remaining = (U32)(uds_srvr.xferTotalBytes - uds_srvr.xferByteCounter);
        U32 read_len  = (remaining < (U32)uds_srvr.xferBlockLength)
                             ? remaining : (U32)uds_srvr.xferBlockLength;

        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TRANSFER_DATA);
        r->send_buf[1] = blockSequenceCounter;
        r->send_len    = UDS_0X36_RESP_BASE_LEN;

        bool in_df = (uds_srvr.xferBlockAddress >= ETX_APP_DATA_ADDR);
        iflash_kind_te kind = in_df ? IFLASH_KIND_DFLASH : IFLASH_KIND_PFLASH;
        
        if (service_iflash_read(kind, uds_srvr.xferBlockAddress, &r->send_buf[2], read_len) != COM_HDR_RET_OK) {
            err = kConditionsNotCorrect;
            goto fail;
        }

        r->send_len               = (U16)(UDS_0X36_RESP_BASE_LEN + read_len);
        uds_srvr.xferBlockAddress += read_len;
        uds_srvr.xferByteCounter  += read_len;
        return kPositiveResponse;
    }

    /* ── Download path (0x34 direction): receive data → write to flash ── */
    if (uds_srvr.xferByteCounter + request_data_len > uds_srvr.xferTotalBytes)
    {
        err = kTransferDataSuspended;
        goto fail;
    }

    bool in_df_write = (uds_srvr.xferBlockAddress >= ETX_APP_DATA_ADDR);
    iflash_kind_te wr_kind = in_df_write ? IFLASH_KIND_DFLASH : IFLASH_KIND_PFLASH;

	if(rr_iflash_program(wr_kind, uds_srvr.xferBlockAddress, transferData, request_data_len) != COM_HDR_RET_OK)
	{
		err = kConditionsNotCorrect;
		goto fail;
	}

	/* Accumulate CRC-32 over every byte written to flash */
	uds_srvr.xfer_crc32_run = crc32_running(uds_srvr.xfer_crc32_run,
	                                         transferData, request_data_len);

	uds_srvr.xferBlockAddress += (request_data_len);
	uds_srvr.xferByteCounter  += request_data_len;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TRANSFER_DATA);
    r->send_buf[1] = blockSequenceCounter;
    r->send_len = UDS_0X36_RESP_BASE_LEN;

    return kPositiveResponse;


fail:
    ResetTransfer();
    return NegativeResponse(r, err);
}

static U8 _0x37_RequestTransferExit(UDSReq_t *r)
{
    if (uds_srvr.sessionType != kProgrammingSession) {
        return NegativeResponse(r, kServiceNotSupportedInActiveSession);
    }
    if (uds_srvr.securityLevel == 0U) {
        return NegativeResponse(r, kSecurityAccessDenied);
    }
    if (!uds_srvr.xferIsActive)
    {
        return NegativeResponse(r, kUploadDownloadNotAccepted);
    }

    /* Optional ECDSA-P256 firmware signature verification.
     * Client may append r[32]||s[32] (64 bytes, big-endian) to the 0x37 request.
     * Enable by setting RR_ECDSA_VERIFY_ENABLED=1 in project defines.          */
#if RR_ECDSA_VERIFY_ENABLED
    if (r->recv_len >= (U16)(UDS_0X37_REQ_BASE_LEN + RR_ECDSA_SIG_LEN))
    {
        U8  slot    = uds_srvr.is_active_slot & 0x01U;
        U32 fw_base = (slot == 0U) ? ETX_APP_BASE_ADDRESS : ETX_APP_BANK_B_ADDRESS;
        if (!firmware_ecdsa_verify(fw_base, (U32)uds_srvr.xferByteCounter,
                                   &r->recv_buf[UDS_0X37_REQ_BASE_LEN]))
        {
            ResetTransfer();
            return NegativeResponse(r, kGeneralProgrammingFailure);
        }
    }
#endif

    /* Finalise the CRC-32 before ResetTransfer() below wipes xfer_crc32_run.
     * xfer_crc32_final survives the reset so the subsequent VerifyChecksum
     * RoutineControl request can still check it. */
    U32 finalized_crc = ~uds_srvr.xfer_crc32_run;
    uds_srvr.xfer_crc32_final = finalized_crc;

    /* Store the finalised CRC-32 and size for this bank in boot metadata.
     * The bootloader will verify this on the next boot before jumping. */
    boot_metadata_t meta;
    if (boot_metadata_read(&meta)) {
        U8 slot = uds_srvr.is_active_slot & 0x01U;
        meta.fw_crc32   = finalized_crc;
        meta.fw_size    = (U32)uds_srvr.xferByteCounter;
        // meta.bank_valid[slot] = 0U;   /* validated by BL on next boot */
        boot_metadata_write(&meta);
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_TRANSFER_EXIT);
    r->send_len = UDS_0X37_RESP_BASE_LEN;

    ResetTransfer();
    PersistBootState(ETX_NORMAL_BOOT);
    return kPositiveResponse;
}

static U8 _0x3E_TesterPresent( UDSReq_t *r)
{
    if ((r->recv_len < UDS_0X3E_REQ_MIN_LEN) || (r->recv_len > UDS_0X3E_REQ_MAX_LEN))
    {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }
    U8 zeroSubFunction = r->recv_buf[1];

    switch (zeroSubFunction)
    {
    case 0x00 :
    {
	  	uds_timer_lib_start_and_stop_timer(1,UDS_TIMER_TYPE_S3TIMER);
        r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_TESTER_PRESENT);
        r->send_buf[1] = 0x00;
        r->send_len = UDS_0X3E_RESP_LEN;
        return kPositiveResponse;
		break;
    }
    case 0x80:
    {
        return kPositiveResponse;
		break;
    }
    default:
    {
        return NegativeResponse(r, kSubFunctionNotSupported);
		break;
    }
    }
}

static U8 _0x85_ControlDTCSetting(UDSReq_t *r) {
    if (r->recv_len < UDS_0X85_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }
    U8 dtcSettingType = r->recv_buf[1] & 0x3F;

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_CONTROL_DTC_SETTING);
    r->send_buf[1] = dtcSettingType;
    r->send_len = UDS_0X85_RESP_LEN;
    return kPositiveResponse;
}

static U8 _0x35_RequestUpload(UDSReq_t *r)
{
    U8  addrBytes;
    U8  lenBytes;
    U32 upload_addr = 0U;
    U32 upload_size = 0U;
    U8  i;

    if (uds_srvr.sessionType != kProgrammingSession) {
        return NegativeResponse(r, kServiceNotSupportedInActiveSession);
    }
    if (uds_srvr.securityLevel == 0U) {
        return NegativeResponse(r, kSecurityAccessDenied);
    }
    if (uds_srvr.xferIsActive) {
        return NegativeResponse(r, kConditionsNotCorrect);
    }
    if (r->recv_len < UDS_0X35_REQ_BASE_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    /* byte 1 = dataFormatIdentifier (ignored — no compression/encryption) */
    addrBytes = (r->recv_buf[2] >> 4) & 0x0FU;
    lenBytes  =  r->recv_buf[2] & 0x0FU;

    if ((addrBytes == 0U) || (addrBytes > 4U) || (lenBytes == 0U) || (lenBytes > 4U)) {
        return NegativeResponse(r, kRequestOutOfRange);
    }
    if (r->recv_len < (U16)(3U + addrBytes + lenBytes)) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (i = 0; i < addrBytes; i++) {
        upload_addr = (upload_addr << 8U) | r->recv_buf[3 + i];
    }
    for (i = 0; i < lenBytes; i++) {
        upload_size = (upload_size << 8U) | r->recv_buf[3 + addrBytes + i];
    }

    /* Restrict upload to application flash banks and data flash only */
    bool in_app = (upload_addr >= ETX_APP_BASE_ADDRESS) &&
                  ((upload_addr + upload_size) <= ETX_APP_BANK_A_END);
    bool in_df  = (upload_addr >= ETX_APP_DATA_ADDR) &&
                  ((upload_addr + upload_size) <= (ETX_APP_DATA_ADDR + ETX_MAX_DATA_SIZE_ERASE));
    if (!in_app && !in_df) {
        return NegativeResponse(r, kRequestOutOfRange);
    }

    ResetTransfer();
    uds_srvr.xferBlockAddress = upload_addr;
    uds_srvr.xferTotalBytes   = upload_size;
    uds_srvr.xferIsActive     = true;
    uds_srvr.xferIsUpload     = true;
    uds_srvr.xferBlockLength  = UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH;
    if (uds_srvr.xferBlockLength > UDS_TP_MTU) {
        uds_srvr.xferBlockLength = UDS_TP_MTU;
    }

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_REQUEST_UPLOAD);
    r->send_buf[1] = (U8)(sizeof(uds_srvr.xferBlockLength) << 4);
    r->send_len    = UDS_0X35_RESP_BASE_LEN;
    r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength >> 24) & 0xFFU);
    r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength >> 16) & 0xFFU);
    r->send_buf[r->send_len++] = (U8)((uds_srvr.xferBlockLength >>  8) & 0xFFU);
    r->send_buf[r->send_len++] = (U8) (uds_srvr.xferBlockLength & 0xFFU);
    return kPositiveResponse;
}

static U8 _0x23_ReadMemoryByAddress(UDSReq_t *r)
{
    U8  addrAndLenFmt;
    U8  addrLen;
    U8  sizeLen;
    U32 mem_addr = 0U;
    U32 mem_size = 0U;
    U8  i;

    if (uds_srvr.sessionType == kDefaultSession) {
        return NegativeResponse(r, kServiceNotSupportedInActiveSession);
    }
    if (uds_srvr.securityLevel == 0U) {
        return NegativeResponse(r, kSecurityAccessDenied);
    }
    if (r->recv_len < UDS_0X23_REQ_MIN_LEN) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    addrAndLenFmt = r->recv_buf[1];
    addrLen = (addrAndLenFmt >> 4) & 0x0FU;
    sizeLen =  addrAndLenFmt & 0x0FU;

    if ((addrLen == 0U) || (addrLen > 4U) || (sizeLen == 0U) || (sizeLen > 4U)) {
        return NegativeResponse(r, kRequestOutOfRange);
    }
    if (r->recv_len < (U16)(2U + addrLen + sizeLen)) {
        return NegativeResponse(r, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (i = 0; i < addrLen; i++) {
        mem_addr = (mem_addr << 8U) | r->recv_buf[2 + i];
    }
    for (i = 0; i < sizeLen; i++) {
        mem_size = (mem_size << 8U) | r->recv_buf[2 + addrLen + i];
    }

    /* Limit to what fits in the response buffer after the 1-byte SID */
    U32 max_read = (U32)(UDS_TP_MTU - (U32)UDS_0X23_RESP_BASE_LEN);
    if ((mem_size == 0U) || (mem_size > max_read)) {
        return NegativeResponse(r, kRequestOutOfRange);
    }

    /* Allow reads from code flash or data flash only */
    bool in_df   = (mem_addr >= ETX_APP_DATA_ADDR);

    r->send_buf[0] = UDS_RESPONSE_SID_OF(kSID_READ_MEMORY_BY_ADDRESS);
    r->send_len    = (U16)UDS_0X23_RESP_BASE_LEN;

    iflash_kind_te kind = in_df ? IFLASH_KIND_DFLASH : IFLASH_KIND_PFLASH;
    if (service_iflash_read(kind, mem_addr, &r->send_buf[1], mem_size) != COM_HDR_RET_OK) {
        return NegativeResponse(r, kConditionsNotCorrect);
    }

    r->send_len = (U16)((U32)UDS_0X23_RESP_BASE_LEN + mem_size);
    return kPositiveResponse;
}

/**
 * @brief Get the internal service handler matching the given SID.
 * @param sid
 * @return pointer to UDSService or NULL if no match
 */
static UDSService getServiceForSID(U8 sid) {
    switch (sid) {
    case kSID_DIAGNOSTIC_SESSION_CONTROL:
        return _0x10_DiagnosticSessionControl;
    
    case kSID_ECU_RESET:
        return _0x11_ECUReset;
    
    case kSID_CLEAR_DIAGNOSTIC_INFORMATION:
        return NULL;
    
    case kSID_READ_DTC_INFORMATION:
        return NULL;
    
    case kSID_READ_DATA_BY_IDENTIFIER:
        return _0x22_ReadDataByIdentifier;
    
    case kSID_READ_MEMORY_BY_ADDRESS:
        return _0x23_ReadMemoryByAddress;
    
    case kSID_READ_SCALING_DATA_BY_IDENTIFIER:
        return NULL;
    
    case kSID_SECURITY_ACCESS:
        return _0x27_SecurityAccess;
    
    case kSID_COMMUNICATION_CONTROL:
        return _0x28_CommunicationControl;
    
    case kSID_READ_PERIODIC_DATA_BY_IDENTIFIER:
        return NULL;
    
    case kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return NULL;
    
    case kSID_WRITE_DATA_BY_IDENTIFIER:
		return _0x2E_WriteDataByIdentifier;
		
    case kSID_INPUT_CONTROL_BY_IDENTIFIER:
        return NULL;
    
    case kSID_ROUTINE_CONTROL:
        return _0x31_RoutineControl;
    
    case kSID_REQUEST_DOWNLOAD:
        return _0x34_RequestDownload;
    
    case kSID_REQUEST_UPLOAD:
        return _0x35_RequestUpload;
    
    case kSID_TRANSFER_DATA:
        return _0x36_TransferData;
    
    case kSID_REQUEST_TRANSFER_EXIT:
        return _0x37_RequestTransferExit;
    
    case kSID_REQUEST_FILE_TRANSFER:
        return NULL;
    
    case kSID_WRITE_MEMORY_BY_ADDRESS:
        return NULL;
    
    case kSID_TESTER_PRESENT:
        return _0x3E_TesterPresent;
    
    case kSID_ACCESS_TIMING_PARAMETER:
        return NULL;
    
    case kSID_SECURED_DATA_TRANSMISSION:
        return NULL;
    
    case kSID_CONTROL_DTC_SETTING:
        return _0x85_ControlDTCSetting;
    
    case kSID_RESPONSE_ON_EVENT:
        return NULL;
    
    default:
        return NULL;
    }
}

/**
 * @brief Call the service if it exists, modifying the response if the spec calls for it.
 * @note see UDS-1 2013 7.5.5 Pseudo code example of server response behavior
 *
 * @param srv
 * @param addressingScheme
 */
static U8 evaluateServiceResponse(UDSReq_t *r){
    U8 response = kPositiveResponse;
    BOOL suppressResponse = false;
    U8 sid = r->recv_buf[0];
    UDSService service = getServiceForSID(sid);
	
	if((uds_srvr.sessionType == kDefaultSession)&&(sid != kSID_DIAGNOSTIC_SESSION_CONTROL)){
		response = NegativeResponse(r, kSubFunctionNotSupportedInActiveSession);
		isotp_can_send(r->send_buf,r->send_len);
		r->send_len = 0;
		return response;
	}

    if (service == NULL) {
        response = NegativeResponse(r, kServiceNotSupported);
		isotp_can_send(r->send_buf,r->send_len);
		r->send_len = 0;
		return response;
    }
	
	switch (sid) {
    /* CASE Service_with_sub-function */
    /* test if service with sub-function is supported */
	  case kSID_DIAGNOSTIC_SESSION_CONTROL:
	  case kSID_ECU_RESET:
	  case kSID_SECURITY_ACCESS:
	  case kSID_COMMUNICATION_CONTROL:
	  case kSID_ROUTINE_CONTROL:
	  case kSID_TESTER_PRESENT:
	  case kSID_CONTROL_DTC_SETTING: {
		  response = service(r);
		  
		  BOOL suppressPosRspMsgIndicationBit = (r->recv_buf[1] & 0x80) != 0;
		  /* test if positive response is required and if responseCode is positive 0x00 */
		  if ((suppressPosRspMsgIndicationBit) && (response == kPositiveResponse)) {
			  suppressResponse = true;
		  }else{
			  suppressResponse = false;
		  }
		  break;
	  }

      /* CASE Service_without_sub-function */
      /* test if service without sub-function is supported */
      case kSID_READ_DATA_BY_IDENTIFIER:
      case kSID_READ_MEMORY_BY_ADDRESS:
      case kSID_WRITE_DATA_BY_IDENTIFIER:
      case kSID_REQUEST_DOWNLOAD:
      case kSID_REQUEST_UPLOAD:
      case kSID_TRANSFER_DATA:
      case kSID_REQUEST_TRANSFER_EXIT:
      case kSID_REQUEST_FILE_TRANSFER: {
          response = service(r);
          break;
      }
	  
	  default: {
		  response = kServiceNotSupported;
		  break;  
	  }
	}
	
	
	if (suppressResponse){				/* Suppress positive response message */
        NoResponse(r);
	}else{ 								/* send negative or positive response */
		isotp_can_send(r->send_buf,r->send_len);
		r->send_len = 0;
	}
	
	return response;
}

void UDS_Bootloader_set_controlsession_programming( void )
{
	uds_srvr.sessionType = kProgrammingSession;
	uds_timer_lib_start_and_stop_timer(1,UDS_TIMER_TYPE_S3TIMER);
	
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = UDS_RESPONSE_SID_OF(kSID_DIAGNOSTIC_SESSION_CONTROL);
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = uds_srvr.sessionType;
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = (U8)(UDS_CLIENT_DEFAULT_P2_MS >> 8);
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = (U8)(UDS_CLIENT_DEFAULT_P2_MS);
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = (U8)((UDS_CLIENT_DEFAULT_P2_STAR_MS / 10) >> 8);
	uds_srvr.r.send_buf[uds_srvr.r.send_len++] = (U8)(UDS_CLIENT_DEFAULT_P2_STAR_MS / 10);
	
	isotp_can_send(uds_srvr.r.send_buf,uds_srvr.r.send_len);
	uds_srvr.r.send_len = 0;
}

void UDS_Bootloader_state_machine( void )
{
    /* Drive the ISO-TP TX state machine — advances consecutive frames for
     * multi-frame responses (0x36 upload, 0x22, 0x23, etc.).               */
    isotp_poll();

    if((uds_srvr.sessionType!= kDefaultSession) && (UDSTimeAfter(uds_timer_lib_get_timer_msec(UDS_TIMER_TYPE_S3TIMER),UDS_SERVER_DEFAULT_S3_MS)))
    {
      uds_timer_lib_start_and_stop_timer(0,UDS_TIMER_TYPE_S3TIMER);
      uds_srvr.sessionType = kDefaultSession;
      if(Read_Reboot_Reason() == ETX_REPROG_REQ_FROM_UDS)
      {
        uds_srvr.is_active_slot = 0;
        PersistBootState(ETX_REPROG_REQ_FROM_UDS);
        SystemSoftwareReset();
      }
      return;
    }

    if (uds_srvr.ecuResetScheduled && UDSTimeAfter(uds_timer_lib_get_timer_msec(UDS_TIMER_TYPE_ECURESET), UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS))
    {
        uds_timer_lib_start_and_stop_timer(0,UDS_TIMER_TYPE_ECURESET);
		switch (uds_srvr.ecuResetScheduled)
        {
            case kHardReset:
		    case kSoftReset:
		    case kKeyOffOnReset :
            {
                SystemSoftwareReset();
			    break;
		    }
		    default:
			    break;
		}
		return;
	}
    
	if(isotp_can_receive(&uds_srvr.r.recv_buf[0],&uds_srvr.r.recv_len) != ISOTP_RET_OK)
    {
		return;
	}
	
	UDSReq_t *r = &uds_srvr.r;

    if(uds_srvr.requestInProgress) 
    {
        if(evaluateServiceResponse(r) ==kRequestCorrectlyReceived_ResponsePending)
        {
			uds_srvr.notReadyToReceive = true;    
		}else
        {
			uds_srvr.RCRRP = false;
            uds_srvr.notReadyToReceive = false;
        }
    }else
    {
        if(uds_srvr.notReadyToReceive)
        {
            return;
        }
        if(r->recv_len > 0)
        {
            uds_srvr.requestInProgress = true;
            if (evaluateServiceResponse(r) == kRequestCorrectlyReceived_ResponsePending ) 
            {
                uds_srvr.RCRRP = true;
            }
        }
    }	
}

/**
  * @brief  
  * @param  
  * @retval
  */
U8 UDS_Bootloader_Initialize(void)
{
    U8 stat = 0U;

    rr_memory_set_u8((U8 *)&uds_srvr, 0,    sizeof(UDSServer_t));
    rr_memory_set_u8(uds_fingerprint, 0x00U, sizeof(uds_fingerprint));

    uds_srvr.sessionType = kDefaultSession;
    uds_srvr.sec_access_boot_delay_timer =
        UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS;
    uds_srvr.xfer_crc32_run = 0xFFFFFFFFUL;

    stat = isotp_link_can_initialize(sizeof(uds_srvr.r.send_buf),
                                     uds_srvr.r.recv_buf,
                                     sizeof(uds_srvr.r.recv_buf));
    return stat;
}
