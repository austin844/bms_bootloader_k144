/**
 * @file rr_timer_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction Timer driver (FTM output-compare via the S32 SDK TIMING PAL).
 * @date 04-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_timer hardware path, built on the S32 SDK TIMING PAL
 * 		 (TIMING_Init / TIMING_StartChannel / TIMING_StopChannel / TIMING_Deinit). The PAL owns the FTM
 * 		 output-compare datapath AND the channel-expiry NVIC interrupt itself: every configured channel's
 * 		 callback is installed once at TIMING_Init and is invoked directly by the PAL from the real FTM
 * 		 ISR, which already re-arms continuous channels (relative reschedule, matching the previous
 * 		 free-running behaviour) and auto-stops one-shot channels before the callback runs. This port
 * 		 therefore only builds the PAL configuration (from the wrapper-injected config) and provides the
 * 		 single per-channel PAL callback that recovers (instance, channel) and dispatches to the
 * 		 wrapper-mirrored per-channel callback. The wrapper (rr_timer.c) owns argument/NULL/range
 * 		 validation and the injected config/callback mirror tables (read across the seam through their
 * 		 extern view in rr_timer_nxp.h); this port owns the vendor PAL calls.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_timer_nxp.h"	/* This port's helper API, the wrapper's public types and the mirror-table extern view */

#if defined(NXP_S32K144_146)

#include "timing_pal.h"	/* S32 SDK TIMING PAL: TIMING_Init/StartChannel/StopChannel/Deinit/GetResolution */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
/* Factor for converting a period expressed in milliseconds to nanoseconds. The millisecond-to-tick
 * conversion below is done against the PAL's NANOSECOND resolution rather than its microsecond
 * resolution: on a fast FTM functional clock (e.g. a fine prescaler on a multi-MHz counter clock) the
 * per-tick period is sub-microsecond, so TIMING_GetResolution() truncates the microsecond resolution to
 * zero and reports STATUS_ERROR, needlessly rejecting an otherwise valid configuration. Nanosecond
 * resolution stays non-zero across the whole supported clock/prescaler range and yields the same tick
 * counts for coarse prescalers, so it is strictly more capable. */
#define TIMER_MS_TO_NS_DIVISOR	(1000000ULL)	/*!< Multiplier applied when converting a period(ms) to nanoseconds */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
/**
 * @brief Stable (instance, channel) tag handed to the PAL as a channel's callbackParam, so the single
 * 		  shared PAL callback (which only receives a @c void* ) can recover which channel expired without
 * 		  an int-to-pointer cast.
 */
typedef struct
{
	timer_inst_te inst_e;	/**< Timer instance this channel tag belongs to */
	U8            chan_u8;	/**< Channel index within that instance */
} nxp_timer_chan_tag_tst;

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/** Index-not-address: resolves a @ref timer_inst_te entry to its PAL instance descriptor (FTM type + hw
 *  index). Built entirely from build-time RR_TIMER_* config, so it is const; a slot outside the
 *  #if TIMER_INSTANCE_n guard below is never read (see @ref nxp_timer_inst_present_ab). */
static const timing_instance_t nxp_timer_pal_inst_ast[TIMER_INST_MAX] =
{
#if TIMER_INSTANCE_0
	[TIMER1_INST] = { TIMING_INST_TYPE_FTM, RR_TIMER_INST0_HW },
#endif
#if TIMER_INSTANCE_1
	[TIMER2_INST] = { TIMING_INST_TYPE_FTM, RR_TIMER_INST1_HW },
#endif
};

/** Index-not-address: COM_HDR_TRUE for every Timer instance turned on at build time, mirroring the
 *  #if TIMER_INSTANCE_n guards above; used to reject calls against a disabled instance. */
static const BOOL nxp_timer_inst_present_ab[TIMER_INST_MAX] =
{
#if TIMER_INSTANCE_0
	[TIMER1_INST] = COM_HDR_TRUE,
#endif
#if TIMER_INSTANCE_1
	[TIMER2_INST] = COM_HDR_TRUE,
#endif
};

/** PAL FTM clock/prescaler/period extension per instance, built once at rr_timer_nxp_initialize() from the
 *  wrapper-injected @ref timer_ftm_ext_tst. */
static extension_ftm_for_timer_t nxp_timer_pal_ext_ast[TIMER_INST_MAX];

/** PAL top-level per-instance timer configuration, built once at rr_timer_nxp_initialize(). */
static timer_config_t nxp_timer_pal_cfg_ast[TIMER_INST_MAX];

/** PAL per-channel configuration entries, built once at rr_timer_nxp_initialize() from the
 *  wrapper-injected channel config table; every entry's callback is this port's shared PAL callback. */
static timer_chan_config_t nxp_timer_pal_chan_aast[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT];

