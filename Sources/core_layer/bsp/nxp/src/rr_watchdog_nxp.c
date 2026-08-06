/**
 * @file rr_watchdog_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction internal watchdog driver (WDG PAL over the WDOG peripheral).
 * @date 04-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_watchdog hardware path, built on the S32 SDK WDG PAL
 * 		 (WDG_Init / WDG_Refresh / WDG_SetTimeout / WDG_Deinit / WDG_ClearIntFlag) over the WDOG peripheral.
 * 		 The PAL owns the vendor register unlock/refresh sequencing; this port builds the vendor configuration
 * 		 once at initialise from the wrapper-injected rr_watchdog configuration plus fixed design constants,
 * 		 and drives the single internal watchdog through a compact instance descriptor
 * 		 ({ WDG_INST_TYPE_WDOG, 0 }, index-not-address). The wrapper (rr_watchdog.c) owns argument validation,
 * 		 the stored configuration pointers and the callback slots; this port owns the vendor PAL calls, the
 * 		 millisecond-to-tick conversion for runtime timeout changes and the early-warning interrupt seam,
 * 		 reading the registered callback across the seam through the extern in rr_watchdog_nxp.h.
 *
 * 		 Behaviour-gating mode changes are applied through the PAL deinit+init reconfiguration path (the WDG
 * 		 PAL has no runtime mode setter): the per-instance vendor configuration is kept in RAM so a mode or
 * 		 timeout change updates it in place before re-initialising.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_watchdog_nxp.h"	/* This port's helper API, the wrapper's public types and the cross-seam extern
								   declaration */

#if defined(NXP_S32K144_146)

#include "wdg_pal.h"	/* S32 SDK WDG PAL: WDG_Init / WDG_Refresh / WDG_SetTimeout / WDG_Deinit / WDG_ClearIntFlag */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/
#include "core_layer/config/generated_code/vcu/inc/rr_hw_cfg.h"	/* Consolidated build-time hardware config: RR_WDOG_INTERNAL_TIMEOUT */

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/** WDG PAL bindings for the internal WDOG instance: the clock source and prescaler are fixed design
 *  choices; the timeout, window opening, window-enable and interrupt-enable are driven by the injected
 *  per-call configuration (@ref wdog_cfg_tst). The rr_hw_cfg.h values below are used only as fall-backs. */
#define WATCHDOG_NXP_CLK_SOURCE			(WDG_PAL_LPO_CLOCK)			/*!< WDOG counter clock: LPO (441.40625 Hz after the fixed 256 prescale) */
#define WATCHDOG_NXP_TIMEOUT_TICKS		(RR_WDOG_INTERNAL_TIMEOUT)	/*!< Fall-back reset timeout (ticks) when the caller supplies no timeout */
#define WATCHDOG_NXP_WINDOW_PERCENT		(RR_WDOG_WINDOW_PERCENT)	/*!< Fall-back window opening, % of timeout, if the derived opening is out of range */
#define WATCHDOG_NXP_PRESCALER_ENABLE	(true)		/*!< Fixed 256 clock prescaler enabled (matches the LPO tick constant) */
#define WATCHDOG_NXP_PERCENT_FULL		(100U)		/*!< Whole-timeout reference for the window-ms -> window-% conversion */

#define WATCHDOG_NXP_LPO_PRESCALED_HZ_X100000	(44140625UL)	/*!< LPO-after-256-prescale frequency, Hz x100000
																	 (441.40625 Hz) */
#define WATCHDOG_NXP_MS_PER_S			(1000UL)	/*!< Milliseconds per second, for the ms->tick conversion */
#define WATCHDOG_NXP_TICKS_MIN_U32		(157UL)		/*!< Minimum accepted timeout, in ticks (~355 ms) */
#define WATCHDOG_NXP_TICKS_MAX_U32		(65535UL)	/*!< Maximum accepted timeout, in ticks (16-bit register limit) */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/** @brief Instance index -> WDG PAL instance descriptor (index-not-address). The internal watchdog is
 * 		   realised over the WDOG peripheral (instance 0); the PAL takes this descriptor by pointer on every
 * 		   call. */
static const wdg_instance_t wdog_nxp_inst_ast[WDOG_INST_MAX] =
{
#if WATCHDOG_INSTANCE_0
	[WATCHDOG1_INST] = { .instType = WDG_INST_TYPE_WDOG, .instIdx = 0U },
#endif
};

