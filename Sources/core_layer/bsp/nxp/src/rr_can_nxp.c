/**
 * @file rr_can_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction CAN driver (FlexCAN via the NXP CAN PAL).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_can hardware path. Drives each FlexCAN channel through the
 * 		 NXP CAN PAL (CAN_* / FLEXCAN_DRV_*): the first can_filters_nos_u8 mailboxes of a channel receive and
 * 		 the rest send, reused round-robin by rr_can_nxp_transmit. The SDK-registered callbacks
 * 		 (can_0/1_callback_event_v / _error_v) are the interrupt-driven seam: they dispatch received frames
 * 		 and the raw error status into the wrapper's generic rr_can_receive() / rr_can_error_callback()
 * 		 (extern in rr_can_nxp.h), which own the middleware callbacks and the bus-off recovery policy. The
 * 		 wrapper (rr_can.c) owns argument validation and the per-channel filter count this port reads across
 * 		 the seam. A NULL pal_pst descriptor entry means that channel is turned off (for example CAN3_INST,
 * 		 the SBC).
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_can_nxp.h"	/* This port's helper API, the wrapper's public types and the cross-seam extern declarations */

#if defined(NXP_S32K144_146)

#if CAN_INSTANCE_0
#include "can_pal_0.h"	/* Generated CAN0 PAL config: can_pal_0_instance / can_pal_0_Config0 */
#endif

#if CAN_INSTANCE_1
#include "can_pal_1.h"	/* Generated CAN1 PAL config: can_pal_1_instance / can_pal_1_config_0 */
#endif

#if CAN_INSTANCE_2
#include "can_pal_2.h"	/* Generated CAN2 PAL config: can_pal_2_instance / can_pal_2_config_0 (SBC) */
#endif

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* How many message slots (mailboxes) each FlexCAN channel has. rr_can_nxp_initialize() gives the first
 * can_filters_nos_u8 slots to receiving (numbered 0, 1, 2, ...) and uses the rest for sending. */
#define NXP_CAN_INST0_MAX_MB_NOS	(32U)	/*!< Number of message slots on CAN0 (CAN1_INST) */
#define NXP_CAN_INST1_MAX_MB_NOS	(16U)	/*!< Number of message slots on CAN1 (CAN2_INST) */

#if CAN_INSTANCE_2
#define NXP_CAN_INST2_MAX_MB_NOS	(16U)	/*!< Number of message slots on CAN2 (CAN3_INST, SBC) */
#endif

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief Which message slots one channel uses for receiving and which for sending. The receive
 * 		  slots come first, one after another, starting at rx_start_u8.
 */
typedef struct
{
	U8 rx_start_u8;	/**< First slot used for receiving (always 0) */
	U8 rx_end_u8;	/**< Last slot used for receiving (number of receive filters - 1) */
	U8 tx_start_u8;	/**< First slot used for sending (the slot right after the receive slots) */
	U8 tx_end_u8;	/**< Last slot used for sending (total slots on the channel - 1) */
} nxp_can_mb_range_tst;

/**
 * @brief Fixed settings for one CAN channel, looked up by @ref can_inst_te. These never change, so
 * 		  they are kept in flash (program memory). If pal_pst is NULL, that channel is turned off.
 */
typedef struct
{
	const can_instance_t*    pal_pst;		/**< Handle for NXP's CAN driver (the "PAL") for this channel */
	const can_user_config_t* cfg_pst;		/**< Start-up settings passed to NXP's CAN driver */
	flexcan_callback_t       event_cb_pf;	/**< Function called when a message is received */
	flexcan_error_callback_t error_cb_pf;	/**< Function called when a bus error happens */
	U8                       mb_max_u8;		/**< Total number of message slots on this channel */
} nxp_can_desc_tst;

/**
 * @brief Values for one CAN channel that change while running, looked up by @ref can_inst_te. These
 * 		  live in RAM. They are kept separate from the fixed @ref nxp_can_desc_tst so the fixed
 * 		  settings can stay in flash.
 */