/** Per-channel (instance, channel) tags handed to the PAL as callbackParam; filled alongside the PAL
 *  channel config above so the shared PAL callback can recover the firing channel by pointer alone. */
static nxp_timer_chan_tag_tst nxp_timer_chan_tag_aast[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT];

/** Each channel's period in ticks, resolved once at rr_timer_nxp_initialize() from the injected
 *  milliseconds period and the PAL's post-init tick resolution; 0 marks a channel that was rejected at
 *  init (period out of the FTM's 16-bit range) and must not be started. */
static U32 nxp_timer_period_ticks_aau32[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT];

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
static void rr_timer_nxp_pal_cb(void* user_data_pv);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief NXP set-up: builds the TIMING PAL configuration for one instance from the middleware-injected
 * 		  channel config (clock/prescaler/final-value extension, per-channel notification type and this
 * 		  port's shared PAL callback), hands it to TIMING_Init, then resolves every channel's period from
 * 		  milliseconds to ticks using the PAL's own post-init tick resolution. Channels are not started
 * 		  (no compare match can fire) until @ref rr_timer_nxp_start_base_v runs.
 *
 * @param timer_inst_e which Timer instance to set up.
 * @param chan_cfg_pst list of @ref timer_chan_cfg_tst entries (period_ms_u32 per channel).
 * @param chan_cnt_u8 how many entries are in @p chan_cfg_pst.
 * @param ext_pst FTM clock source / prescaler / MOD extension.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 *
 * @note The FTM compare register is 16 bits wide (max @c COM_HDR_MAX_U16 ticks); a period that resolves
 * 		 to more ticks than that, or to zero ticks, is rejected for that channel without aborting the
 * 		 remaining channels (OR'd fault, matching the rr_can init-loop convention); the channel is simply
 * 		 left un-started by @ref rr_timer_nxp_start_base_v / @ref rr_timer_nxp_arm_channel.
 */
U8 rr_timer_nxp_initialize(timer_inst_te timer_inst_e, const timer_chan_cfg_tst* const chan_cfg_pst, U8 chan_cnt_u8,
							const timer_ftm_ext_tst* const ext_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 i_u8;
	extension_ftm_for_timer_t* const pal_ext_pst = &nxp_timer_pal_ext_ast[timer_inst_e];
	timer_config_t* const pal_cfg_pst = &nxp_timer_pal_cfg_ast[timer_inst_e];
	ftm_clock_ps_t presc_e;
	ftm_clock_source_t clk_src_e;
	U64 resolution_ns_u64 = 0ULL;

	if(COM_HDR_TRUE != nxp_timer_inst_present_ab[timer_inst_e])	/* this instance is turned off */
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		/* translate the injected vendor-neutral prescaler token to the FTM clock prescaler */
		switch(ext_pst->prescaler_e)
		{
			case TIMER_PRESCALE_2:
			{
				presc_e = FTM_CLOCK_DIVID_BY_2;
			}
			break;

			case TIMER_PRESCALE_4:
			{
				presc_e = FTM_CLOCK_DIVID_BY_4;
			}
			break;

			case TIMER_PRESCALE_8:
			{
				presc_e = FTM_CLOCK_DIVID_BY_8;
			}
			break;

			case TIMER_PRESCALE_16:
			{
				presc_e = FTM_CLOCK_DIVID_BY_16;
			}
			break;

			case TIMER_PRESCALE_32:
			{
				presc_e = FTM_CLOCK_DIVID_BY_32;
			}
			break;

			case TIMER_PRESCALE_64:
			{
				presc_e = FTM_CLOCK_DIVID_BY_64;
			}
			break;

			case TIMER_PRESCALE_128:
			{
				presc_e = FTM_CLOCK_DIVID_BY_128;
			}
			break;

			case TIMER_PRESCALE_1:
			default:
			{
				presc_e = FTM_CLOCK_DIVID_BY_1;	/* divide-by-1 (legacy default) */
			}
			break;
		}

		/* translate the injected vendor-neutral clock-source token to the FTM clock source */
		switch(ext_pst->clockSelect_e)
		{
			case TIMER_CLK_SRC_FIXED:
			{
				clk_src_e = FTM_CLOCK_SOURCE_FIXEDCLK;
			}
			break;

			case TIMER_CLK_SRC_EXTERNAL:
			{
				clk_src_e = FTM_CLOCK_SOURCE_EXTERNALCLK;
			}
			break;

			case TIMER_CLK_SRC_SYSTEM:
			default:
			{
				clk_src_e = FTM_CLOCK_SOURCE_SYSTEMCLK;	/* system clock (legacy default) */
			}
			break;
		}

		pal_ext_pst->clockSelect = clk_src_e;
		pal_ext_pst->prescaler   = presc_e;
		pal_ext_pst->finalValue  = ext_pst->finalValue_u16;

		/* Build every configured channel's PAL descriptor and stable (instance, channel) tag.
		 * The PAL consumes chanConfigArray[0 .. numChan-1] packed by array position and keys the hardware
		 * channel off each entry's .channel field, so the descriptors are packed by the loop counter i_u8
		 * (NOT by channel number): a non-contiguous channel set such as {0, 2} must still occupy array
		 * slots 0 and 1, or the PAL would read an unwritten slot and never configure the second channel.
		 * The (instance, channel) tag stays keyed by channel number so the ISR callback recovers the
		 * firing channel correctly from its callbackParam. */
		for(i_u8 = 0; i_u8 < chan_cnt_u8; i_u8++)
		{
			const U32 chan_u32 = chan_cfg_pst[i_u8].channel_u8;
			timer_chan_config_t* const pal_chan_pst = &nxp_timer_pal_chan_aast[timer_inst_e][i_u8];
			nxp_timer_chan_tag_tst* const tag_pst = &nxp_timer_chan_tag_aast[timer_inst_e][chan_u32];

			tag_pst->inst_e  = timer_inst_e;
			tag_pst->chan_u8 = (U8)chan_u32;

			if(TIMER_CHAN_MODE_ONESHOT == chan_cfg_pst[i_u8].chanType_e)
			{
				pal_chan_pst->chanType = TIMER_CHAN_TYPE_ONESHOT;
			}
			else
			{
				pal_chan_pst->chanType = TIMER_CHAN_TYPE_CONTINUOUS;
			}

			pal_chan_pst->channel       = (uint8_t)chan_u32;
			pal_chan_pst->callback      = &rr_timer_nxp_pal_cb;
			pal_chan_pst->callbackParam = (void*)tag_pst;
		}

		pal_cfg_pst->chanConfigArray = nxp_timer_pal_chan_aast[timer_inst_e];
		pal_cfg_pst->numChan         = chan_cnt_u8;
		pal_cfg_pst->extension       = pal_ext_pst;

		if(STATUS_SUCCESS != TIMING_Init(&nxp_timer_pal_inst_ast[timer_inst_e], pal_cfg_pst))
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		if(STATUS_SUCCESS != TIMING_GetResolution(&nxp_timer_pal_inst_ast[timer_inst_e], TIMER_RESOLUTION_TYPE_NANOSECOND,
												   &resolution_ns_u64))
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		/* Resolve every configured channel's period from milliseconds to ticks and cache it for
		 * rr_timer_nxp_start_base_v() / rr_timer_nxp_arm_channel(); a channel that resolves to zero or
		 * more than COM_HDR_MAX_U16 ticks is left at 0 (rejected, never started) without touching the
		 * remaining channels */
		for(i_u8 = 0; i_u8 < chan_cnt_u8; i_u8++)
		{
			const U32 chan_u32 = chan_cfg_pst[i_u8].channel_u8;
			const U64 period_ns_u64 = (U64)chan_cfg_pst[i_u8].period_ms_u32 * TIMER_MS_TO_NS_DIVISOR;
			const U64 period_ticks_u64 = period_ns_u64 / resolution_ns_u64;

			if((0ULL == period_ticks_u64) || (period_ticks_u64 > (U64)COM_HDR_MAX_U16))
			{
				ret_u8 = COM_HDR_RET_ERR;
				nxp_timer_period_ticks_aau32[timer_inst_e][chan_u32] = 0U;
			}
			else
			{
				nxp_timer_period_ticks_aau32[timer_inst_e][chan_u32] = (U32)period_ticks_u64;
			}
		}
	}

	return ret_u8;
}

/**
 * @brief NXP shutdown: deinitializes the TIMING PAL instance (which stops every channel's notification)
 * 		  and clears the cached per-channel tick periods.
 *
 * @param timer_inst_e which Timer instance to shut down.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on failure.
 */
U8 rr_timer_nxp_deinit(timer_inst_te timer_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 i_u8;

	if(COM_HDR_TRUE != nxp_timer_inst_present_ab[timer_inst_e])
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	if(COM_HDR_RET_OK == ret_u8)
	{
		TIMING_Deinit(&nxp_timer_pal_inst_ast[timer_inst_e]);

		for(i_u8 = 0; i_u8 < TIMER_CHANNEL_MAX_COUNT; i_u8++)
		{
			nxp_timer_period_ticks_aau32[timer_inst_e][i_u8] = 0U;
		}
	}

	return ret_u8;
}

/**
 * @brief NXP start: starts every configured channel that resolved to a valid tick period at
 * 		  @ref rr_timer_nxp_initialize, so its first (and, for a continuous channel, every subsequent)
 * 		  compare match raises the real FTM interrupt the PAL installed, which invokes
 * 		  @ref rr_timer_nxp_pal_cb directly for that channel.
 *
 * @param timer_inst_e which Timer instance to start.
 */
void rr_timer_nxp_start_base_v(timer_inst_te timer_inst_e)
{
	U8 i_u8;

	for(i_u8 = 0; i_u8 < timer_chan_cnt_au8[timer_inst_e]; i_u8++)
	{
		const U8 chan_u8 = timer_cfg_pst[timer_inst_e][i_u8].channel_u8;
		const U32 period_ticks_u32 = nxp_timer_period_ticks_aau32[timer_inst_e][chan_u8];

		if(0U != period_ticks_u32)
		{
			TIMING_StartChannel(&nxp_timer_pal_inst_ast[timer_inst_e], chan_u8, period_ticks_u32);
		}
		else
		{
			/* @note Channel rejected at init (period out of range); left un-started */
		}
	}
}

/**
 * @brief NXP stop: stops every configured channel's notification, silencing all channel-expiry callbacks
 * 		  without touching the FTM instance's clock/period/output-compare set-up.
 *
 * @param timer_inst_e which Timer instance to stop.
 */
void rr_timer_nxp_stop_base_v(timer_inst_te timer_inst_e)
{
	U8 i_u8;

	for(i_u8 = 0; i_u8 < timer_chan_cnt_au8[timer_inst_e]; i_u8++)
	{
		const U8 chan_u8 = timer_cfg_pst[timer_inst_e][i_u8].channel_u8;

		TIMING_StopChannel(&nxp_timer_pal_inst_ast[timer_inst_e], chan_u8);
	}
}

/**
 * @brief NXP per-channel arm: (re)starts the channel's notification with its configured period, matching
 * 		  @ref rr_timer_nxp_start_base_v's first arm.
 *
 * @param timer_inst_e which Timer instance owns the channel.
 * @param chan_idx_u8 channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) to arm.
 * @return U8 @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR if the channel has no valid cached period.
 */
U8 rr_timer_nxp_arm_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	const U32 period_ticks_u32 = nxp_timer_period_ticks_aau32[timer_inst_e][chan_idx_u8];

	if(0U == period_ticks_u32)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		TIMING_StartChannel(&nxp_timer_pal_inst_ast[timer_inst_e], chan_idx_u8, period_ticks_u32);
	}

	return ret_u8;
}

