/**
 * @file rr_watchdog.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction internal watchdog driver (WDOG timeout, refresh, mode gating).
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note One platform-abstracted API serves every internal on-chip watchdog instance. The generic layer
 * 		 validates arguments and delegates; every vendor call (the WDG PAL) lives in the per-chip port (the
 * 		 NXP path is rr_watchdog_nxp under bsp/nxp). The configuration layer injects the full timeout/window/
 * 		 mode/interrupt setup at @ref rr_watchdog_initialize, the driver owns no board data of its own.
 *
 * 		 The early-warning interrupt is optional and dispatched through a single volatile callback slot
 * 		 registered at @ref rr_watchdog_initialize and cleared at @ref rr_watchdog_deinit. This driver
 * 		 never calls INT_SYS_/HAL_NVIC_; enabling/masking the vector is rr_nvic's job.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_watchdog.h"	/* This module's public API, instance/mode enums and injected
													   config type */

#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_watchdog_nxp.h"	/* NXP port: rr_watchdog_nxp_* WDOG helpers (vendor WDG PAL calls confined to
								   bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#else

/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 watchdog driver macros */

#elif defined(NXP_S32K144_146)

/* @note NXP timeout-conversion and fixed WDG PAL design macros live in the bsp/nxp port (rr_watchdog_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas watchdog driver macros */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 watchdog driver types */

#elif defined(NXP_S32K144_146)

/* @note NXP vendor-config types are used only inside the bsp/nxp port (rr_watchdog_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas watchdog driver types */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 watchdog driver state */

#elif defined(NXP_S32K144_146)

/* @note NXP instance index -> vendor instance number lookup lives in the bsp/nxp port (rr_watchdog_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas watchdog driver state */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/
/** @brief Stored configuration pointer per instance; @c COM_HDR_NULL_P until @ref rr_watchdog_initialize
 * 		   succeeds. Used by the wrapper to gate refresh/reconfigure calls on an initialised instance. */
const wdog_cfg_tst* wdog_cfg_pst[WDOG_INST_MAX] = { COM_HDR_NULL_P };

/** @brief Registered early-warning callback per instance; cleared at @ref rr_watchdog_deinit. External
 * 		   linkage so the bsp/nxp port's interrupt seam can read it across the seam (extern in
 * 		   rr_watchdog_nxp.h). */
rr_watchdog_cb_t volatile wdog_cb_apf[WDOG_INST_MAX] = { COM_HDR_NULL_P };

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 watchdog private helpers */

#elif defined(NXP_S32K144_146)

/* @note NXP versions of each function (the public functions call these) are declared in rr_watchdog_nxp.h (bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas watchdog private helpers */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initialise one watchdog instance and store the caller's configuration.
 *
 * @param wdog_inst_e Watchdog instance to initialise; must be less than @c WDOG_INST_MAX.
 * @param cfg_pst     Pointer to the driver configuration; must not be @c COM_HDR_NULL_P.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if @p wdog_inst_e is out of range,
 * 		   @p cfg_pst is @c COM_HDR_NULL_P, or the platform initialisation fails.
 *
 * @note On a platform-init failure the stored pointer and callback are cleared so the instance stays
 * 		 idle. The optional early-warning callback (@c cfg_pst->cb_pf) is registered here and dispatched
 * 		 later from @ref rr_watchdog_irqHandler.
 */
