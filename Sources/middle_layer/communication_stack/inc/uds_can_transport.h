/**
  ******************************************************************************
  * @attention
  * Copyright (c) - All Rights Reserved.
  * Unauthorized copying of this file, via any medium is strictly prohibited.
  * Proprietary and confidential.
  ******************************************************************************
  * @file           : uds_can_transport.h
  * @brief          : This file contains the common helper functions to encode and decode bytes.
  *
  * @author         : Adilahmed Bagwan
  * @created        : 17-July-2023
  * @lastmodified   : 17-July-2024
  * ******************************************************************************
  * */

#ifndef _TRANSPORT_INC_CAN_TP_H_
#define _TRANSPORT_INC_CAN_TP_H_

#include "common_header.h"

#define UDS_SENDR_ARBITR_ID		    		0x7FC
#define BOOTLOADER_RX_BOOT_CMD_ID   		0x7FA
#define BOOTLOADER_RX_DATA_ID       		0x7FB
#define BOOTLOADER_RX_EN_CMD_ID     		0x7C1
#define BOOTLOADER_RX_FUNCTIONAL_CMD_ID		0x7DF

/* Max number of messages the receiver can receive at one time, this value 
 * is affected by can driver queue length
 */
#define ISO_TP_DEFAULT_BLOCK_SIZE   8

/* The STmin parameter value specifies the minimum time gap allowed between 
 * the transmission of consecutive frame network protocol data units
 */
#define ISO_TP_DEFAULT_ST_MIN_US    0

/* This parameter indicate how many FC N_PDU WTs can be transmitted by the 
 * receiver in a row.
 */
#define ISO_TP_MAX_WFT_NUMBER       1

/* Private: The default timeout to use when waiting for a response during a
 * multi-frame send or receive.
 */
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 1000

/* Private: Determines if by default, padding is added to ISO-TP message frames.
 */
//#define ISO_TP_FRAME_PADDING

/* Private: Value to use when padding frames if enabled by ISO_TP_FRAME_PADDING
 */
#ifndef ISO_TP_FRAME_PADDING_VALUE
#define ISO_TP_FRAME_PADDING_VALUE 0xAA
#endif

	
#define ISOTP_BYTE_ORDER_LITTLE_ENDIAN 1

/* CAN-FD feature flag (must match rr_can.c). 0 = classic: consecutive frames carry
 * 7 data bytes. 1 = CAN-FD: consecutive frames carry 63 data bytes. Enable via the
 * project preprocessor symbol RR_CAN_FD_ENABLE=1 (one define flips both layers). */
#ifndef RR_CAN_FD_ENABLE
#define RR_CAN_FD_ENABLE 0
#endif
#if RR_CAN_FD_ENABLE
#define ISOTP_CF_DATA_MAX   (63U)
#else
#define ISOTP_CF_DATA_MAX   (7U)
#endif
/**************************************************************
 * S32ernal used defines
 *************************************************************/
#define ISOTP_RET_OK           0
#define ISOTP_RET_ERROR        -1
#define ISOTP_RET_INPROGRESS   -2
#define ISOTP_RET_OVERFLOW     -3
#define ISOTP_RET_WRONG_SN     -4
#define ISOTP_RET_NO_DATA      -5
#define ISOTP_RET_TIMEOUT      -6
#define ISOTP_RET_LENGTH       -7
#define ISOTP_RET_NOSPACE      -8

/* return logic true if 'a' is after 'b' */
#define IsoTpTimeAfter(a,b) ((S3232_t)((S3232_t)(b) - (S3232_t)(a)) < 0)

/*  invalid bs */
#define ISOTP_INVALID_BS       0xFFFF

/* ISOTP sender status */
typedef enum {
    ISOTP_SEND_STATUS_IDLE,
    ISOTP_SEND_STATUS_INPROGRESS,
    ISOTP_SEND_STATUS_ERROR,
} IsoTpSendStatusTypes;

/* ISOTP receiver status */
typedef enum {
    ISOTP_RECEIVE_STATUS_IDLE,
    ISOTP_RECEIVE_STATUS_INPROGRESS,
    ISOTP_RECEIVE_STATUS_FULL,
} IsoTpReceiveStatusTypes;