typedef struct
{
	can_message_t        tx_msg_st;		/**< Temporary message that rr_can_nxp_transmit() fills in before sending */
	can_message_t        rx_msg_st;		/**< Temporary buffer the driver writes a received message into */
	nxp_can_mb_range_tst mb_range_st;	/**< Which slots receive and which send, set in rr_can_nxp_initialize() */
	U8                   tx_mb_u8;		/**< The next send slot to use; loops around tx_start_u8..tx_end_u8 */
} nxp_can_state_tst;

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/** Running values for each CAN channel (which slots receive/send, the next send slot, and the
 *  temporary send/receive buffers). The temporary buffers are shared, which is safe only because each
 *  channel is sent to from one place (the wrapper's rr_can_transmit()) and its receive buffer is
 *  refilled from that channel's own callback. */
static nxp_can_state_tst nxp_can_state_ast[CAN_INST_MAX];

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

/* NXP private helpers */
static void rr_can_nxp_compute_mask(U32 id_start_u32, U32 id_end_u32, U32* const id_pu32, U32* const mask_pu32);
static U8   rr_can_nxp_set_mb_range(can_inst_te can_inst_e, U8 can_filters_nos_u8);

/* Functions the NXP CAN driver calls for received messages and for errors */
#if CAN_INSTANCE_0
static void can_0_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
	flexcan_state_t *flexcanState);
static void can_0_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState);
#endif

#if CAN_INSTANCE_1
static void can_1_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
	flexcan_state_t *flexcanState);
static void can_1_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState);
#endif

#if CAN_INSTANCE_2
void can_2_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
	flexcan_state_t *flexcanState);
void can_2_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState);
#endif

/** Fixed settings for each CAN channel. A NULL pal_pst entry means that channel is turned off. Each
 *  row is labelled with its @ref can_inst_te name so the rows stay matched to the channel list. */
static const nxp_can_desc_tst nxp_can_desc_ast[CAN_INST_MAX] =
{
#if CAN_INSTANCE_0
	[CAN1_INST] = { &can_pal_0_instance, &can_pal_0_Config0,
					&can_0_callback_event_v, &can_0_callback_error_v, NXP_CAN_INST0_MAX_MB_NOS },
#endif

#if CAN_INSTANCE_1
	[CAN2_INST] = { &can_pal_1_instance, &can_pal_1_config_0,
					&can_1_callback_event_v, &can_1_callback_error_v, NXP_CAN_INST1_MAX_MB_NOS },
#endif

#if CAN_INSTANCE_2
	[CAN3_INST] = { &can_pal_2_instance, &can_pal_2_config_0,
					&can_2_callback_event_v, &can_2_callback_error_v, NXP_CAN_INST2_MAX_MB_NOS },
#endif
};

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
#if CAN_INSTANCE_0
/**
 * @brief The CAN driver calls this when a message is received on channel 0 (CAN1_INST).
 */
