/**
 * @file rr_can.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction CAN driver interface driver.
 * @date 04-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note The functions that other code calls are the same on every chip. Inside each function the
 * 		 \#if defined(STM32) / NXP_S32K144_146 / RENESAS lines pick the code for the chip being built;
 * 		 the vendor CAN calls themselves live in the per-chip port (the NXP path is rr_can_nxp under
 * 		 bsp/nxp). Each function returns OK or NOK (@c COM_HDR_RET_OK / @c COM_HDR_RET_ERR) only to say
 * 		 whether its inputs were valid and the action worked; the driver does not store any error itself.
 * 		 Received messages and decoded bus errors (@ref can_error_flag_te) are handed up to the
 * 		 middleware through callback functions the middleware registers. If the bus turns off
 * 		 (the "bus-off" state), the driver stops and starts the channel again so it can recover.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_can.h"	/* This module's functions, data types and error flags */

#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver headers */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_can_nxp.h"	/* NXP port: rr_can_nxp_* FlexCAN helpers (CAN_ / FLEXCAN_DRV_ calls confined to bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA driver headers */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver macros */

#elif defined(NXP_S32K144_146)

/* @note NXP per-channel mailbox-count macros live in the bsp/nxp port (rr_can_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA CAN driver macros */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver types */

#elif defined(NXP_S32K144_146)

/* @note NXP per-channel descriptor/state types live in the bsp/nxp port (rr_can_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA driver types */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver state */

#elif defined(NXP_S32K144_146)

/* @note NXP per-channel running state (mailbox split, next send slot, shared tx/rx buffers) lives in
 * 		 the bsp/nxp port (rr_can_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA driver state */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/** The callback functions the middleware passed to rr_can_initialize(), one pair per channel. Marked
 *  volatile because normal code sets them while an interrupt may read them. Stored in their real
 *  types, so no function-pointer cast is ever needed (MISRA Rule 11.1). */
static volatile rr_can_rx_cb_t  can_rx_cb_apf[CAN_INST_MAX]  = { NULL };

/** The error callback functions the middleware passed to rr_can_initialize(), one per channel. Same
 *  volatile/typed-storage rationale as can_rx_cb_apf. */
static volatile rr_can_err_cb_t can_err_cb_apf[CAN_INST_MAX] = { NULL };

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/
/** How many receive filters each channel has, remembered from rr_can_initialize(). Filter number i
 *  uses receive slot i (starting at 0), so rr_can_configMailbox() sets up this many filters and
 *  start()/stop() turn slots 0..count-1 on and off. External linkage so the bsp/nxp port can read it
 *  across the seam (extern in rr_can_nxp.h). */
U8 can_filter_cnt_au8[CAN_INST_MAX] = { 0 };

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN private helpers / callbacks */

#elif defined(NXP_S32K144_146)

/* @note NXP versions of each function (the public functions call these) and the SDK-registered
 * 		 receive/error callbacks are declared in rr_can_nxp.h (bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA private helpers / callbacks */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Sets up a CAN channel so it is ready to use, and saves the middleware callback functions
 * 		  the driver will call for received messages and bus errors.
 *
 * @param can_inst_e which CAN channel to set up.
 * @param can_filters_nos_u8 how many receive filters to reserve for this channel. It is remembered here
 * 		  and reused by rr_can_configMailbox(), so you give the count only once.
 * @param rx_cb_pf called with every received message; pass @c NULL for none.
 * @param err_cb_pf called with decoded bus-error flags; pass @c NULL for none.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_initialize(can_inst_te can_inst_e, U8 can_filters_nos_u8, rr_can_rx_cb_t const rx_cb_pf,
					  rr_can_err_cb_t const err_cb_pf)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Save the callbacks before the hardware is set up, so no interrupt can ever see a half-set pair */
		can_rx_cb_apf[can_inst_e]  = rx_cb_pf;
		can_err_cb_apf[can_inst_e] = err_cb_pf;

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_initialize(can_inst_e, can_filters_nos_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif

		/* If set-up worked, remember the filter count; configMailbox / start / stop use it later */
		if(COM_HDR_RET_OK == ret_u8)
		{
			can_filter_cnt_au8[can_inst_e] = can_filters_nos_u8;
		}
	}

	return ret_u8;
}

