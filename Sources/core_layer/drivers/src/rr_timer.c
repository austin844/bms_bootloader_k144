/**
 * @file rr_timer.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction Timer driver interface driver.
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note The functions that other code calls are the same on every chip. Inside each function the
 * 		 \#if defined(STM32) / NXP_S32K144_146 / RENESAS lines pick the code for the chip being built.
 * 		 Each function returns OK or NOK (@c COM_HDR_RET_OK / @c COM_HDR_RET_ERR) only to say whether its
 * 		 inputs were valid and the action worked; the driver does not store any error itself.
 * 		 Channel-expiry events are handed up to the middleware through the per-channel callback each
 * 		 channel was configured with; the driver does not fan a single event out to more than one
 * 		 callback. A channel's @c chanType_e decides what happens to it after that callback runs:
 * 		 @c TIMER_CHAN_MODE_CONTINUOUS channels reschedule themselves for another period;
 * 		 @c TIMER_CHAN_MODE_ONESHOT channels auto-stop instead, and stay stopped until re-armed with
 * 		 @c rr_timer_arm_channel.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_timer.h"	/* This module's functions, data types and channel config */

#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_timer_nxp.h"	/* NXP port: rr_timer_nxp_* FTM output-compare helpers (FTM_DRV_* confined to bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP FTM channel interrupt/flag bit macros live in the bsp/nxp port (rr_timer_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP FTM descriptor/state typedefs live in the bsp/nxp port (rr_timer_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP FTM base LUT, cached state and channel bit tables live in the bsp/nxp port (rr_timer_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/
/** The channel config table the middleware passed to rr_timer_init_u8(), one pointer + count per
 *  instance. The driver never owns this table; middleware sizes and keeps it (config-injection seam).
 *  Wrapper-owned; external linkage only so the NXP port (rr_timer_nxp.c) can walk the configured channels. */
const timer_chan_cfg_tst* timer_cfg_pst[TIMER_INST_MAX]     = { NULL };
U8                        timer_chan_cnt_au8[TIMER_INST_MAX] = { 0 };

/** The per-channel callback functions mirrored from the injected config table, so the ISR seam can
 *  snapshot them without walking the middleware-owned table. Marked volatile because normal code sets
 *  them while an interrupt may read them. Wrapper-owned; external linkage only so the NXP port's ISR seam
 *  can snapshot them. */
volatile rr_timer_cb_t timer_cb_aapf[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT] = { { NULL } };

/** Each channel's operating mode (@ref timer_chan_mode_te), mirrored from the injected config table at
 *  @ref rr_timer_init_u8 so the ISR seam and the per-channel arm/cancel API can tell continuous from
 *  one-shot channels by index alone, without walking the middleware-owned table. Wrapper-owned; external
 *  linkage only so the NXP port's ISR seam can read the mode by index. */
