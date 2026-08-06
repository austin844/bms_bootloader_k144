/**
  ******************************************************************************
  * @attention
  * Copyright (c) - All Rights Reserved.
  * Unauthorized copying of this file, via any medium is strictly prohibited.
  * Proprietary and confidential.
  ******************************************************************************
  * @file           : uds_can_transport.c
  * @brief          : This file contains the common helper functions to encode and decode bytes.
  *
  * @author         : Adilahmed Bagwan
  * @created        : 18-July-2023
  * @lastmodified   : 22-July-2024
  * ******************************************************************************
  * */

#include "middle_layer/communication_stack/inc/uds_can_transport.h"
#include "middle_layer/communication_stack/inc/uds_timer_lib.h"
#include "middle_layer/services/inc/service_can.h"
#include "core_layer/drivers/inc/rr_memory.h"
#include <assert.h>

static IsoTpLink can_tplink;
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static U8 isotp_us_to_st_min(U32 us);
static S32 isotp_send_flow_control(U8 flow_status, U8 block_size, U32 st_min_us);

/* st_min to microsecond */
static U8 isotp_us_to_st_min(U32 us) 
{
    if (us <= 127000) 
    {
        if (us >= 100 && us <= 900) 
        {
            return (U8)(0xF0 + (us / 100));
        } else 
        {
            return (U8)(us / 1000u);
        }
    }

    return 0;
}

static S32 isotp_send_flow_control(U8 flow_status, U8 block_size, U32 st_min_us) 
{    
    S8 ret;
	IsoTpCanMessage message;
	service_can_msg_tst tx_msg;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS = flow_status;
    message.as.flow_control.BS = block_size;
    message.as.flow_control.STmin = isotp_us_to_st_min(st_min_us);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    rr_memory_set_u8(message.as.flow_control.reserve, ISO_TP_FRAME_PADDING_VALUE, sizeof(message.as.flow_control.reserve));
    ret = rr_can_transmit_data_u8(UDS_SENDR_ARBITR_ID, message.as.data_array.ptr, sizeof(message));
#else
    /* Map to services_can_com.h structure */
    tx_msg.id_u32 = UDS_SENDR_ARBITR_ID;
    tx_msg.length_u8 = 3; /* Length of Flow Control */
    rr_memory_copy_u8(tx_msg.data_au8, message.as.data_array.ptr, 3);

    if(service_can_transmit(CAN_APP_INST_1, &tx_msg, CAN_APP_TX_BLOCKING, 0) != CAN_APP_OK)
    {
        ret = ISOTP_RET_ERROR;
    }

    // ret = (S8)rr_can_transmit_data_u8(UDS_SENDR_ARBITR_ID,message.as.data_array.ptr,3);
#endif

    return ret;
}