/**
 * @brief Sets up which message IDs the channel will accept (the receive filters) and the slots used
 * 		  for sending. Receiving does not start yet; that happens in rr_can_start(). The number of
 * 		  filters comes from rr_can_initialize(), so @p cfg_arr_pst must have that many entries.
 * 		  Entry i is used for receive slot i (0, 1, 2, ...). The remaining slots are set up once for
 * 		  sending and then reused by rr_can_transmit().
 *
 * @param can_inst_e which CAN channel to set up.
 * @param cfg_arr_pst list of @ref can_filter_cfg_tst entries (one exact ID, or a range), one per filter.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_configMailbox(can_inst_te can_inst_e, const can_filter_cfg_tst* const cfg_arr_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((NULL == cfg_arr_pst) || (can_inst_e >= CAN_INST_MAX))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_configMailbox(can_inst_e, cfg_arr_pst);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Starts a CAN channel so it can receive messages.
 *
 * @param can_inst_e which CAN channel to start.
 *
 * @note NXP: gets every receive slot ready to take a message; after each message arrives, the
 * 		 receive callback makes that slot ready again.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_start(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_start(can_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);

#else

		COM_HDR_UNUSED(can_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Stops a CAN channel so it no longer receives messages.
 *
 * @param can_inst_e which CAN channel to stop.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_stop(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_stop(can_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);

#else

		COM_HDR_UNUSED(can_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Shuts a CAN channel down and undoes its set-up. The saved callbacks and filter count are
 * 		  cleared, so the channel must go through rr_can_initialize() again before reuse.
 *
 * @param can_inst_e which CAN channel to shut down.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_deinit(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Forget the callbacks and filter count first, so a late interrupt cannot call into stale
		 * middleware code while the hardware is being shut down */
		can_rx_cb_apf[can_inst_e]      = NULL;
		can_err_cb_apf[can_inst_e]     = NULL;
		can_filter_cnt_au8[can_inst_e] = 0;

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_deinit(can_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);

