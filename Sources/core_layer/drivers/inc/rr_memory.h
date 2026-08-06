/**
 * @file rr_memory.h
 * @author vishalagarwal_rideri
 * @brief Platform-independent byte-memory utility driver interface (copy, set, compare).
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Stateless byte-wise memory helpers used across the SDK in place of the standard C
 * 		 library. There is no HAL/SDK dependency, no configuration and no initialisation: every
 * 		 API is a pure, immediately-usable C loop over caller-supplied buffers. The caller is
 * 		 responsible for pointer validity and, for @ref rr_memory_copy_u8, for the two ranges not
 * 		 overlapping (memcpy semantics, not memmove).
 *
 */

#ifndef CORE_LAYER_DRIVERS_INC_RR_MEMORY_H_
#define CORE_LAYER_DRIVERS_INC_RR_MEMORY_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) */

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note @ref rr_memory_copy_u8 and @ref rr_memory_set_u8 return @c void; they perform no argument
 * 		 validation and always run to completion. @ref rr_memory_compare_u8 returns @c COM_HDR_RET_OK
 * 		 when the two buffers match over @p len_u32 bytes, or @c COM_HDR_RET_ERR on the first
 * 		 mismatching byte (early exit). Each function is documented above its definition in rr_memory.c.
 */
void rr_memory_copy_u8(U8* const dest_pu8, const U8* const src_pu8, U32 len_u32);
void rr_memory_set_u8(U8* const dest_pu8, U8 val_u8, U32 len_u32);

U8 rr_memory_compare_u8(const U8* const buf1_pu8, const U8* const buf2_pu8, U32 len_u32);

#endif /* CORE_LAYER_INC_RR_MEMORY_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