void isotp_can_message_recvhndlr(const U8 *data, U8 len)
{

    IsoTpCanMessage message;

    if ((len >= 2u) && (len <= 64u))   /* CAN-FD: frames may be up to 64 bytes */
    {
    switch ((data[0] &0xF0)>>4)
    {
        case ISOTP_PCI_TYPE_SINGLE: 
        {
            /* update protocol result */
            if (can_tplink.receive_status == ISOTP_RECEIVE_STATUS_INPROGRESS) 
            {
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }
			
			if (((data[0] &0x0F) == 0) || ((data[0] &0x0F) > (len - 1)))
            {
				break;
    		}
			can_tplink.receive_size = data[0] &0x0F;
    		rr_memory_copy_u8(can_tplink.receive_buffer, &data[1], can_tplink.receive_size);			/* copying data */
			can_tplink.receive_status = ISOTP_RECEIVE_STATUS_FULL;
            break;
        }
        case ISOTP_PCI_TYPE_FIRST_FRAME: 
        {
            /* update protocol result */
            if (can_tplink.receive_status == ISOTP_RECEIVE_STATUS_INPROGRESS) 
            {
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }
			
			if (len != 8)
            {
				break;
			}
			U16 payload_length = data[0] &0x0F;
			payload_length = (U16)(payload_length << 8) + data[1];

			/* should not use multiple frame transmition */
			if (payload_length <= 7)
            {
				break;
			}
				
			if (payload_length > can_tplink.receive_buf_size)
            {
				/* update protocol result */
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                /* change status */
                can_tplink.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                /* request overflow FC — deferred to isotp_poll(), see field comment in
                 * uds_can_transport.h (never call isotp_send_flow_control() from this ISR) */
                can_tplink.receive_fc_status = PCI_FLOW_STATUS_OVERFLOW;
                can_tplink.receive_fc_bs = 0;
                can_tplink.receive_fc_stmin_us = 0;
                can_tplink.receive_fc_pending = true;
                break;
			}

			/* copying data */
			rr_memory_copy_u8(can_tplink.receive_buffer, &data[2], (len-2));
			can_tplink.receive_size = payload_length;
			can_tplink.receive_offset = len-2;
			can_tplink.receive_sn = 1;
			/* change status */
			can_tplink.receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
			/* request fc frame — deferred to isotp_poll(), never sent directly from this ISR */
			can_tplink.receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
			can_tplink.receive_fc_status = PCI_FLOW_STATUS_CONTINUE;
			can_tplink.receive_fc_bs = can_tplink.receive_bs_count;
			can_tplink.receive_fc_stmin_us = ISO_TP_DEFAULT_ST_MIN_US;
			can_tplink.receive_fc_pending = true;
			/* refresh timer cs */
			can_tplink.receive_timer_cr = /*isotp_user_get_us() +*/ ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            
            break;
        }
        case TSOTP_PCI_TYPE_CONSECUTIVE_FRAME: 
        {
            /* check if in receiving status */
            if (can_tplink.receive_status != ISOTP_RECEIVE_STATUS_INPROGRESS) 
            {
                can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
                break;
            }
			U8 consecutive_frame_sn = data[0] &0x0F;
			
			/* check sn */
			if (consecutive_frame_sn != can_tplink.receive_sn ) 
            {
				/* if wrong sn */
				can_tplink.receive_protocol_result = ISOTP_PROTOCOL_RESULT_WRONG_SN;
                can_tplink.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                break;
			}

			/* check data length */
			U16 remaining_bytes = can_tplink.receive_size - can_tplink.receive_offset;
			if (remaining_bytes > (len - 1)) 
            {
				remaining_bytes = len-1;
			}

			/* copying data */
			rr_memory_copy_u8(can_tplink.receive_buffer + can_tplink.receive_offset, &data[1], remaining_bytes);

			can_tplink.receive_offset += remaining_bytes;
			if (++(can_tplink.receive_sn) > 0x0F) 
            {
				can_tplink.receive_sn = 0;
			}
			 /* refresh timer cs */
		   can_tplink.receive_timer_cr = /*isotp_user_get_us() +*/ ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;			
			/* receive finished */
			if (can_tplink.receive_offset >= can_tplink.receive_size) 
            {
				can_tplink.receive_status = ISOTP_RECEIVE_STATUS_FULL;
			} else {
				/* request fc when bs reaches limit — deferred to isotp_poll(),
				 * never sent directly from this ISR (see uds_can_transport.h) */
				if (0 == --can_tplink.receive_bs_count) {
					can_tplink.receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
					can_tplink.receive_fc_status = PCI_FLOW_STATUS_CONTINUE;
					can_tplink.receive_fc_bs = can_tplink.receive_bs_count;
					can_tplink.receive_fc_stmin_us = ISO_TP_DEFAULT_ST_MIN_US;
					can_tplink.receive_fc_pending = true;
				}
			}
            break;
        }
	  	case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
        {
            /* handle fc frame only when sending in progress  */
            if (can_tplink.send_status != ISOTP_SEND_STATUS_INPROGRESS )
            {
                break;
            }
            /* check message length */
            if (len < 3)
            {
                break;
            }
            /* populate union from raw CAN bytes so bit-field reads are valid */
            rr_memory_copy_u8(message.as.data_array.ptr, data, len);

            /* refresh bs timer */
            can_tplink.send_timer_bs = ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;

            /* overflow */
            if (message.as.flow_control.FS == PCI_FLOW_STATUS_OVERFLOW )
            {
                can_tplink.send_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                can_tplink.send_status = ISOTP_SEND_STATUS_ERROR;
            }

            /* wait */
            else if (message.as.flow_control.FS == PCI_FLOW_STATUS_WAIT )
            {
                can_tplink.send_wtf_count += 1;
                /* wait exceed allowed count */
                if (can_tplink.send_wtf_count > ISO_TP_MAX_WFT_NUMBER) {
                    can_tplink.send_protocol_result = ISOTP_PROTOCOL_RESULT_WFT_OVRN;
                    can_tplink.send_status = ISOTP_SEND_STATUS_ERROR;
                }
            }

            /* permit send */
            else if (message.as.flow_control.FS == PCI_FLOW_STATUS_CONTINUE)
            {
                if (message.as.flow_control.BS == 0) {
                    can_tplink.send_bs_remain = ISOTP_INVALID_BS;
                } else {
                    can_tplink.send_bs_remain = message.as.flow_control.BS;
                }
                can_tplink.send_wtf_count = 0;
            }
            break;
		}
        default:
            break;
    }
    }
}

