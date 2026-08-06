/**
 * @file rr_can_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction CAN driver (FlexCAN via the NXP CAN PAL).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_can hardware path. Included only
 * 		 by the rr_can wrapper's NXP arm and by rr_can_nxp.c. The whole body folds to empty on non-NXP
 * 		 targets, so it may be included unconditionally. The public CAN API, its argument validation, the
 * 		 registered middleware callbacks and the bus-off recovery policy live in rr_can.h / rr_can.c; this
 * 		 header exposes only the vendor-backed helpers the wrapper dispatches to, plus the extern view of the
 * 		 wrapper-owned per-channel filter count and the wrapper's generic dispatch helpers that the port's
 * 		 SDK-registered callbacks reach across the seam.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_CAN_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_CAN_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_can.h"	/* Wrapper's public types (can_inst_te, can_msg_tst, can_tx_mode_te,
 * 		 can_filter_cfg_tst) and the CAN_INSTANCE_* enables */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/
/** @brief Wrapper-owned per-channel receive-filter count (defined in rr_can.c), remembered from
 *  	   rr_can_initialize(). Read by the port to size its filter set-up, start and stop mailbox loops. */
extern U8 can_filter_cnt_au8[CAN_INST_MAX];

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every helper below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on failure (channel
 * 		 turned off, mailbox split invalid or vendor-driver error), except rr_can_nxp_error_decode which
 * 		 returns the decoded @ref can_error_flag_te bit-mask. Arguments arrive already validated by the
 * 		 rr_can wrapper. Each function is documented above its definition in rr_can_nxp.c.
 */
U8 rr_can_nxp_initialize(can_inst_te can_inst_e, U8 can_filters_nos_u8);
U8 rr_can_nxp_configMailbox(can_inst_te can_inst_e, const can_filter_cfg_tst* const cfg_arr_pst);
U8 rr_can_nxp_deinit(can_inst_te can_inst_e);

U8 rr_can_nxp_start(can_inst_te can_inst_e);
U8 rr_can_nxp_stop(can_inst_te can_inst_e);

U8 rr_can_nxp_transmit(can_inst_te can_inst_e, const can_msg_tst* const can_tx_msg_st, can_tx_mode_te tx_mode_e,
						U32 timeout_ms_u32);
U8 rr_can_nxp_receive(can_inst_te can_inst_e, U32 rx_mailbox_u32, can_msg_tst* const rx_msg_pst);

U8 rr_can_nxp_abortTx(can_inst_te can_inst_e);
U8 rr_can_nxp_error_decode(U32 error_status_u32);

/**
 * @note Wrapper-owned generic dispatch helpers (defined in rr_can.c): external linkage only so the port's
 * 		 SDK-registered callbacks (rr_can_nxp.c) can hand received frames and raw bus-error status up to the
 * 		 registered middleware callbacks. rr_can_receive returns @c COM_HDR_RET_OK / @c COM_HDR_RET_ERR;
 * 		 rr_can_error_callback also applies the wrapper's bus-off recovery policy.
 */
U8   rr_can_receive(can_inst_te can_inst_e, U32 rx_mailbox_u32);
void rr_can_error_callback(can_inst_te can_inst_e, U32 error_status_u32);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_CAN_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