timer_chan_mode_te timer_chan_type_aae[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT] = { { TIMER_CHAN_MODE_CONTINUOUS } };

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP versions of each function (the rr_timer_nxp_* helpers the public functions call) are declared in
 * 		 rr_timer_nxp.h and defined in the bsp/nxp port (rr_timer_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Sets up a Timer instance so it is ready to use: saves the injected per-channel config and
 * 		  callbacks, then configures the FTM instance's clock/prescaler/period and every channel's
 * 		  output-compare mode. Channels are NOT armed here; call @ref rr_timer_start_base_v afterwards.
 *
 * @param timer_inst_e which Timer instance to set up.
 * @param chan_cfg_apst list of @ref timer_chan_cfg_tst entries, one per channel; middleware sizes and
 * 		  owns this array (config-injection seam), the driver only stores a const pointer + count.
 * @param chan_cnt_u8 how many entries are in @p chan_cfg_apst (must be <= @ref TIMER_CHANNEL_MAX_COUNT).
 * @param ext_pst NXP: FTM clock source / prescaler / MOD extension (see @ref timer_ftm_ext_tst).
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_timer_init_u8(timer_inst_te timer_inst_e, const timer_chan_cfg_tst* const chan_cfg_apst, U8 chan_cnt_u8,
					 const timer_ftm_ext_tst* const ext_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 i_u8;

	if((NULL == chan_cfg_apst) || (NULL == ext_pst) || (timer_inst_e >= TIMER_INST_MAX) ||
	   (chan_cnt_u8 > TIMER_CHANNEL_MAX_COUNT))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		/* Save the config table and mirror the per-channel callbacks before the hardware is set up, so no
		 * interrupt can ever see a half-set table */
		timer_cfg_pst[timer_inst_e]     = chan_cfg_apst;
		timer_chan_cnt_au8[timer_inst_e] = chan_cnt_u8;

		for(i_u8 = 0; i_u8 < chan_cnt_u8; i_u8++)
		{
			if((chan_cfg_apst[i_u8].channel_u8 < TIMER_CHANNEL_MAX_COUNT) &&
			   (chan_cfg_apst[i_u8].chanType_e < TIMER_CHAN_MODE_MAX))
			{
				timer_cb_aapf[timer_inst_e][chan_cfg_apst[i_u8].channel_u8]     = chan_cfg_apst[i_u8].callback_pf;
				timer_chan_type_aae[timer_inst_e][chan_cfg_apst[i_u8].channel_u8] = chan_cfg_apst[i_u8].chanType_e;
			}
			else
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
		}
	}

	if(COM_HDR_RET_OK != ret_u8)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		ret_u8 = COM_HDR_RET_ERR;

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_timer_nxp_initialize(timer_inst_e, chan_cfg_apst, chan_cnt_u8, ext_pst);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		ret_u8 = COM_HDR_RET_ERR;

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif
	}

	return ret_u8;
}

/**
 * @brief Shuts a Timer instance down and undoes its set-up. The saved config table, channel count and
 * 		  callbacks are cleared, so the instance must go through rr_timer_init_u8() again before reuse.
 *
 * @param timer_inst_e which Timer instance to shut down.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_timer_deinit(timer_inst_te timer_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 i_u8;

	if(timer_inst_e >= TIMER_INST_MAX)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		/* Forget the callbacks and config table first, so a late interrupt cannot call into stale
		 * middleware code while the hardware is being shut down */
		for(i_u8 = 0; i_u8 < TIMER_CHANNEL_MAX_COUNT; i_u8++)
		{
			timer_cb_aapf[timer_inst_e][i_u8]      = NULL;
			timer_chan_type_aae[timer_inst_e][i_u8] = TIMER_CHAN_MODE_CONTINUOUS;
		}

		timer_cfg_pst[timer_inst_e]     = NULL;
		timer_chan_cnt_au8[timer_inst_e] = 0;

