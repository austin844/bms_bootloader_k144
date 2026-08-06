/**
 * @file rr_memory.c
 * @author vishalagarwal_rideri
 * @brief Platform-independent byte-memory utility driver (copy, set, compare).
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Stateless byte-wise memory helpers used across the SDK in place of the standard C
 * 		 library. There is no HAL/SDK dependency and no configuration; every public API runs a
 * 		 plain C loop over caller-supplied buffers, identically on every platform. No vendor SDK
 * 		 header is required and no peripheral/register access is involved.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_memory.h"	/* This module's public API */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

static void rr_memory_copy_loop_u8(U8* const dest_pu8, const U8* const src_pu8, U32 len_u32);
static void rr_memory_set_loop_u8(U8* const dest_pu8, U8 val_u8, U32 len_u32);
static U8 rr_memory_compare_loop_u8(const U8* const buf1_pu8, const U8* const buf2_pu8, U32 len_u32);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Copy a byte range from a source buffer to a destination buffer.
 *
 * @param dest_pu8 Destination buffer to copy into. Caller-owned; a @c COM_HDR_NULL_P pointer is
 * 		  rejected and the copy skipped.
 * @param src_pu8  Source buffer to copy from. Caller-owned; a @c COM_HDR_NULL_P pointer is rejected
 * 		  and the copy skipped.
 * @param len_u32  Number of bytes to copy.
 *
 * @note NULL buffers are guarded at this public entry so an untrusted pointer is never dereferenced
 * 		 (CWE-822). No overlap detection (memcpy semantics: @p dest_pu8 and @p src_pu8 must not
 * 		 overlap). The caller is responsible for valid ranges.
 */
void rr_memory_copy_u8(U8* const dest_pu8, const U8* const src_pu8, U32 len_u32)
{
	if((COM_HDR_NULL_P == dest_pu8) || (COM_HDR_NULL_P == src_pu8))
	{
		/* untrusted/NULL buffer: do not dereference (CWE-822 taint guard) */
	}
	else
	{
		rr_memory_copy_loop_u8(dest_pu8, src_pu8, len_u32);
	}
}

/**
 * @brief Fill a byte range of a destination buffer with a constant value.
 *
 * @param dest_pu8 Destination buffer to fill. Caller-owned; a @c COM_HDR_NULL_P pointer is rejected
 * 		  and the fill skipped.
 * @param val_u8   Byte value written to every position in the range.
 * @param len_u32  Number of bytes to fill.
 *
 * @note The NULL buffer is guarded at this public entry so an untrusted pointer is never
 * 		 dereferenced (CWE-822). The caller is responsible for a valid range.
 */
void rr_memory_set_u8(U8* const dest_pu8, U8 val_u8, U32 len_u32)
{
	if(COM_HDR_NULL_P == dest_pu8)
	{
		/* untrusted/NULL buffer: do not dereference (CWE-822 taint guard) */
	}
	else
	{
		rr_memory_set_loop_u8(dest_pu8, val_u8, len_u32);
	}
}

/**
 * @brief Compare two byte buffers over a given length.
 *
 * @param buf1_pu8 First buffer to compare. Caller-owned; a @c COM_HDR_NULL_P pointer is rejected.
 * @param buf2_pu8 Second buffer to compare. Caller-owned; a @c COM_HDR_NULL_P pointer is rejected.
 * @param len_u32  Number of bytes to compare.
 * @return @c COM_HDR_RET_OK if the two buffers match over @p len_u32 bytes; @c COM_HDR_RET_ERR on the
 * 		   first mismatching byte or if either buffer is @c COM_HDR_NULL_P.
 *
 * @note Early-exits on the first difference. NULL buffers are guarded at this public entry so an
 * 		 untrusted pointer is never dereferenced (CWE-822).
 */
U8 rr_memory_compare_u8(const U8* const buf1_pu8, const U8* const buf2_pu8, U32 len_u32)
{
	U8 ret_u8;

	if((COM_HDR_NULL_P == buf1_pu8) || (COM_HDR_NULL_P == buf2_pu8))
	{
		ret_u8 = COM_HDR_RET_ERR;	/* untrusted/NULL buffer: do not dereference (CWE-822 taint guard) */
	}
	else
	{
		ret_u8 = rr_memory_compare_loop_u8(buf1_pu8, buf2_pu8, len_u32);
	}

	return ret_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/

/**
 * @brief Byte-wise copy loop from a source buffer to a destination buffer.
 *
 * @param dest_pu8 Destination buffer to copy into.
 * @param src_pu8  Source buffer to copy from.
 * @param len_u32  Number of bytes to copy.
 *
 * @note Plain sequential loop; no vendor call, no DMA. Overkill cases (large/high-volume
 * 		 transfers) belong to a dedicated EDMA-based driver, not here.
 */
static void rr_memory_copy_loop_u8(U8* const dest_pu8, const U8* const src_pu8, U32 len_u32)
{
	U32 i_u32;

	for (i_u32 = 0U; i_u32 < len_u32; i_u32++)
	{
		dest_pu8[i_u32] = src_pu8[i_u32];
	}
}

/**
 * @brief Byte-wise fill loop over a destination buffer.
 *
 * @param dest_pu8 Destination buffer to fill.
 * @param val_u8   Byte value written to every position in the range.
 * @param len_u32  Number of bytes to fill.
 *
 * @note Plain sequential loop; no vendor call.
 */
static void rr_memory_set_loop_u8(U8* const dest_pu8, U8 val_u8, U32 len_u32)
{
	U32 i_u32;

	for (i_u32 = 0U; i_u32 < len_u32; i_u32++)
	{
		dest_pu8[i_u32] = val_u8;
	}
}

/**
 * @brief Byte-wise comparison loop between two buffers, exiting early on a mismatch.
 *
 * @param buf1_pu8 First buffer to compare.
 * @param buf2_pu8 Second buffer to compare.
 * @param len_u32  Number of bytes to compare.
 * @return @c COM_HDR_RET_OK if every byte matches over @p len_u32 bytes; @c COM_HDR_RET_ERR on the
 * 		   first mismatching byte.
 *
 * @note Plain sequential loop; no vendor call. Stops comparing as soon as a difference is found.
 */
static U8 rr_memory_compare_loop_u8(const U8* const buf1_pu8, const U8* const buf2_pu8, U32 len_u32)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U32 i_u32;

	for (i_u32 = 0U; i_u32 < len_u32; i_u32++)
	{
		if (buf1_pu8[i_u32] != buf2_pu8[i_u32])
		{
			ret_u8 = COM_HDR_RET_ERR;
			break;
		}
		else
		{
			/* bytes match at this index; continue scanning */
		}
	}

	return ret_u8;
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