static void can_0_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
							 flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch (eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_RX_COMPLETE:
		{
			(void)rr_can_receive(CAN1_INST, buffIdx_u32);
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}

/**
 * @brief The CAN driver calls this when a bus error happens on channel 0 (CAN1_INST).
 */
static void can_0_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch(eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_ERROR:
		{
			rr_can_error_callback(CAN1_INST, FLEXCAN_DRV_GetErrorStatus((U8)can_pal_0_instance.instIdx));
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}
#endif /* CAN_INSTANCE_0 */

#if CAN_INSTANCE_1
/**
 * @brief The CAN driver calls this when a message is received on channel 1 (CAN2_INST).
 */
static void can_1_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
							 flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch (eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_RX_COMPLETE:
		{
			(void)rr_can_receive(CAN2_INST, buffIdx_u32);
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}

/**
 * @brief The CAN driver calls this when a bus error happens on channel 1 (CAN2_INST).
 */
static void can_1_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch(eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_ERROR:
		{
			rr_can_error_callback(CAN2_INST, FLEXCAN_DRV_GetErrorStatus((U8)can_pal_1_instance.instIdx));
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}
#endif /* CAN_INSTANCE_1 */

#if CAN_INSTANCE_2
/**
 * @brief The CAN driver calls this when a message is received on channel 2 (CAN3_INST, SBC).
 */
void can_2_callback_event_v(U8 instance_u8, flexcan_event_type_t eventType, U32 buffIdx_u32,
							 flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch (eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_RX_COMPLETE:
		{
			(void)rr_can_receive(CAN3_INST, buffIdx_u32);
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}

/**
 * @brief The CAN driver calls this when a bus error happens on channel 2 (CAN3_INST, SBC).
 */
void can_2_callback_error_v(U8 instance_u8, flexcan_event_type_t eventType, flexcan_state_t *flexcanState)
{
	COM_HDR_UNUSED(instance_u8);
	COM_HDR_UNUSED(flexcanState);

	switch(eventType)
	{
		/****************************************************************************************************************/
		case FLEXCAN_EVENT_ERROR:
		{
			rr_can_error_callback(CAN3_INST, FLEXCAN_DRV_GetErrorStatus((U8)can_pal_2_instance.instIdx));
		}
		break;
		/****************************************************************************************************************/
		default:
		{

		}
		break;
		/****************************************************************************************************************/
	}
}
#endif /* CAN_INSTANCE_2 */

/**
 * @brief NXP set-up: decides the receive/send slot split, starts the CAN hardware, and tells it which
 * 		  functions to call when a message is received or an error happens.
 *
 * @param can_inst_e which CAN channel to set up.
 * @param can_filters_nos_u8 how many receive filters to reserve.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_initialize(can_inst_te can_inst_e, U8 can_filters_nos_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const nxp_can_desc_tst* const desc_pst = &nxp_can_desc_ast[can_inst_e];

	if(NULL == desc_pst->pal_pst)	/* this channel is turned off (for example CAN3_INST, the SBC) */
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	/* Give the first can_filters_nos_u8 slots to receiving (0..n-1); the rest are used for sending */
	else if(COM_HDR_RET_ERR == rr_can_nxp_set_mb_range(can_inst_e, can_filters_nos_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		if(CAN_Init(desc_pst->pal_pst, desc_pst->cfg_pst) != STATUS_SUCCESS)
		{
			ret_u8 = COM_HDR_RET_ERR;
		}

		FLEXCAN_DRV_InstallEventCallback((U8)desc_pst->pal_pst->instIdx, desc_pst->event_cb_pf, NULL);
		FLEXCAN_DRV_InstallErrorCallback((U8)desc_pst->pal_pst->instIdx, desc_pst->error_cb_pf, NULL);
	}

	return ret_u8;
}

/**
 * @brief NXP shutdown: turns the CAN hardware for this channel off.
 *
 * @param can_inst_e which CAN channel to shut down.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_deinit(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if(CAN_Deinit(pal_inst_pst) != STATUS_SUCCESS)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Channel shut down successfully; nothing more to do */
	}

	return ret_u8;
}

/**
 * @brief NXP slot set-up: first checks that each filter uses the matching receive slot (entry 0 ->
 * 		  slot 0, and so on), then sets up the receive filters, then sets up the remaining slots for
 * 		  sending.
 *
 * @param can_inst_e which CAN channel to set up.
 * @param cfg_arr_pst list of @ref can_filter_cfg_tst entries, one per reserved receive filter.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_configMailbox(can_inst_te can_inst_e, const can_filter_cfg_tst* const cfg_arr_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 i_u8;
	const can_instance_t* pal_inst_pst = NULL;
	static can_buff_config_t buff_cfg_st;

	pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;
	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Plain CAN (not CAN-FD), 11-bit ID, normal data message; used for both receive and send slots */
		buff_cfg_st.enableFD  = (bool)COM_HDR_FALSE;
		buff_cfg_st.enableBRS = (bool)COM_HDR_FALSE;
		buff_cfg_st.idType    = CAN_MSG_ID_STD;
		buff_cfg_st.isRemote  = (bool)COM_HDR_FALSE;
		buff_cfg_st.fdPadding = 0;

		for(i_u8 = 0; i_u8 < can_filter_cnt_au8[can_inst_e]; i_u8++)
		{
			U32 accept_id_u32 = 0;
			U32 accept_mask_u32 = 0;

			/* For a single ID both ends are id_start; for a range they are id_start and id_end */
			const U32 id_end_u32 = (CAN_FILTER_RANGE == cfg_arr_pst[i_u8].filter_type_e) ?
									cfg_arr_pst[i_u8].id_end_u32 : cfg_arr_pst[i_u8].id_start_u32;

			rr_can_nxp_compute_mask(cfg_arr_pst[i_u8].id_start_u32, id_end_u32, &accept_id_u32, &accept_mask_u32);

			if(CAN_ConfigRxBuff(pal_inst_pst, i_u8, &buff_cfg_st, accept_id_u32) != STATUS_SUCCESS)
			{
				ret_u8 = COM_HDR_RET_ERR;
			}

			if(CAN_SetRxFilter(pal_inst_pst, CAN_MSG_ID_STD, i_u8, accept_mask_u32) != STATUS_SUCCESS)
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}

		/* Set up the remaining slots (tx_start..tx_end) for sending once; sending reuses them as they are */
		for(U8 mb_u8 = nxp_can_state_ast[can_inst_e].mb_range_st.tx_start_u8;
			mb_u8 <= nxp_can_state_ast[can_inst_e].mb_range_st.tx_end_u8; mb_u8++)
		{
			if(CAN_ConfigTxBuff(pal_inst_pst, mb_u8, &buff_cfg_st) != STATUS_SUCCESS)
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}
	}

	return ret_u8;
}

/**
 * @brief NXP start: gets every receive slot ready to take a message.
 *
 * @param can_inst_e which CAN channel to start.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_start(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;
	can_message_t* const rx_buf_pst = &nxp_can_state_ast[can_inst_e].rx_msg_st;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Make each receive slot ready; after a message arrives, the callback makes that slot ready again */
		for(U8 i_u8 = 0; i_u8 < can_filter_cnt_au8[can_inst_e]; i_u8++)
		{
			if(CAN_Receive(pal_inst_pst, i_u8, rx_buf_pst) != STATUS_SUCCESS)
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}
	}

	return ret_u8;
}