#if defined(STM32)

		/* @note Reserved for STM32 build support */
		COM_HDR_UNUSED(timer_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_timer_nxp_deinit(timer_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		COM_HDR_UNUSED(timer_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#else

		COM_HDR_UNUSED(timer_inst_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif
	}

	return ret_u8;
}

/**
 * @brief Arms every configured channel on a Timer instance with its configured period and enables the
 * 		  per-channel compare interrupt. Must be called after a successful @ref rr_timer_init_u8.
 *
 * @param timer_inst_e which Timer instance to start.
 *
 * @note Silently does nothing if @p timer_inst_e is invalid or the instance was never initialized
 * 		 (@ref timer_cfg_pst entry NULL), matching the void return of this API.
 */
void rr_timer_start_base_v(timer_inst_te timer_inst_e)
{
	if((timer_inst_e >= TIMER_INST_MAX) || (timer_cfg_pst[timer_inst_e] == NULL))
	{
		/* invalid instance or never initialized: silently do nothing (void API) */
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		COM_HDR_UNUSED(timer_inst_e);

#elif defined(NXP_S32K144_146)

		rr_timer_nxp_start_base_v(timer_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		COM_HDR_UNUSED(timer_inst_e);

#else

		COM_HDR_UNUSED(timer_inst_e);

#endif
	}
}

/**
 * @brief Disables the compare interrupt for every configured channel on a Timer instance, silencing all
 * 		  channel-expiry callbacks without deinitializing the FTM instance. Complement of
 * 		  @ref rr_timer_start_base_v; the instance can be restarted later with another call to
 * 		  @ref rr_timer_start_base_v.
 *
 * @param timer_inst_e which Timer instance to stop.
 *
 * @note Silently does nothing if @p timer_inst_e is invalid or the instance was never initialized
 * 		 (@ref timer_cfg_pst entry NULL), matching the void return of this API.
 */
void rr_timer_stop_base_v(timer_inst_te timer_inst_e)
{
	if((timer_inst_e >= TIMER_INST_MAX) || (timer_cfg_pst[timer_inst_e] == NULL))
	{
		/* invalid instance or never initialized: silently do nothing (void API) */
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		COM_HDR_UNUSED(timer_inst_e);

#elif defined(NXP_S32K144_146)

		rr_timer_nxp_stop_base_v(timer_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		COM_HDR_UNUSED(timer_inst_e);

#else

		COM_HDR_UNUSED(timer_inst_e);

#endif
	}
}

/**
 * @brief Arms a single channel on a Timer instance: schedules its next compare match one configured
 * 		  period from now and enables that channel's compare interrupt. Intended to (re-)start a
 * 		  @ref TIMER_CHAN_MODE_ONESHOT channel (which auto-stops itself in the ISR seam after firing), but
 * 		  is also valid on a @ref TIMER_CHAN_MODE_CONTINUOUS channel.
 *
 * @param timer_inst_e which Timer instance owns the channel.
 * @param chan_idx_u8 channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) to arm; must be one of the
 * 		  channels passed to @ref rr_timer_init_u8 for this instance.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_timer_arm_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((timer_inst_e >= TIMER_INST_MAX) || (timer_cfg_pst[timer_inst_e] == NULL) ||
	   (chan_idx_u8 >= TIMER_CHANNEL_MAX_COUNT))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		ret_u8 = COM_HDR_RET_ERR;

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_timer_nxp_arm_channel(timer_inst_e, chan_idx_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		ret_u8 = COM_HDR_RET_ERR;

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif
	}

	return ret_u8;
}

/**
 * @brief Cancels a single channel on a Timer instance by disabling its compare interrupt, without
 * 		  touching any other channel. Intended to cancel a @ref TIMER_CHAN_MODE_ONESHOT channel before it
 * 		  fires, but is also valid on a @ref TIMER_CHAN_MODE_CONTINUOUS channel.
 *
 * @param timer_inst_e which Timer instance owns the channel.
 * @param chan_idx_u8 channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) to cancel; must be one of the
 * 		  channels passed to @ref rr_timer_init_u8 for this instance.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_timer_cancel_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((timer_inst_e >= TIMER_INST_MAX) || (timer_cfg_pst[timer_inst_e] == NULL) ||
	   (chan_idx_u8 >= TIMER_CHANNEL_MAX_COUNT))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		ret_u8 = COM_HDR_RET_ERR;

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_timer_nxp_cancel_channel(timer_inst_e, chan_idx_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		ret_u8 = COM_HDR_RET_ERR;

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif
	}

	return ret_u8;
}

/**
 * @brief Timer channel-expiry ISR seam. Reads the hardware compare flags for every configured channel,
 * 		  clears each set flag and reschedules that channel's next period, then dispatches to the single
 * 		  middleware callback registered for that channel.
 *
 * @param timer_inst_e which Timer instance raised the interrupt.
 *
 * @note This module does not touch the NVIC; the consuming project's vector table calls this function
 * 		 directly (or through rr_nvic once that module exists). Each channel's flag is cleared and its
 * 		 next compare value rescheduled before its callback is invoked, and the callback pointer is taken
 * 		 as a single volatile snapshot (TOCTOU guard) so a concurrent rr_timer_deinit() cannot race a
 * 		 half-read function pointer.
 */
void rr_timer_irqHandler(timer_inst_te timer_inst_e)
{
	if(timer_inst_e >= TIMER_INST_MAX)
	{
		/* invalid instance: silently do nothing (void API) */
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 build support */
		COM_HDR_UNUSED(timer_inst_e);

#elif defined(NXP_S32K144_146)

		rr_timer_nxp_irqHandler(timer_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */
		COM_HDR_UNUSED(timer_inst_e);

#else

		COM_HDR_UNUSED(timer_inst_e);

#endif
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP versions of each function (the rr_timer_nxp_* helpers the public functions call above) are
 * 		 defined in the bsp/nxp port (rr_timer_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
