/**
 * @file rr_timer_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction Timer driver (FTM output-compare via FTM_DRV_*).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_timer hardware path. Included
 * 		 only by the rr_timer wrapper's NXP arm and by rr_timer_nxp.c. The whole body folds to empty on
 * 		 non-NXP targets, so it may be included unconditionally. The public Timer API, its argument
 * 		 validation and the injected config/callback mirror tables live in rr_timer.h / rr_timer.c; this
 * 		 header exposes only the vendor-backed helpers the wrapper dispatches to, plus the extern view of
 * 		 the wrapper-owned config/callback mirror tables that the port reads across the seam.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_TIMER_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_TIMER_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U16/U32) and COM_HDR_* constants */
#include "core_layer/drivers/inc/rr_timer.h"	/* Wrapper's public types: timer_inst_te, timer_chan_cfg_tst,
											 	   timer_ftm_ext_tst, rr_timer_cb_t */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/
/** @brief Wrapper-owned channel config table pointers (defined in rr_timer.c), one per instance; NULL until
 *  	   @c rr_timer_init_u8 succeeds. Read by the port's start/stop helpers to walk the configured channels. */
extern const timer_chan_cfg_tst* timer_cfg_pst[TIMER_INST_MAX];

/** @brief Wrapper-owned channel counts (defined in rr_timer.c), one per instance. Read by the port's
 *  	   start/stop helpers to bound the configured-channel walk. */
extern U8 timer_chan_cnt_au8[TIMER_INST_MAX];

/** @brief Wrapper-owned per-channel callback mirror (defined in rr_timer.c). Read by the port's ISR seam as a
 *  	   single volatile snapshot per expiry (TOCTOU guard). */
extern volatile rr_timer_cb_t timer_cb_aapf[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT];

/** @brief Wrapper-owned per-channel operating-mode mirror (defined in rr_timer.c). Read by the port's ISR
 *  	   seam to tell continuous from one-shot channels by index alone. */
extern timer_chan_mode_te timer_chan_type_aae[TIMER_INST_MAX][TIMER_CHANNEL_MAX_COUNT];

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note rr_timer_nxp_initialize / rr_timer_nxp_deinit / rr_timer_nxp_arm_channel return @c COM_HDR_RET_OK on
 * 		 success or @c COM_HDR_RET_ERR on a vendor-level failure; rr_timer_nxp_cancel_channel always returns
 * 		 @c COM_HDR_RET_OK (the vendor disable call reports no failure status). rr_timer_nxp_start_base_v /
 * 		 rr_timer_nxp_stop_base_v (void) enable/disable the configured channels' compare interrupts;
 * 		 rr_timer_nxp_irqHandler (void) is the channel-expiry ISR seam. Argument validation is owned by the
 * 		 wrapper (rr_timer.c). Each function is documented above its definition in rr_timer_nxp.c.
 */
U8 rr_timer_nxp_initialize(timer_inst_te timer_inst_e, const timer_chan_cfg_tst* const chan_cfg_pst, U8 chan_cnt_u8,
							const timer_ftm_ext_tst* const ext_pst);
U8 rr_timer_nxp_deinit(timer_inst_te timer_inst_e);

void rr_timer_nxp_start_base_v(timer_inst_te timer_inst_e);
void rr_timer_nxp_stop_base_v(timer_inst_te timer_inst_e);

U8 rr_timer_nxp_arm_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8);
U8 rr_timer_nxp_cancel_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8);

void rr_timer_nxp_irqHandler(timer_inst_te timer_inst_e);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_TIMER_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
