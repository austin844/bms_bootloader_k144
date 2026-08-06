/**
  ******************************************************************************
  * @attention
  * Copyright (c) - All Rights Reserved.
  * Unauthorized copying of this file, via any medium is strictly prohibited.
  * Proprietary and confidential.
  ******************************************************************************
  * @file           : uds.h
  * @brief          : This file contains the common helper functions to encode and decode bytes.
  *
  * @author         : Adilahmed Bagwan
  * @created        : 17-July-2023
  * @lastmodified   : 17-July-2023
  * ******************************************************************************
  * */

#ifndef _APP_INCLUDE_UDS_H_
#define _APP_INCLUDE_UDS_H_

#ifdef __cplusplus
extern "C" {
#endif
  
#include <stddef.h>
#include "common_header.h"

/* true when elapsed milliseconds 'a' exceed threshold 'b' */
#define UDSTimeAfter(a, b)  ((U32)(a) >= (U32)(b))
	

/*General values for all types of frames*/ 
#define UDS_NEGATIVE_SERVICE_ID                     0x7F
#define UDS_RESPONSE_SID_OF(request_sid) (request_sid + 0x40)

#define UDS_NEG_RESP_LEN 3U
#define UDS_0X10_REQ_LEN 2U
#define UDS_0X10_RESP_LEN 4U
#define UDS_0X11_REQ_MIN_LEN 2U
#define UDS_0X11_RESP_BASE_LEN 2U
#define UDS_0X23_REQ_MIN_LEN 4U
#define UDS_0X23_RESP_BASE_LEN 1U
#define UDS_0X22_RESP_BASE_LEN 1U
#define UDS_0X27_REQ_BASE_LEN 2U
#define UDS_0X27_RESP_BASE_LEN 2U
#define UDS_0X28_REQ_BASE_LEN 3U
#define UDS_0X28_RESP_LEN 2U
#define UDS_0X2E_REQ_BASE_LEN 3U
#define UDS_0X2E_REQ_MIN_LEN 4U
#define UDS_0X2E_RESP_LEN 3U
#define UDS_0X31_REQ_MIN_LEN 4U
#define UDS_0X31_RESP_MIN_LEN 4U
#define UDS_0X34_REQ_BASE_LEN 3U
#define UDS_0X34_RESP_BASE_LEN 2U
#define UDS_0X35_REQ_BASE_LEN 3U
#define UDS_0X35_RESP_BASE_LEN 2U
#define UDS_0X36_REQ_BASE_LEN 2U
#define UDS_0X36_RESP_BASE_LEN 2U
#define UDS_0X37_REQ_BASE_LEN 1U
#define UDS_0X37_RESP_BASE_LEN 1U
#define UDS_0X3E_REQ_MIN_LEN 2U
#define UDS_0X3E_REQ_MAX_LEN 2U
#define UDS_0X3E_RESP_LEN 2U
#define UDS_0X85_REQ_BASE_LEN 2U
#define UDS_0X85_RESP_LEN 2U

#define ROUTINE_ERASE_MEMORY        0xFF00
#define ROUTINE_VERIFY_CHECKSUM     0x0202
#define ROUTINE_SWAP_BANK           0xFF01

enum UDSDiagnosticServiceId {
    kSID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    kSID_ECU_RESET = 0x11,
    kSID_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    kSID_READ_DTC_INFORMATION = 0x19,
    kSID_READ_DATA_BY_IDENTIFIER = 0x22,
    kSID_READ_MEMORY_BY_ADDRESS = 0x23,
    kSID_READ_SCALING_DATA_BY_IDENTIFIER = 0x24,
    kSID_SECURITY_ACCESS = 0x27,
    kSID_COMMUNICATION_CONTROL = 0x28,
    kSID_READ_PERIODIC_DATA_BY_IDENTIFIER = 0x2A,
    kSID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER = 0x2C,
    kSID_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    kSID_INPUT_CONTROL_BY_IDENTIFIER = 0x2F,
    kSID_ROUTINE_CONTROL = 0x31,
    kSID_REQUEST_DOWNLOAD = 0x34,
    kSID_REQUEST_UPLOAD = 0x35,
    kSID_TRANSFER_DATA = 0x36,
    kSID_REQUEST_TRANSFER_EXIT = 0x37,
    kSID_REQUEST_FILE_TRANSFER = 0x38,
    kSID_WRITE_MEMORY_BY_ADDRESS = 0x3D,
    kSID_TESTER_PRESENT = 0x3E,
    kSID_ACCESS_TIMING_PARAMETER = 0x83,
    kSID_SECURED_DATA_TRANSMISSION = 0x84,
    kSID_CONTROL_DTC_SETTING = 0x85,
    kSID_RESPONSE_ON_EVENT = 0x86,
};

enum UDSDiagnosticSessionType {
    kDefaultSession = 0x01,
    kProgrammingSession = 0x02,
    kExtendedDiagnostic = 0x03,
    kSafetySystemDiagnostic = 0x04,
};

enum UDSDiagnosticResponseType{
    kPositiveResponse = 0,
    kGeneralReject = 0x10,
    kServiceNotSupported = 0x11,
    kSubFunctionNotSupported = 0x12,
    kIncorrectMessageLengthOrInvalidFormat = 0x13,
    kResponseTooLong = 0x14,
    kBusyRepeatRequest = 0x21,
    kConditionsNotCorrect = 0x22,
    kRequestSequenceError = 0x24,
    kNoResponseFromSubnetComponent = 0x25,
    kFailurePreventsExecutionOfRequestedAction = 0x26,
    kRequestOutOfRange = 0x31,
    kSecurityAccessDenied = 0x33,
    kInvalidKey = 0x35,
    kExceedNumberOfAttempts = 0x36,
    kRequiredTimeDelayNotExpired = 0x37,
    kUploadDownloadNotAccepted = 0x70,
    kTransferDataSuspended = 0x71,
    kGeneralProgrammingFailure = 0x72,
    kWrongBlockSequenceCounter = 0x73,
    kRequestCorrectlyReceived_ResponsePending = 0x78,
    kSubFunctionNotSupportedInActiveSession = 0x7E,
    kServiceNotSupportedInActiveSession = 0x7F,
};

/**
 * @brief LEV_RT_
 * @addtogroup ecuReset_0x11
 */
enum UDSECUResetType {
    kHardReset = 1,
    kKeyOffOnReset = 2,
    kSoftReset = 3,
    kEnableRapidPowerShutDown = 4,
    kDisableRapidPowerShutDown = 5,
};

typedef U8 UDSECUReset_t;

/**
 * @addtogroup securityAccess_0x27
 */
enum UDSSecurityAccessType {
    kRequestSeed = 0x01,
    kSendKey = 0x02,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

/**
 * @addtogroup communicationControl_0x28
 */
enum UDSCommunicationType {
    kNormalCommunicationMessages = 0x1,
    kNetworkManagementCommunicationMessages = 0x2,
    kNetworkManagementCommunicationMessagesAndNormalCommunicationMessages = 0x3,
};

/**
 * @addtogroup routineControl_0x31
 */
enum RoutineControlType {
    kStartRoutine = 1,
    kStopRoutine = 2,
    kRequestRoutineResults = 3,
};

/**
 * @addtogroup controlDTCSetting_0x85
 */
enum DTCSettingType {
    kDTCSettingON = 0x01,
    kDTCSettingOFF = 0x02,
};

/*Negative response struct*/
typedef struct{
	U8 NegativeResponseID;
	U8 ServiceID;
	U8 NRC;
}NegativeResponse_t;

/**
 * @brief Server request context
 */
typedef struct {
    U8     recv_buf[4096];
    U8     send_buf[4096];
    U16    recv_len;
    U16    send_len;
} UDSReq_t;

typedef struct{
	U8		    is_active_slot;
    U8          ecuResetScheduled;              // nonzero indicates that an ECUReset has been scheduled
    U32         sec_access_auth_fail_timer;     // brute-force hardening: rate limit security access requests
    U32         sec_access_boot_delay_timer;    // brute-force hardening: restrict security access until timer expires
    U32         current_seed;                   // seed issued in the last 0x27 RequestSeed
    U32         xfer_crc32_run;                 // running CRC-32 register accumulated across 0x36 blocks
    U32         xfer_crc32_final;               // finalized CRC-32 of the last completed transfer; set by
                                                // RequestTransferExit and survives ResetTransfer() so VerifyChecksum can read it
	U32         xferBlockAddress;				// current flash address for active transfer
    BOOL        xferIsActive;                   /**
                                                * @brief UDS-1-2013: Table 407 - 0x36 TransferData Supported negative
                                                * response codes requires that the server keep track of whether the
                                                * transfer is active
                                                */
    BOOL        xferIsUpload;                   /* true = 0x35 upload (ECU→tester), false = 0x34 download (tester→ECU) */
                                                // UDS-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter // value starts at 0x01
    U8          xferBlockSequenceCounter;
    U32         xferTotalBytes;                 // total transfer size in bytes requested by the client
    U32         xferByteCounter;                // total number of bytes transferred
    U32         xferBlockLength;                // block length (convenience for the TransferData API)
    U8          sessionType;                    // diagnostic session type (0x10)
    U8          securityLevel;                  // SecurityAccess (0x27) level
    BOOL        RCRRP;                          // set to true when user fn returns 0x78 and false otherwise
    BOOL        requestInProgress;              // set to true when a request has been processed but the response has not yet been sent
    BOOL        notReadyToReceive;              // UDS-1 2013 defines the following conditions under which the server does not
                                                // process incoming requests:
                                                // - not ready to receive (Table A.1 0x78)
                                                // - not accepting request messages and not sending responses (9.3.1)
                                                // when this variable is set to true, incoming ISO-TP data will not be processed.
    UDSReq_t    r;
} UDSServer_t;

/*UDS APIs*/
U8 UDS_Bootloader_Initialize( void );

void UDS_Bootloader_state_machine( void );

void UDS_Bootloader_set_controlsession_programming( void );

#ifdef __cplusplus
}
#endif

#endif /* _APP_INCLUDE_UDS_H_ */