/* can frame definition */
#if defined(ISOTP_BYTE_ORDER_LITTLE_ENDIAN)
typedef struct {
    U8 reserve_1:4;
    U8 type:4;
    U8 reserve_2[7];
} IsoTpPciType;

typedef struct {
    U8 SF_DL:4;
    U8 type:4;
    U8 data[7];
} IsoTpSingleFrame;

typedef struct {
    U8 FF_DL_high:4;
    U8 type:4;
    U8 FF_DL_low;
    U8 data[6];
} IsoTpFirstFrame;

typedef struct {
    U8 SN:4;
    U8 type:4;
    U8 data[63];          /* CAN-FD: up to 63 data bytes per consecutive frame */
} IsoTpConsecutiveFrame;

typedef struct {
    U8 FS:4;
    U8 type:4;
    U8 BS;
    U8 STmin;
    U8 reserve[5];
} IsoTpFlowControl;

#else

typedef struct {
    U8 type:4;
    U8 reserve_1:4;
    U8 reserve_2[7];
} IsoTpPciType;

/*
* single frame
* +-------------------------+-----+
* | byte #0                 | ... |
* +-------------------------+-----+
* | nibble #0   | nibble #1 | ... |
* +-------------+-----------+ ... +
* | PCIType = 0 | SF_DL     | ... |
* +-------------+-----------+-----+
*/
typedef struct {
    U8 type:4;
    U8 SF_DL:4;
    U8 data[7];
} IsoTpSingleFrame;

/*
* first frame
* +-------------------------+-----------------------+-----+
* | byte #0                 | byte #1               | ... |
* +-------------------------+-----------+-----------+-----+
* | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ... |
* +-------------+-----------+-----------+-----------+-----+
* | PCIType = 1 | FF_DL                             | ... |
* +-------------+-----------+-----------------------+-----+
*/
typedef struct {
    U8 type:4;
    U8 FF_DL_high:4;
    U8 FF_DL_low;
    U8 data[6];
} IsoTpFirstFrame;

/*
* consecutive frame
* +-------------------------+-----+
* | byte #0                 | ... |
* +-------------------------+-----+
* | nibble #0   | nibble #1 | ... |
* +-------------+-----------+ ... +
* | PCIType = 0 | SN        | ... |
* +-------------+-----------+-----+
*/
typedef struct {
    U8 type:4;
    U8 SN:4;
    U8 data[63];          /* CAN-FD: up to 63 data bytes per consecutive frame */
} IsoTpConsecutiveFrame;

/*
* flow control frame
* +-------------------------+-----------------------+-----------------------+-----+
* | byte #0                 | byte #1               | byte #2               | ... |
* +-------------------------+-----------+-----------+-----------+-----------+-----+
* | nibble #0   | nibble #1 | nibble #2 | nibble #3 | nibble #4 | nibble #5 | ... |
* +-------------+-----------+-----------+-----------+-----------+-----------+-----+
* | PCIType = 1 | FS        | BS                    | STmin                 | ... |
* +-------------+-----------+-----------------------+-----------------------+-----+
*/
typedef struct {
    U8 type:4;
    U8 FS:4;
    U8 BS;
    U8 STmin;
    U8 reserve[5];
} IsoTpFlowControl;

#endif

typedef struct {
    U8 ptr[64];           /* CAN-FD: raw view spans a full 64-byte frame */
} IsoTpDataArray;

typedef struct {
    union {
        IsoTpPciType          common;
        IsoTpSingleFrame      single_frame;
        IsoTpFirstFrame       first_frame;
        IsoTpConsecutiveFrame consecutive_frame;
        IsoTpFlowControl      flow_control;
        IsoTpDataArray        data_array;
    } as;
} IsoTpCanMessage;

/**************************************************************
 * protocol specific defines
 *************************************************************/

/* Private: Protocol Control Information (PCI) types, for identifying each frame of an ISO-TP message.
 */
typedef enum {
    ISOTP_PCI_TYPE_SINGLE             = 0x0,
    ISOTP_PCI_TYPE_FIRST_FRAME        = 0x1,
    TSOTP_PCI_TYPE_CONSECUTIVE_FRAME  = 0x2,
    ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME = 0x3
} IsoTpProtocolControlInformation;