/**
 * @brief NXP stop: cancels every waiting receive slot so no more messages come in, and sets the
 * 		  "next send slot" back to the first send slot.
 *
 * @param can_inst_e which CAN channel to stop.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_stop(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Cancel each waiting receive slot so the channel stops receiving */
		for(U8 i_u8 = 0; i_u8 < can_filter_cnt_au8[can_inst_e]; i_u8++)
		{
			if(FLEXCAN_DRV_AbortTransfer((U8)pal_inst_pst->instIdx, i_u8) != STATUS_SUCCESS)
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}

		/* Set the "next send slot" back to the first send slot */
		nxp_can_state_ast[can_inst_e].tx_mb_u8 = nxp_can_state_ast[can_inst_e].mb_range_st.tx_start_u8;
	}

	return ret_u8;
}

/**
 * @brief NXP send: picks the next send slot, copies the message into it, and sends it either waiting
 * 		  until done (blocking) or in the background (interrupt).
 *
 * @param can_inst_e which CAN channel to send on.
 * @param can_tx_msg_st the message to send.
 * @param tx_mode_e wait until done (blocking) or send in the background (interrupt). See
 * 		  @ref can_tx_mode_te.
 * @param timeout_ms_u32 how long to wait, in milliseconds, in blocking mode; not used for
 * 		  @ref CAN_TX_INTERRUPT.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_transmit(can_inst_te can_inst_e, const can_msg_tst* const can_tx_msg_st, can_tx_mode_te tx_mode_e,
						U32 timeout_ms_u32)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	/* can_inst_e was already range-checked by rr_can_transmit() */
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;
	nxp_can_state_tst* state_pst = &nxp_can_state_ast[can_inst_e];
	can_message_t* const tx_msg_pst = &state_pst->tx_msg_st;
	status_t send_sts_e;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	/* No slots left for sending (all slots were given to receiving, or the channel was not started) */
	else if(state_pst->mb_range_st.tx_start_u8 > state_pst->mb_range_st.tx_end_u8)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Make sure the "next send slot" is still inside the send range, in case it moved */
		if((state_pst->tx_mb_u8 < state_pst->mb_range_st.tx_start_u8) ||
		   (state_pst->tx_mb_u8 > state_pst->mb_range_st.tx_end_u8))
		{
			state_pst->tx_mb_u8 = state_pst->mb_range_st.tx_start_u8;
		}

		/* A CAN message holds at most CAN_DATABYTE_MAX bytes, so cap the length, then fill in the message */
		tx_msg_pst->length = (U8)((can_tx_msg_st->length_u8 > (U8)CAN_DATABYTE_MAX) ?
								  (U8)CAN_DATABYTE_MAX : can_tx_msg_st->length_u8);

		tx_msg_pst->id = can_tx_msg_st->id_u32;
		for(U8 i_u8 = 0; i_u8 < tx_msg_pst->length; i_u8++)
		{
			tx_msg_pst->data[i_u8] = can_tx_msg_st->data_au8[i_u8];
		}

		if(CAN_TX_BLOCKING == tx_mode_e)
		{
			send_sts_e = CAN_SendBlocking(pal_inst_pst, state_pst->tx_mb_u8, tx_msg_pst, timeout_ms_u32);
		}
		else
		{
			send_sts_e = CAN_Send(pal_inst_pst, state_pst->tx_mb_u8, tx_msg_pst);
		}

		/* Use this slot now, then move to the next one (wrapping back to the start after the last) */
		state_pst->tx_mb_u8 = (state_pst->tx_mb_u8 >= state_pst->mb_range_st.tx_end_u8)
							? state_pst->mb_range_st.tx_start_u8
							: (U8)(state_pst->tx_mb_u8 + 1U);

		if(STATUS_SUCCESS != send_sts_e)
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/**
 * @brief NXP receive: copies the message that just arrived into @p rx_msg_pst, then makes the receive
 * 		  slot ready for the next message.
 *
 * @param can_inst_e which CAN channel the message arrived on.
 * @param rx_mailbox_u32 the receive slot number reported by the callback.
 * @param rx_msg_pst output: filled in with a copy of the message.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_nxp_receive(can_inst_te can_inst_e, U32 rx_mailbox_u32, can_msg_tst* const rx_msg_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;
	can_message_t* const rx_buf_pst = &nxp_can_state_ast[can_inst_e].rx_msg_st;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Copy the message out first; the next message could arrive and overwrite the shared buffer */
		rx_msg_pst->id_u32 = rx_buf_pst->id;
		rx_msg_pst->length_u8 = (U8)((rx_buf_pst->length > (U8)CAN_DATABYTE_MAX) ? (U8)CAN_DATABYTE_MAX : rx_buf_pst->length);

		for(U8 i_u8 = 0; i_u8 < rx_msg_pst->length_u8; i_u8++)
		{
			rx_msg_pst->data_au8[i_u8] = rx_buf_pst->data[i_u8];
		}

		if(CAN_Receive(pal_inst_pst, rx_mailbox_u32, rx_buf_pst) != STATUS_SUCCESS)
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/**
 * @brief NXP transmit abort: cancels any frame in flight on the channel's transmit mailbox range so a
 * 		  bus error-warning does not leave a stuck pending transfer.
 *
 * @param can_inst_e which CAN channel to abort transmits on; already range-checked by the wrapper.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR if the channel is off or a vendor abort
 * 		   failed (failures do not stop the sweep).
 *
 * @note Aborts the whole transmit mailbox range (tx_start..tx_end); the round-robin send slot means the
 * 		 in-flight frame can be on any of them. When no slots are reserved for sending the range is empty
 * 		 and nothing is aborted.
 */
