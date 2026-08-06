/**
 * @file rr_watchdog_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction internal watchdog driver (WDG PAL over WDOG).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_watchdog hardware path. Included
 * 		 only by the rr_watchdog wrapper's NXP arm and by rr_watchdog_nxp.c. The whole body folds to empty on
 * 		 non-NXP targets, so it may be included unconditionally. The public watchdog API, its argument
 * 		 validation and the stored configuration/callback slots live in rr_watchdog.h / rr_watchdog.c; this
 * 		 header exposes only the WDG-PAL-backed helpers the wrapper dispatches to, plus the extern view of the
 * 		 wrapper-owned early-warning callback slots that the port's interrupt seam reads across the seam to
 * 		 dispatch each instance's callback.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_WATCHDOG_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_WATCHDOG_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_watchdog.h"	/* Wrapper's public types: wdog_inst_te, wdog_mode_te,
													   wdog_cfg_tst, rr_watchdog_cb_t */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/
/** @brief Wrapper-owned registered early-warning callback per instance (defined in rr_watchdog.c);
 *  	   @c COM_HDR_NULL_P when no middleware callback is armed. Read by the port's interrupt seam. */
extern rr_watchdog_cb_t volatile wdog_cb_apf[WDOG_INST_MAX];

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note rr_watchdog_nxp_initialize / rr_watchdog_nxp_deinit / rr_watchdog_nxp_setTimeout /
 * 		 rr_watchdog_nxp_setMode return @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on a vendor-level
 * 		 failure. rr_watchdog_nxp_refresh and rr_watchdog_nxp_irqHandler have no return (best-effort,
 * 		 time-critical). Argument validation is owned by the wrapper (rr_watchdog.c). Each function is
 * 		 documented above its definition in rr_watchdog_nxp.c.
 */
U8 rr_watchdog_nxp_initialize(wdog_inst_te wdog_inst_e, const wdog_cfg_tst* const cfg_pst);
U8 rr_watchdog_nxp_deinit(wdog_inst_te wdog_inst_e);

void rr_watchdog_nxp_refresh(wdog_inst_te wdog_inst_e);
U8 rr_watchdog_nxp_setTimeout(wdog_inst_te wdog_inst_e, U32 timeout_ms_u32);
U8 rr_watchdog_nxp_setMode(wdog_inst_te wdog_inst_e, wdog_mode_te mode_e, U8 enable_u8);

void rr_watchdog_nxp_irqHandler(wdog_inst_te wdog_inst_e);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_WATCHDOG_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