/* Private: Protocol Control Information (PCI) flow control identifiers.
 */
typedef enum {
    PCI_FLOW_STATUS_CONTINUE = 0x0,
    PCI_FLOW_STATUS_WAIT     = 0x1,
    PCI_FLOW_STATUS_OVERFLOW = 0x2
} IsoTpFlowStatus;

/* Private: network layer result code.
 */
#define ISOTP_PROTOCOL_RESULT_OK            0
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_A    -1
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_BS   -2
#define ISOTP_PROTOCOL_RESULT_TIMEOUT_CR   -3
#define ISOTP_PROTOCOL_RESULT_WRONG_SN     -4
#define ISOTP_PROTOCOL_RESULT_INVALID_FS   -5
#define ISOTP_PROTOCOL_RESULT_UNEXP_PDU    -6
#define ISOTP_PROTOCOL_RESULT_WFT_OVRN     -7
#define ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW -8
#define ISOTP_PROTOCOL_RESULT_ERROR        -9

/**
 * @brief Struct containing the data for linking an application to a CAN instance.
 * The data stored in this struct is used S32ernally and may be used by software programs
 * using this library.
 */
typedef struct IsoTpLink {
    /* sender parameters */
    U8*                    send_buffer;    /* poS32er to payload being sent (multi-frame) */
    U16                    send_size;      /* total payload size (multi-frame)            */
    U16                    send_buf_size;
    U16                    send_offset;
    /* multi-frame flags */
    U8                     send_sn;
    U16                    send_bs_remain; /* Remaining block size */
    U32                    send_st_min_us; /* Separation Time between consecutive frames */
    U8                     send_wtf_count; /* Maximum number of FC.Wait frame transmissions  */
    U32                    send_timer_st;  /* Last time send consecutive frame */
    U32                    send_timer_bs;  /* Time until reception of the next FlowControl N_PDU start at sending FF, CF, receive FC end at receive FC */
    S32                         send_protocol_result;
    U8                     send_status;

    /* message buffer */
    U8*                    receive_buffer;
    U16                    receive_buf_size;
    U16                    receive_size;
    U16                    receive_offset;
    /* multi-frame control */
    U8                     receive_sn;
    U8                     receive_bs_count; /* Maximum number of FC.Wait frame transmissions  */
    U32                    receive_timer_cr; /* Time until transmission of the next ConsecutiveFrame N_PDU start at sending FC, receive CF end at receive FC */
    S32                         receive_protocol_result;
    U8                     receive_status;

    /* Deferred Flow Control request: the RX ISR (isotp_can_message_recvhndlr)
     * must never send a Flow Control frame directly — rr_can_transmit_data_u8()
     * busy-waits on a TX-complete flag set by a same-NVIC-priority sibling ISR,
     * which cannot preempt the RX ISR that's waiting on it. Instead the ISR
     * records the request here and isotp_poll() (thread-mode, main loop) sends
     * it, where blocking briefly on that flag is safe. */
    volatile BOOL               receive_fc_pending;
    U8                     receive_fc_status;
    U8                     receive_fc_bs;
    U32                    receive_fc_stmin_us;

    /* user implemented callback functions */
    void (*isotp_user_can_recvhndlr)(const U8 *data, U8 len);
} IsoTpLink;

/**
 * @brief Initializes the ISO-TP library.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param sendid The ID used to send data to other CAN nodes.
 * @param sendbuf A poS32er to an area in memory which can be used as a buffer for data to be sent.
 * @param sendbufsize The size of the buffer area.
 * @param recvbuf A poS32er to an area in memory which can be used as a buffer for data to be received.
 * @param recvbufsize The size of the buffer area.
 */
U8 isotp_link_can_initialize(U16 sendbufsize, U8 *recvbuf, U16 recvbufsize);

void isotp_poll(void );
S32 isotp_can_receive(U8 *payload, U16 *out_size);

S32 isotp_can_send(U8 *payload, U16 size);
void isotp_can_message_recvhndlr(const U8 *data, U8 len);

#endif /* _TRANSPORT_INCLUDE_CAN_TP_H_ */
