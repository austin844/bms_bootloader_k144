/**
 * @file rr_crc_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction CRC driver (on-chip CRC-32 generator via CRC_DRV_*).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_crc hardware path. Included only
 * 		 by the rr_crc wrapper's NXP arm and by rr_crc_nxp.c. The whole body folds to empty on non-NXP
 * 		 targets, so it may be included unconditionally. The public CRC API and its software fallback live in
 * 		 rr_crc.h / rr_crc.c; this header exposes only the vendor-backed helpers the wrapper dispatches to.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_CRC_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_CRC_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note rr_crc_nxp_initialize / rr_crc_nxp_deinit return @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on
 * 		 failure. rr_crc_nxp_hw_ready returns @c COM_HDR_TRUE once the hardware generator is enabled (it selects
 * 		 the hardware-vs-software path in the wrapper), else @c COM_HDR_FALSE. rr_crc_nxp_compute32 returns the
 * 		 32-bit CRC of the range using the hardware generator. Each function is documented above its definition
 * 		 in rr_crc_nxp.c.
 */
U8 rr_crc_nxp_initialize(void);
U8 rr_crc_nxp_deinit(void);
U8 rr_crc_nxp_hw_ready(void);

U32 rr_crc_nxp_compute32(const U8* const data_pu8, U32 size_u32);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_CRC_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
