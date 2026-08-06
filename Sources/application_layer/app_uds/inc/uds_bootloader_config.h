/**
  ******************************************************************************
  * @attention
  * Copyright (c) - All Rights Reserved.
  * Unauthorized copying of this file, via any medium is strictly prohibited.
  * Proprietary and confidential.
  ******************************************************************************
  * @file           : uds_bootloader_config.h
  * @brief          : This file contains the common helper functions to encode and decode bytes.
  *
  * @author         : Adilahmed Bagwan.
  * @created        : 17-July-2022
  * @lastmodified   : 17-July-2022
  * ******************************************************************************
  * */

#ifndef APP_INCLUDE_BOOTLOADER_CONFIG_H_
#define APP_INCLUDE_BOOTLOADER_CONFIG_H_

#pragma once

#define BOOTLOADER_RESPONSE_TIME		13
#define MAX_ERRORS_TIMES				3

#ifndef UDS_ENABLE_ASSERT
#define UDS_ENABLE_ASSERT 0
#endif

/** ISO-TP Maximum Transmissiable Unit (ISO-15764-2-2004 section 5.3.3).
 *  Sized so one TransferData (0x36) message carries 3072 data bytes (3 KB, and a
 *  multiple of the 128-byte flash write unit) plus the 2-byte header
 *  (SID + blockSequenceCounter) = 3074. This is reported to the client as
 *  maxNumberOfBlockLength in the RequestDownload (0x34) response, so the host
 *  flasher transfers 3072 bytes per block.
 *  Bounds: <= 4095 (ISO-TP 12-bit First Frame length limit) and <= 4096 (device
 *  UDS recv_buf/send_buf in uds.h), so a full block fits without the 32-bit
 *  escape First Frame and without buffer overflow. */
#define UDS_ISOTP_MTU               (1026)

#ifndef UDS_TP_MTU
#define UDS_TP_MTU UDS_ISOTP_MTU
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_MS
#define UDS_CLIENT_DEFAULT_P2_MS    (50)
#endif

#ifndef UDS_CLIENT_DEFAULT_P2_STAR_MS
#define UDS_CLIENT_DEFAULT_P2_STAR_MS (1000U)
#endif

//_Static_assert(UDS_CLIENT_DEFAULT_P2_STAR_MS > UDS_CLIENT_DEFAULT_P2_MS, "EXTENDED TIMEOUT LESS THAN STANDARD TIMEOUT");

#ifndef UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS
#define UDS_SERVER_DEFAULT_POWER_DOWN_TIME_MS (3000)
#endif

#ifndef UDS_SERVER_DEFAULT_P2_MS
#define UDS_SERVER_DEFAULT_P2_MS (50)
#endif

#ifndef UDS_SERVER_DEFAULT_P2_STAR_MS
#define UDS_SERVER_DEFAULT_P2_STAR_MS (200)
#endif

#ifndef UDS_SERVER_DEFAULT_S3_MS
#define UDS_SERVER_DEFAULT_S3_MS (5000)   /* ISO 14229-1 minimum: 5000 ms */
#endif

// Amount of time to wait after boot before accepting 0x27 requests.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_BOOT_DELAY_MS (1000)
#endif

// Amount of time to wait after an authentication failure before accepting another 0x27 request.
#ifndef UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS
#define UDS_SERVER_0x27_BRUTE_FORCE_MITIGATION_AUTH_FAIL_DELAY_MS (1000)
#endif

#ifndef UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH
/*! ISO14229-1:2013 Table 396. This parameter is used by the requestDownload positive response
message to inform the client how many data bytes (maxNumberOfBlockLength) to include in each
TransferData request message from the client. */
#define UDS_SERVER_DEFAULT_XFER_DATA_MAX_BLOCKLENGTH (UDS_TP_MTU)
#endif


#endif /* APP_INCLUDE_BOOTLOADER_CONFIG_H_ */