U8 rr_can_nxp_abortTx(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const can_instance_t* const pal_inst_pst = nxp_can_desc_ast[can_inst_e].pal_pst;
	const nxp_can_mb_range_tst* const mb_range_pst = &nxp_can_state_ast[can_inst_e].mb_range_st;
	U8 mb_u8;

	if(NULL == pal_inst_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Cancel each send slot; tx_start > tx_end means no send slots are reserved, so the loop is skipped */
		for(mb_u8 = mb_range_pst->tx_start_u8; mb_u8 <= mb_range_pst->tx_end_u8; mb_u8++)
		{
			if(STATUS_SUCCESS != FLEXCAN_DRV_AbortTransfer((U8)pal_inst_pst->instIdx, mb_u8))
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}
	}

	return ret_u8;
}

/**
 * @brief NXP error decode: turns the hardware error status (the ESR1 register) into a set of simple
 * 		  error flags (@ref can_error_flag_te).
 *
 * @param error_status_u32 the CAN hardware error status (ESR1 register).
 * @return U8 the matching @ref can_error_flag_te flags.
 *
 * @note The NXP driver clears these error flags by itself in FLEXCAN_Error_IRQHandler() after the
 * 		 callback returns, so we do not need to clear any flag here.
 */
U8 rr_can_nxp_error_decode(U32 error_status_u32)
{
	U8 error_flags_u8 = (U8)CAN_ERR_NONE;

	if((error_status_u32 & CAN_ESR1_TXWRN_MASK) != 0U)
	{
		error_flags_u8 |= (U8)CAN_ERR_TX_WARNING;
	}
	if((error_status_u32 & CAN_ESR1_BOFFINT_MASK) != 0U)
	{
		error_flags_u8 |= (U8)CAN_ERR_BUS_OFF;
	}
	if((error_status_u32 & CAN_ESR1_RXWRN_MASK) != 0U)
	{
		error_flags_u8 |= (U8)CAN_ERR_RX_WARNING;
	}

	return error_flags_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Works out the ID value and the "mask" the hardware needs to accept one ID or a range of IDs.
 * 		  The mask says which ID bits the hardware must check and which it can ignore.
 *
 * @param id_start_u32 the exact ID, or the first ID of a range.
 * @param id_end_u32 the last ID of a range (same as id_start_u32 for a single exact ID).
 * @param id_pu32 output: the ID value to match.
 * @param mask_pu32 output: the mask (a 1 bit means the hardware must check that ID bit).
 *
 * @note Bits that are the same in both IDs are checked; bits that differ are ignored. For a range
 * 		 this gives the closest single ID+mask that covers both ends, so it may also let through some
 * 		 IDs in between. This is just how mask filtering works.
 */
static void rr_can_nxp_compute_mask(U32 id_start_u32, U32 id_end_u32, U32* const id_pu32, U32* const mask_pu32)
{
	/* Where both IDs agree, check the bit; where they differ, ignore it */
	const U32 mask_u32 = (~(id_start_u32 ^ id_end_u32)) & CAN_STD_ID_MASK;

	*mask_pu32 = mask_u32;
	*id_pu32 = id_start_u32 & mask_u32;
}

/**
 * @brief Decides which message slots a channel uses for receiving and which for sending. The first
 * 		  can_filters_nos_u8 slots (0..n-1) are for receiving, the rest are for sending, and the "next
 * 		  send slot" is set to the first send slot.
 *
 * @param can_inst_e which CAN channel.
 * @param can_filters_nos_u8 how many receive slots to reserve.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR if the channel is off or the count is more
 * 		   than the channel has slots.
 */
static U8 rr_can_nxp_set_mb_range(can_inst_te can_inst_e, U8 can_filters_nos_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const U8 mb_max_u8 = nxp_can_desc_ast[can_inst_e].mb_max_u8;

	if((0U == mb_max_u8) || (can_filters_nos_u8 > mb_max_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Slots 0..n-1 receive; slots n..max-1 send; start the "next send slot" at the first send slot */
		nxp_can_state_ast[can_inst_e].mb_range_st.rx_start_u8 = 0;
		nxp_can_state_ast[can_inst_e].mb_range_st.rx_end_u8   = (can_filters_nos_u8 > 0U) ? (U8)(can_filters_nos_u8 - 1U) : 0;
		nxp_can_state_ast[can_inst_e].mb_range_st.tx_start_u8 = can_filters_nos_u8;
		nxp_can_state_ast[can_inst_e].mb_range_st.tx_end_u8   = (U8)(mb_max_u8 - 1U);
		nxp_can_state_ast[can_inst_e].tx_mb_u8 = can_filters_nos_u8;
	}

	return ret_u8;
}

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