/**
 * @brief NXP per-channel cancel: stops only this channel's notification, leaving every other configured
 * 		  channel on the instance running untouched.
 *
 * @param timer_inst_e which Timer instance owns the channel.
 * @param chan_idx_u8 channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) to cancel.
 * @return U8 always @c COM_HDR_RET_OK (the PAL stop call reports no failure status).
 */
U8 rr_timer_nxp_cancel_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8)
{
	TIMING_StopChannel(&nxp_timer_pal_inst_ast[timer_inst_e], chan_idx_u8);

	return COM_HDR_RET_OK;
}

/**
 * @brief NXP channel-expiry seam. With the TIMING PAL, channel-expiry notification is delivered directly
 * 		  from the real FTM NVIC ISR (installed internally by the PAL) to @ref rr_timer_nxp_pal_cb for the
 * 		  firing channel; this function is no longer the dispatch path but is kept to preserve the public
 * 		  rr_timer ISR-seam contract (@ref rr_timer_irqHandler still calls it).
 *
 * @param timer_inst_e which Timer instance raised the interrupt (unused).
 */
void rr_timer_nxp_irqHandler(timer_inst_te timer_inst_e)
{
	COM_HDR_UNUSED(timer_inst_e);
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief TIMING PAL per-channel callback (runs in real FTM interrupt context). Recovers the firing
 * 		  (instance, channel) pair from the callback parameter and dispatches to the wrapper-mirrored
 * 		  per-channel callback; the PAL has already re-armed a continuous channel or auto-stopped a
 * 		  one-shot channel before invoking this callback, so no rescheduling is done here.
 *
 * @param user_data_pv points at the firing channel's @ref nxp_timer_chan_tag_tst entry.
 */
static void rr_timer_nxp_pal_cb(void* user_data_pv)
{
	if(NULL != user_data_pv)
	{
		const nxp_timer_chan_tag_tst* const tag_pst = (const nxp_timer_chan_tag_tst*)user_data_pv;

		/* Single volatile snapshot (TOCTOU guard): a concurrent rr_timer_deinit() may clear this slot
		 * right after the read, but never leaves a half-written function pointer */
		rr_timer_cb_t const cb_pf = timer_cb_aapf[tag_pst->inst_e][tag_pst->chan_u8];

		if(NULL != cb_pf)
		{
			cb_pf(tag_pst->inst_e, tag_pst->chan_u8);
		}
		else
		{
			/* @note No middleware callback registered for this channel */
		}
	}
	else
	{
		/* @note Malformed notification; nothing to dispatch */
	}
}

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