#else

		COM_HDR_UNUSED(can_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Puts a CAN channel into low-power (sleep) mode.
 *
 * @param can_inst_e which CAN channel to put to sleep.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_enterSleep(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(NXP_S32K144_146)

		/* The CAN hardware keeps its settings; low-power is handled by the transceiver chip instead */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);

#else

		COM_HDR_UNUSED(can_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Wakes a CAN channel back up from low-power (sleep) mode.
 *
 * @param can_inst_e which CAN channel to wake.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_wakeup(can_inst_te can_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(NXP_S32K144_146)

		/* The CAN hardware kept its settings during sleep, so there is nothing to set up again here */
		COM_HDR_UNUSED(can_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);

#else

		COM_HDR_UNUSED(can_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Sends one CAN message on the given channel.
 *
 * @param can_inst_e which CAN channel to send on.
 * @param can_tx_msg_st the message to send.
 * @param tx_mode_e wait until done (blocking) or return at once and finish in the background
 * 		  (interrupt). See @ref can_tx_mode_te.
 * @param timeout_ms_u32 how long to wait, in milliseconds, in blocking mode; not used for
 * 		  @ref CAN_TX_INTERRUPT. The value is handed to the chip driver unchanged. 0 does NOT mean
 * 		  "wait forever" - it gives the send no time to finish and will normally report failure, so
 * 		  always pass a real wait time in blocking mode.
 *
 * @note Do not call this for the same channel from two places at once. It updates the "next send slot"
 * 		 without any lock, so always send a given channel from one place only (for example, the CAN
 * 		 send task).
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_can_transmit(can_inst_te can_inst_e, const can_msg_tst* const can_tx_msg_st, can_tx_mode_te tx_mode_e,
					U32 timeout_ms_u32)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((NULL == can_tx_msg_st) || (can_inst_e >= CAN_INST_MAX) || (tx_mode_e >= CAN_TX_MODE_MAX))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(can_inst_e);
		COM_HDR_UNUSED(tx_mode_e);
		COM_HDR_UNUSED(timeout_ms_u32);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_transmit(can_inst_e, can_tx_msg_st, tx_mode_e, timeout_ms_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(can_inst_e);
		COM_HDR_UNUSED(tx_mode_e);
		COM_HDR_UNUSED(timeout_ms_u32);

#else

		COM_HDR_UNUSED(can_inst_e);
		COM_HDR_UNUSED(tx_mode_e);
		COM_HDR_UNUSED(timeout_ms_u32);
		ret_u8 = COM_HDR_RET_ERR;

#endif

	}

	return ret_u8;
}

/**
 * @brief Reads a received CAN message into a local copy and passes it to the registered receive
 * 		  callback.
 *
 * @param can_inst_e which CAN channel the message arrived on.
 * @param rx_mailbox_u32 NXP: the receive slot number reported by the callback.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 *
 * @note External linkage: the bsp/nxp port's SDK-registered receive callbacks reach this generic
 * 		 dispatch helper across the seam (declared in rr_can_nxp.h).
 */
U8 rr_can_receive(can_inst_te can_inst_e, U32 rx_mailbox_u32)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	can_msg_tst rx_msg_st;

	if(can_inst_e >= CAN_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(rx_mailbox_u32);
		ret_u8 = COM_HDR_RET_ERR;

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_can_nxp_receive(can_inst_e, rx_mailbox_u32, &rx_msg_st);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(rx_mailbox_u32);
		ret_u8 = COM_HDR_RET_ERR;

#else

		COM_HDR_UNUSED(rx_mailbox_u32);
		ret_u8 = COM_HDR_RET_ERR;

#endif

		if(COM_HDR_RET_OK == ret_u8)
		{
			rr_can_rx_cb_t const rx_cb_pf = can_rx_cb_apf[can_inst_e];

			if(NULL != rx_cb_pf)
			{
				rx_cb_pf(can_inst_e, &rx_msg_st);
			}
		}
	}

	return ret_u8;
}

/**
 * @brief Turns the CAN error status into simple error flags (@ref can_error_flag_te) and passes them
 * 		  to the registered error callback.
 *
 * @param can_inst_e which CAN channel is reporting the error.
 * @param error_status_u32 NXP: the CAN hardware error status (ESR1 register).
 *
 * @note External linkage: the bsp/nxp port's SDK-registered error callbacks reach this generic
 * 		 dispatch helper across the seam (declared in rr_can_nxp.h).
 * @note On a Tx or Rx error-warning the driver aborts the in-flight transmit so a warning does not
 * 		 leave a stuck pending transfer. If the bus turned off, the driver stops and starts the channel
 * 		 again so receiving and sending work once more. The middleware is still told about the error and
 * 		 decides what else to do.
 */
void rr_can_error_callback(can_inst_te can_inst_e, U32 error_status_u32)
{
	U8 error_flags_u8 = (U8)CAN_ERR_NONE;

	if(can_inst_e < CAN_INST_MAX)
	{

#if defined(STM32)

		/* @note Reserved for STM32 FDCAN implementation */
		COM_HDR_UNUSED(error_status_u32);

#elif defined(NXP_S32K144_146)

		error_flags_u8 = rr_can_nxp_error_decode(error_status_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(error_status_u32);

#else

		COM_HDR_UNUSED(error_status_u32);

#endif

		rr_can_err_cb_t const err_cb_pf = can_err_cb_apf[can_inst_e];

		if(NULL != err_cb_pf)
		{
			err_cb_pf(can_inst_e, error_flags_u8);
		}

		/* Tx / Rx error-warning: abort the in-flight transmit so a warning does not leave a stuck pending
		 * transfer. Best-effort; if it fails the next error interrupt repeats it (MISRA 17.7). */
		if((error_flags_u8 & ((U8)CAN_ERR_TX_WARNING | (U8)CAN_ERR_RX_WARNING)) != 0U)
		{

#if defined(NXP_S32K144_146)

			(void)rr_can_nxp_abortTx(can_inst_e);

#endif

		}

		/* Bus turned off: stop and start the channel to get receiving and sending working again.
		 * Recovery is best-effort; if it fails the next error interrupt repeats it (MISRA 17.7). */
		if((error_flags_u8 & (U8)CAN_ERR_BUS_OFF) != 0U)
		{
			(void)rr_can_stop(can_inst_e);
			(void)rr_can_start(can_inst_e);
		}
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN implementation */

#elif defined(NXP_S32K144_146)

/* @note NXP FlexCAN helper implementations and the SDK-registered callbacks live in the bsp/nxp port (rr_can_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA implementation */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