/** @brief Per-instance vendor configuration, built once at @ref rr_watchdog_nxp_initialize from the injected
 * 		   configuration plus the fixed design constants and re-applied by the deinit+init reconfiguration
 * 		   path. Kept in RAM so runtime timeout/mode changes can update it in place. */
static wdg_config_t wdog_nxp_cfg_ast[WDOG_INST_MAX];

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

static U32 rr_watchdog_nxp_msToTicks(U32 time_ms_u32);
static void rr_watchdog_nxp_buildConfig(const wdog_cfg_tst* const cfg_pst, wdg_config_t* const ucfg_pst);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Build the vendor configuration from the injected @ref wdog_cfg_tst and initialise the WDG PAL.
 *
 * @param wdog_inst_e Watchdog instance to initialise.
 * @param cfg_pst     Injected configuration; guaranteed non-NULL by the caller.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on any PAL failure.
 *
 * @note The vendor configuration is stored per-instance so later timeout/mode reconfiguration can update it
 * 		 in place and re-initialise. Clock source and prescaler are fixed design constants; the timeout,
 * 		 window opening, window-enable, interrupt-enable and debug/wait/stop gating all come from the
 * 		 injected configuration (see @ref rr_watchdog_nxp_buildConfig).
 */
U8 rr_watchdog_nxp_initialize(wdog_inst_te wdog_inst_e, const wdog_cfg_tst* const cfg_pst)
{
	U8 ret_u8;
	wdg_config_t* const ucfg_pst = &wdog_nxp_cfg_ast[wdog_inst_e];

	rr_watchdog_nxp_buildConfig(cfg_pst, ucfg_pst);

	if (STATUS_SUCCESS == WDG_Init(&wdog_nxp_inst_ast[wdog_inst_e], ucfg_pst))
	{
		ret_u8 = COM_HDR_RET_OK;
	}
	else
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	return ret_u8;
}

/**
 * @brief De-initialise one watchdog instance's WDG PAL, resetting the peripheral to default and disabling it.
 *
 * @param wdog_inst_e Watchdog instance to de-initialise.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the PAL call fails (e.g. the instance is locked).
 */
U8 rr_watchdog_nxp_deinit(wdog_inst_te wdog_inst_e)
{
	U8 ret_u8;

	if (STATUS_SUCCESS == WDG_Deinit(&wdog_nxp_inst_ast[wdog_inst_e]))
	{
		ret_u8 = COM_HDR_RET_OK;
	}
	else
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	return ret_u8;
}

/**
 * @brief Refresh (retrigger) one watchdog instance's countdown through the WDG PAL.
 *
 * @param wdog_inst_e Watchdog instance to refresh.
 *
 * @note @c WDG_Refresh performs the vendor unlock-then-refresh sequence atomically; no polling needed. In
 * 		 window mode the refresh only takes effect inside the open window latched at initialise.
 */
void rr_watchdog_nxp_refresh(wdog_inst_te wdog_inst_e)
{
	WDG_Refresh(&wdog_nxp_inst_ast[wdog_inst_e]);
}

/**
 * @brief Reconfigure one watchdog instance's reset timeout through the WDG PAL.
 *
 * @param wdog_inst_e    Watchdog instance to reconfigure.
 * @param timeout_ms_u32 Requested timeout in milliseconds; converted to ticks and clamped to
 * 						 [@c WATCHDOG_NXP_TICKS_MIN_U32, @c WATCHDOG_NXP_TICKS_MAX_U32] by
 * 						 @ref rr_watchdog_nxp_msToTicks.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the PAL call fails.
 *
 * @note On success the stored vendor configuration's timeout is updated too, so a later mode reconfiguration
 * 		 (which re-initialises from the stored configuration) preserves the new timeout.
 */
U8 rr_watchdog_nxp_setTimeout(wdog_inst_te wdog_inst_e, U32 timeout_ms_u32)
{
	U8 ret_u8;
	const U32 ticks_u32 = rr_watchdog_nxp_msToTicks(timeout_ms_u32);

	if (STATUS_SUCCESS == WDG_SetTimeout(&wdog_nxp_inst_ast[wdog_inst_e], ticks_u32))
	{
		wdog_nxp_cfg_ast[wdog_inst_e].timeoutValue = ticks_u32;
		ret_u8 = COM_HDR_RET_OK;
	}
	else
	{
		ret_u8 = COM_HDR_RET_ERR;
	}

	return ret_u8;
}