U8 rr_watchdog_initialize(wdog_inst_te wdog_inst_e, const wdog_cfg_tst* const cfg_pst)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((WDOG_INST_MAX > wdog_inst_e) && (COM_HDR_NULL_P != cfg_pst))
	{
		wdog_cfg_pst[wdog_inst_e] = cfg_pst;
		wdog_cb_apf[wdog_inst_e] = cfg_pst->cb_pf;

#if defined(STM32)

		/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_watchdog_nxp_initialize(wdog_inst_e, cfg_pst);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

		if (COM_HDR_RET_ERR == ret_u8)
		{
			wdog_cfg_pst[wdog_inst_e] = COM_HDR_NULL_P;
			wdog_cb_apf[wdog_inst_e] = COM_HDR_NULL_P;
		}
		else
		{
			/* ret_u8 already set by platform delegate */
		}
	}
	else
	{
		/* wdog_inst_e out of range or cfg_pst invalid - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief De-initialise one watchdog instance and clear its stored configuration and callback.
 *
 * @param wdog_inst_e Watchdog instance to de-initialise; must be less than @c WDOG_INST_MAX.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if @p wdog_inst_e is out of range or the
 * 		   platform de-initialisation fails.
 *
 * @note The configuration pointer and callback are cleared unconditionally, even when the platform
 * 		 de-init returns @c COM_HDR_RET_ERR.
 */
U8 rr_watchdog_deinit(wdog_inst_te wdog_inst_e)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (WDOG_INST_MAX > wdog_inst_e)
	{

#if defined(STM32)

		/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_watchdog_nxp_deinit(wdog_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

		wdog_cfg_pst[wdog_inst_e] = COM_HDR_NULL_P;
		wdog_cb_apf[wdog_inst_e] = COM_HDR_NULL_P;
	}
	else
	{
		/* wdog_inst_e out of range - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Refresh (retrigger) one watchdog instance's countdown.
 *
 * @param wdog_inst_e Watchdog instance to refresh; must be less than @c WDOG_INST_MAX.
 *
 * @note Time-critical, best-effort: out-of-range instances and instances that were never initialised
 * 		 are silently ignored, matching the original design intent that a refresh call is never allowed
 * 		 to fault the caller.
 */
void rr_watchdog_refresh(wdog_inst_te wdog_inst_e)
{
	if ((WDOG_INST_MAX > wdog_inst_e) && (COM_HDR_NULL_P != wdog_cfg_pst[wdog_inst_e]))
	{

#if defined(STM32)

		/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

		rr_watchdog_nxp_refresh(wdog_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* wdog_inst_e out of range or uninitialised - no-op */
	}
}

/**
 * @brief Reconfigure one watchdog instance's reset timeout.
 *
 * @param wdog_inst_e   Watchdog instance to reconfigure; must be less than @c WDOG_INST_MAX.
 * @param timeout_ms_u32 Requested timeout in milliseconds; converted to ticks and clamped to
 * 						 [@c WATCHDOG_NXP_TICKS_MIN_U32, @c WATCHDOG_NXP_TICKS_MAX_U32] ticks.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if @p wdog_inst_e is out of range, the
 * 		   instance is uninitialised, or the vendor call fails.
 */
U8 rr_watchdog_setTimeout(wdog_inst_te wdog_inst_e, U32 timeout_ms_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((WDOG_INST_MAX > wdog_inst_e) && (COM_HDR_NULL_P != wdog_cfg_pst[wdog_inst_e]))
	{

#if defined(STM32)

		/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_watchdog_nxp_setTimeout(wdog_inst_e, timeout_ms_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* wdog_inst_e out of range or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Enable or disable a debug/wait/stop behaviour-gating mode on one watchdog instance.
 *
 * @param wdog_inst_e Watchdog instance to reconfigure; must be less than @c WDOG_INST_MAX.
 * @param mode_e      Mode to gate; one of @c WDOG_MODE_DEBUG / @c WDOG_MODE_WAIT / @c WDOG_MODE_STOP.
 * 					  @c WDOG_MODE_NONE is a no-op guard that returns @c COM_HDR_RET_OK without touching
 * 					  the hardware.
 * @param enable_u8   @c COM_HDR_TRUE to keep the watchdog active while in @p mode_e; @c COM_HDR_FALSE
 * 					  to allow it to pause.
 * @return @c COM_HDR_RET_OK on success (including the @c WDOG_MODE_NONE no-op); @c COM_HDR_RET_ERR if
 * 		   @p wdog_inst_e is out of range, the instance is uninitialised, @p mode_e is out of range, or
 * 		   the vendor call fails.
 */
U8 rr_watchdog_setMode(wdog_inst_te wdog_inst_e, wdog_mode_te mode_e, U8 enable_u8)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((WDOG_INST_MAX > wdog_inst_e) && (COM_HDR_NULL_P != wdog_cfg_pst[wdog_inst_e]) &&
		(WDOG_MODE_MAX > mode_e))
	{
		if (WDOG_MODE_NONE == mode_e)
		{
			/* No-op guard: nothing to change. */
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{

#if defined(STM32)

			/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

			ret_u8 = rr_watchdog_nxp_setMode(wdog_inst_e, mode_e, enable_u8);

#elif defined(RENESAS)

			/* @note Reserved for Renesas build support */

#else

			/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

		}
	}
	else
	{
		/* wdog_inst_e/mode_e out of range or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Early-warning interrupt seam for one watchdog instance.
 *
 * @param wdog_inst_e Watchdog instance the early-warning interrupt fired on; out-of-range or
 * 					  uninitialised values are ignored.
 *
 * @note Validates the instance then delegates to the port, which clears the vendor interrupt flag and
 * 		 dispatches the registered early-warning callback. This driver never touches the NVIC; the consuming
 * 		 project's vector calls this handler directly (or via rr_nvic once that module owns the vector table
 * 		 entry).
 */
void rr_watchdog_irqHandler(wdog_inst_te wdog_inst_e)
{
	if ((WDOG_INST_MAX > wdog_inst_e) && (COM_HDR_NULL_P != wdog_cfg_pst[wdog_inst_e]))
	{

#if defined(STM32)

		/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

		rr_watchdog_nxp_irqHandler(wdog_inst_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas build support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* wdog_inst_e out of range or uninitialised - ignored */
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 implementation */

#elif defined(NXP_S32K144_146)

/* @note NXP WDOG helper implementations live in the bsp/nxp port (rr_watchdog_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas implementation */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
