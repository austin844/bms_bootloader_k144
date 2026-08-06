/**
 * @file rr_crc.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction CRC driver interface (hardware CRC-32 with software fallback; software CRC-8 PEC).
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note One platform-abstracted API computes a standard CRC-32 (IEEE 802.3) over a byte range. On the NXP
 * 		 target rr_crc_initialize() enables the on-chip CRC generator and rr_crc_compute32() uses it; if the
 * 		 engine is unavailable (driver not initialised, or a platform with no CRC peripheral) the same call
 * 		 uses a software CRC-32 that returns an identical value. The software path is always present, so
 * 		 rr_crc_compute32() works without a prior rr_crc_initialize(). rr_crc_compute8() is a separate,
 * 		 always-software CRC-8 (SMBus PEC: poly 0x07, MSB-first, init 0x00, no reflect, no final XOR) used to
 * 		 protect SMBus/PMBus transactions; it has no hardware path on any arm.
 */

#ifndef CORE_LAYER_INC_RR_CRC_H_
#define CORE_LAYER_INC_RR_CRC_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note rr_crc_initialize / rr_crc_deinit return @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on
 * 		 failure. rr_crc_compute32 returns the 32-bit CRC of the range, or @c 0U for a @c COM_HDR_NULL_P
 * 		 buffer or a zero length. rr_crc_compute8 returns the 8-bit SMBus PEC of the range, or @c 0U for a
 * 		 @c COM_HDR_NULL_P buffer or a zero length. Each function is documented above its definition in
 * 		 rr_crc.c.
 */
U8 rr_crc_initialize(void);
U8 rr_crc_deinit(void);

U32 rr_crc_compute32(const U8* const data_pu8, U32 size_u32);
U8  rr_crc_compute8(const U8* const data_pu8, U16 len_u16);

#endif /* CORE_LAYER_INC_RR_CRC_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