S32 isotp_can_receive(U8 *payload, U16 *out_size)
{
    (void)payload;
    if (can_tplink.receive_status != ISOTP_RECEIVE_STATUS_FULL)
    {
        return ISOTP_RET_NO_DATA;
    }
    *out_size = can_tplink.receive_size;
    can_tplink.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    return ISOTP_RET_OK;
}

S32 isotp_can_send( U8 *payload, U16 size) 
{
	IsoTpCanMessage message;
	service_can_msg_tst tx_msg;

    if (size > can_tplink.send_buf_size) 
    {
		return ISOTP_RET_OVERFLOW;
    }

    if (can_tplink.send_status == ISOTP_SEND_STATUS_INPROGRESS ) 
    {
        return ISOTP_RET_INPROGRESS;
    }

    /* copy S32o local buffer */
    can_tplink.send_offset = 0;

    if (size < 8) 
    {
        /* send single frame */
		/* setup message  */
    	message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    	message.as.single_frame.SF_DL = (U8) size;
    	rr_memory_copy_u8(message.as.single_frame.data, payload, size);
	    /* send message */
		#ifdef ISO_TP_FRAME_PADDING
			(void) rr_memory_set_u8(message.as.single_frame.data + can_tplink.send_size, ISO_TP_FRAME_PADDING_VALUE, sizeof(message.as.single_frame.data) - can_tplink.send_size);
			ret = rr_can_transmit_data_u8(UDS_SENDR_ARBITR_ID, message.as.data_array.ptr, sizeof(message));
		#else
            tx_msg.id_u32 = UDS_SENDR_ARBITR_ID;
            tx_msg.length_u8 = size + 1;
			rr_memory_copy_u8(tx_msg.data_au8, message.as.data_array.ptr, tx_msg.length_u8);
			// return rr_can_transmit_data_u8(UDS_SENDR_ARBITR_ID,message.as.data_array.ptr,size+ 1);
		#endif

        if (service_can_transmit(CAN_APP_INST_1, &tx_msg, CAN_APP_TX_BLOCKING, 0) == CAN_APP_OK)
        {
            return ISOTP_RET_OK;
        }

        return ISOTP_RET_ERROR;

    } 
    else 
    {
        /* send multi-frame */
		message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
		message.as.first_frame.FF_DL_low = (U8) size;
		message.as.first_frame.FF_DL_high = (U8) (0x0F & (size>> 8));
		rr_memory_copy_u8(message.as.first_frame.data, payload, sizeof(message.as.first_frame.data));

		/* Send the First Frame as exactly 2 PCI bytes + 6 data bytes (8 bytes).
		 * The union is now 64 bytes (CAN-FD CF), so sizeof(message) must NOT be used
		 * here — only the first 8 bytes are populated; the rest is uninitialised. */
		if (service_can_transmit(CAN_APP_INST_1, &tx_msg, CAN_APP_TX_BLOCKING, 0) == CAN_APP_OK)
        {
            can_tplink.send_buffer  = payload;
            can_tplink.send_size    = size;
            can_tplink.send_offset += sizeof(message.as.first_frame.data);
            can_tplink.send_sn = 1;
            can_tplink.send_bs_remain = 0;
            can_tplink.send_st_min_us = 0;
            can_tplink.send_wtf_count = 0;
            can_tplink.send_timer_bs = ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            can_tplink.send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            can_tplink.send_status = ISOTP_SEND_STATUS_INPROGRESS;
            
            return ISOTP_RET_OK;
        }
        return ISOTP_RET_ERROR;
    }
}