/**
 * @brief Enable or disable a debug/wait/stop behaviour-gating mode through the WDG PAL deinit+init path.
 *
 * @param wdog_inst_e Watchdog instance to reconfigure.
 * @param mode_e      Mode to gate; guaranteed by the caller to be one of @c WDOG_MODE_DEBUG /
 * 					  @c WDOG_MODE_WAIT / @c WDOG_MODE_STOP (never @c WDOG_MODE_NONE).
 * @param enable_u8   @c COM_HDR_TRUE to keep the watchdog active while in @p mode_e.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if @p mode_e does not map to a vendor mode or
 * 		   either PAL call fails.
 *
 * @note The WDG PAL exposes no runtime mode setter, so the requested mode is written into the stored vendor
 * 		 configuration's @c opMode and re-applied via @c WDG_Deinit followed by @c WDG_Init. The watchdog is
 * 		 briefly disabled across the deinit->init window; if the re-init fails the instance is left
 * 		 de-initialised.
 */
U8 rr_watchdog_nxp_setMode(wdog_inst_te wdog_inst_e, wdog_mode_te mode_e, U8 enable_u8)
{
	U8 ret_u8 = COM_HDR_RET_ERR;
	wdg_config_t* const ucfg_pst = &wdog_nxp_cfg_ast[wdog_inst_e];
	const bool enable_b = (COM_HDR_TRUE == enable_u8);
	BOOL mode_ok_b = COM_HDR_TRUE;

	if (WDOG_MODE_DEBUG == mode_e)
	{
		ucfg_pst->opMode.debug = enable_b;
	}
	else if (WDOG_MODE_WAIT == mode_e)
	{
		ucfg_pst->opMode.wait = enable_b;
	}
	else if (WDOG_MODE_STOP == mode_e)
	{
		ucfg_pst->opMode.stop = enable_b;
	}
	else
	{
		mode_ok_b = COM_HDR_FALSE;
	}

	if (COM_HDR_TRUE == mode_ok_b)
	{
		if (STATUS_SUCCESS != WDG_Deinit(&wdog_nxp_inst_ast[wdog_inst_e]))
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
		else if (STATUS_SUCCESS == WDG_Init(&wdog_nxp_inst_ast[wdog_inst_e], ucfg_pst))
		{
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}
	else
	{
		/* mode_e did not map to a vendor mode - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Early-warning interrupt seam for one watchdog instance (NXP realisation).
 *
 * @param wdog_inst_e Watchdog instance the early-warning interrupt fired on; guaranteed in range and
 * 					  initialised by the wrapper.
 *
 * @note Clears the vendor interrupt flag via @c WDG_ClearIntFlag before dispatch, then takes a single
 * 		 volatile snapshot of the wrapper-owned registered callback (TOCTOU guard) and calls it only if
 * 		 non-NULL. This port never touches the NVIC; enabling/masking the vector is rr_nvic's job.
 */
void rr_watchdog_nxp_irqHandler(wdog_inst_te wdog_inst_e)
{
	rr_watchdog_cb_t volatile cb_pf;

	WDG_ClearIntFlag(&wdog_nxp_inst_ast[wdog_inst_e]);

	cb_pf = wdog_cb_apf[wdog_inst_e];	/* single volatile snapshot: TOCTOU guard */

	if (COM_HDR_NULL_P != cb_pf)
	{
		cb_pf(wdog_inst_e);
	}
	else
	{
		/* no callback registered - flag already cleared */
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Convert a millisecond timeout into WDOG counter ticks and clamp to the 16-bit register range.
 *
 * @param time_ms_u32 Requested timeout in milliseconds.
 * @return Tick count clamped to [@c WATCHDOG_NXP_TICKS_MIN_U32, @c WATCHDOG_NXP_TICKS_MAX_U32].
 *
 * @note LPO clock is assumed post-256-prescale at 441.40625 Hz (no runtime calibration), matching the fixed
 * 		 @c WATCHDOG_NXP_PRESCALER_ENABLE setting applied at @ref rr_watchdog_nxp_initialize.
 * 		 Ticks = time_ms * f_Hz / 1000.
 */
static U32 rr_watchdog_nxp_msToTicks(U32 time_ms_u32)
{
	U32 ticks_u32;

	/* ticks = time_ms * (LPO_PRE_FREQ_Hz) / 1000, with LPO_PRE_FREQ_Hz carried as Hz*100000 to avoid
	   a floating-point dependency for a fixed silicon constant. */
	ticks_u32 = (U32)(((U64)time_ms_u32 * (U64)WATCHDOG_NXP_LPO_PRESCALED_HZ_X100000) /
					  ((U64)WATCHDOG_NXP_MS_PER_S * (U64)COM_HDR_SCALE_100000));

	if (WATCHDOG_NXP_TICKS_MIN_U32 > ticks_u32)
	{
		ticks_u32 = WATCHDOG_NXP_TICKS_MIN_U32;
	}
	else if (WATCHDOG_NXP_TICKS_MAX_U32 < ticks_u32)
	{
		ticks_u32 = WATCHDOG_NXP_TICKS_MAX_U32;
	}
	else
	{
		/* within range already */
	}

	return ticks_u32;
}

/**
 * @brief Populate a vendor @c wdg_config_t from the injected @ref wdog_cfg_tst and the fixed design constants.
 *
 * @param cfg_pst  Injected configuration; guaranteed non-NULL by the caller. Supplies the debug/wait/stop
 * 				   behaviour-gating selections.
 * @param ucfg_pst Output vendor configuration to populate.
 *
 * @note Clock source and prescaler are fixed design constants. The timeout, window opening,
 * 		 window-enable and early-warning interrupt-enable are taken from the injected configuration so a
 * 		 caller (or a later re-init) gets exactly the mode it asked for: the timeout is converted from ms
 * 		 to WDOG ticks; the window opening is derived as (@c window_ms / @c timeout_ms) percent and clamped;
 * 		 window mode and the interrupt are enabled only when the caller requests them. Missing timeout or an
 * 		 out-of-range derived window fall back to the rr_hw_cfg.h design defaults.
 */
static void rr_watchdog_nxp_buildConfig(const wdog_cfg_tst* const cfg_pst, wdg_config_t* const ucfg_pst)
{
	U32 window_percent_u32 = 0U;

	/* Derive the windowed-refresh opening as a percent of the timeout from the injected window/timeout
	 * pair (the vendor takes an 8-bit percent). A window opening at or beyond the whole timeout is
	 * meaningless, so fall back to the design default. Left at zero when window mode is disabled or no
	 * timeout was supplied. */
	if ((COM_HDR_TRUE == cfg_pst->window_enable_u8) && (0U != cfg_pst->timeout_ms_u32))
	{
		window_percent_u32 = ((U32)cfg_pst->window_ms_u32 * (U32)WATCHDOG_NXP_PERCENT_FULL) / cfg_pst->timeout_ms_u32;

		if ((U32)WATCHDOG_NXP_PERCENT_FULL <= window_percent_u32)
		{
			window_percent_u32 = (U32)WATCHDOG_NXP_WINDOW_PERCENT;
		}
		else
		{
			/* derived opening already inside the legal range */
		}
	}
	else
	{
		/* window disabled or no timeout supplied: no window opening */
	}

	ucfg_pst->clkSource       = WATCHDOG_NXP_CLK_SOURCE;
	ucfg_pst->opMode.debug    = (COM_HDR_TRUE == cfg_pst->debug_mode_enable_u8);
	ucfg_pst->opMode.wait     = (COM_HDR_TRUE == cfg_pst->wait_mode_enable_u8);
	ucfg_pst->opMode.stop     = (COM_HDR_TRUE == cfg_pst->stop_mode_enable_u8);
	ucfg_pst->timeoutValue    = (0U != cfg_pst->timeout_ms_u32) ?
								rr_watchdog_nxp_msToTicks(cfg_pst->timeout_ms_u32) : (U32)WATCHDOG_NXP_TIMEOUT_TICKS;
	ucfg_pst->percentWindow   = (uint8_t)window_percent_u32;
	ucfg_pst->intEnable       = (COM_HDR_TRUE == cfg_pst->int_enable_u8);
	ucfg_pst->winEnable       = (COM_HDR_TRUE == cfg_pst->window_enable_u8);
	ucfg_pst->prescalerEnable = WATCHDOG_NXP_PRESCALER_ENABLE;
}

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