void isotp_poll(void )
{
    service_can_msg_tst tx_msg;
    IsoTpCanMessage message;
    U16 data_length;

    if (can_tplink.receive_fc_pending) {
        can_tplink.receive_fc_pending = false;

        isotp_send_flow_control(can_tplink.receive_fc_status,
                                 can_tplink.receive_fc_bs,
                                 can_tplink.receive_fc_stmin_us);
    }

    if (can_tplink.send_status == ISOTP_SEND_STATUS_INPROGRESS ) 
    {
        if ((can_tplink.send_bs_remain == ISOTP_INVALID_BS || can_tplink.send_bs_remain > 0U))
        {
            assert(can_tplink.send_size > 7);
            message.as.consecutive_frame.type = TSOTP_PCI_TYPE_CONSECUTIVE_FRAME;
            message.as.consecutive_frame.SN = can_tplink.send_sn;
            
            data_length = can_tplink.send_size - can_tplink.send_offset;
            if (data_length > sizeof(message.as.consecutive_frame.data))
            {
                data_length = sizeof(message.as.consecutive_frame.data);
            }
            
            rr_memory_copy_u8(message.as.consecutive_frame.data,
                   can_tplink.send_buffer + can_tplink.send_offset,
                   data_length);

            tx_msg.id_u32 = UDS_SENDR_ARBITR_ID;
            tx_msg.length_u8 = data_length + 1;
            rr_memory_copy_u8(tx_msg.data_au8, message.as.data_array.ptr, tx_msg.length_u8);

            if (service_can_transmit(CAN_APP_INST_1, &tx_msg, CAN_APP_TX_BLOCKING, 0) == CAN_APP_OK)
            {
                can_tplink.send_offset += data_length;
                if (++(can_tplink.send_sn) > 0x0F)
                {
                    can_tplink.send_sn = 0;
                }
            } else {
                can_tplink.send_status = ISOTP_SEND_STATUS_ERROR;
            }

            if (can_tplink.send_bs_remain != ISOTP_INVALID_BS)
            {
                can_tplink.send_bs_remain -= 1;
            }
            can_tplink.send_timer_bs += ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US;
            can_tplink.send_timer_st += can_tplink.send_st_min_us;

            if (can_tplink.send_offset >= can_tplink.send_size)
            {
                can_tplink.send_status = ISOTP_SEND_STATUS_IDLE;
            }
        }
    }
}
U8 isotp_link_can_initialize( U16 sendbufsize, U8 *recvbuf, U16 recvbufsize)
{
    U8 stat = 1;
    /* Initialize uds timers */
    stat = uds_timer_lib_initialize();

    /* Initialize can_tp_link */
	rr_memory_set_u8((U8*)&can_tplink, 0, sizeof(can_tplink));
    can_tplink.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    can_tplink.send_status = ISOTP_SEND_STATUS_IDLE;
    can_tplink.send_buf_size = sendbufsize;
    can_tplink.receive_buffer = recvbuf;
    can_tplink.receive_buf_size = recvbufsize;

	return stat;
}
