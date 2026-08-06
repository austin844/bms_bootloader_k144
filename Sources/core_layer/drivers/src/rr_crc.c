/**
 * @file rr_crc.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction CRC driver (hardware CRC-32 generator with software fallback; software CRC-8 PEC).
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Computes a standard CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320, init 0xFFFFFFFF, final XOR
 * 		 0xFFFFFFFF). The NXP arm drives the on-chip CRC generator through the vendor peripheral driver
 * 		 (CRC_DRV_*), configured for the same parameters (GPOLY 0x04C11DB7 with input/output bit-reflection
 * 		 and output complement), so the hardware and software paths return the same value. rr_crc_compute32()
 * 		 uses the hardware engine once rr_crc_initialize() has enabled it, otherwise the software CRC-32. The
 * 		 only direct register access is the PCC clock gate (the SDK ships no clock config); all CRC datapath
 * 		 access goes through CRC_DRV_* and stays inside the \#if defined(NXP_S32K144_146) region.
 *
 * @note rr_crc_compute8() computes the CRC-8 SMBus/PMBus Packet Error Code (poly 0x07, MSB-first, init
 * 		 0x00, no reflect, no final XOR) via a 256-entry lookup table. It has no hardware backing on any
 * 		 supported arm, so it is implemented once, outside the platform \#if/\#elif ladder, and shared by
 * 		 every arm.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_crc.h"	/* This module's public API */

#if defined(STM32)

/* @note Reserved for STM32 CRC peripheral headers */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_crc_nxp.h"	/* NXP port: rr_crc_nxp_* hardware CRC-32 helpers (CRC_DRV_* confined to bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA CRC (r_crc) headers */

#else

/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
/** @brief CRC-32 reflected generator polynomial (software path), applied LSB-first per byte. */
#define RR_CRC_SW_POLY				(0xEDB88320U)
/** @brief CRC-32 initial register value. */
#define RR_CRC_SW_INIT				(0xFFFFFFFFU)
/** @brief CRC-32 final output XOR mask (software path). */
#define RR_CRC_SW_XOROUT			(0xFFFFFFFFU)
/** @brief Bits processed per input byte by the software inner loop. */
#define RR_CRC_SW_BITS_PER_BYTE		(8U)

/** @brief CRC-8 SMBus/PMBus PEC initial register value. */
#define RR_CRC8_INIT				(0x00U)
/** @brief Number of entries in the CRC-8 lookup table (one per possible byte value). */
#define RR_CRC8_TABLE_SIZE			(256U)

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/** @brief CRC-8 SMBus/PMBus PEC lookup table (poly 0x07, MSB-first, indexed by
 *  	   crc ^ next_byte); common to every arm since this CRC has no hardware backing. */
static const U8 rr_crc8_table_au8[RR_CRC8_TABLE_SIZE] =
{
	0x00U, 0x07U, 0x0EU, 0x09U, 0x1CU, 0x1BU, 0x12U, 0x15U,
	0x38U, 0x3FU, 0x36U, 0x31U, 0x24U, 0x23U, 0x2AU, 0x2DU,
	0x70U, 0x77U, 0x7EU, 0x79U, 0x6CU, 0x6BU, 0x62U, 0x65U,
	0x48U, 0x4FU, 0x46U, 0x41U, 0x54U, 0x53U, 0x5AU, 0x5DU,
	0xE0U, 0xE7U, 0xEEU, 0xE9U, 0xFCU, 0xFBU, 0xF2U, 0xF5U,
	0xD8U, 0xDFU, 0xD6U, 0xD1U, 0xC4U, 0xC3U, 0xCAU, 0xCDU,
	0x90U, 0x97U, 0x9EU, 0x99U, 0x8CU, 0x8BU, 0x82U, 0x85U,
	0xA8U, 0xAFU, 0xA6U, 0xA1U, 0xB4U, 0xB3U, 0xBAU, 0xBDU,
	0xC7U, 0xC0U, 0xC9U, 0xCEU, 0xDBU, 0xDCU, 0xD5U, 0xD2U,
	0xFFU, 0xF8U, 0xF1U, 0xF6U, 0xE3U, 0xE4U, 0xEDU, 0xEAU,
	0xB7U, 0xB0U, 0xB9U, 0xBEU, 0xABU, 0xACU, 0xA5U, 0xA2U,
	0x8FU, 0x88U, 0x81U, 0x86U, 0x93U, 0x94U, 0x9DU, 0x9AU,
	0x27U, 0x20U, 0x29U, 0x2EU, 0x3BU, 0x3CU, 0x35U, 0x32U,
	0x1FU, 0x18U, 0x11U, 0x16U, 0x03U, 0x04U, 0x0DU, 0x0AU,
	0x57U, 0x50U, 0x59U, 0x5EU, 0x4BU, 0x4CU, 0x45U, 0x42U,
	0x6FU, 0x68U, 0x61U, 0x66U, 0x73U, 0x74U, 0x7DU, 0x7AU,
	0x89U, 0x8EU, 0x87U, 0x80U, 0x95U, 0x92U, 0x9BU, 0x9CU,
	0xB1U, 0xB6U, 0xBFU, 0xB8U, 0xADU, 0xAAU, 0xA3U, 0xA4U,
	0xF9U, 0xFEU, 0xF7U, 0xF0U, 0xE5U, 0xE2U, 0xEBU, 0xECU,
	0xC1U, 0xC6U, 0xCFU, 0xC8U, 0xDDU, 0xDAU, 0xD3U, 0xD4U,
	0x69U, 0x6EU, 0x67U, 0x60U, 0x75U, 0x72U, 0x7BU, 0x7CU,
	0x51U, 0x56U, 0x5FU, 0x58U, 0x4DU, 0x4AU, 0x43U, 0x44U,
	0x19U, 0x1EU, 0x17U, 0x10U, 0x05U, 0x02U, 0x0BU, 0x0CU,
	0x21U, 0x26U, 0x2FU, 0x28U, 0x3DU, 0x3AU, 0x33U, 0x34U,
	0x4EU, 0x49U, 0x40U, 0x47U, 0x52U, 0x55U, 0x5CU, 0x5BU,
	0x76U, 0x71U, 0x78U, 0x7FU, 0x6AU, 0x6DU, 0x64U, 0x63U,
	0x3EU, 0x39U, 0x30U, 0x37U, 0x22U, 0x25U, 0x2CU, 0x2BU,
	0x06U, 0x01U, 0x08U, 0x0FU, 0x1AU, 0x1DU, 0x14U, 0x13U,
	0xAEU, 0xA9U, 0xA0U, 0xA7U, 0xB2U, 0xB5U, 0xBCU, 0xBBU,
	0x96U, 0x91U, 0x98U, 0x9FU, 0x8AU, 0x8DU, 0x84U, 0x83U,
	0xDEU, 0xD9U, 0xD0U, 0xD7U, 0xC2U, 0xC5U, 0xCCU, 0xCBU,
	0xE6U, 0xE1U, 0xE8U, 0xEFU, 0xFAU, 0xFDU, 0xF4U, 0xF3U,
};

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

static U32 rr_crc_sw_compute32(const U8* const data_pu8, U32 size_u32);
static U8  rr_crc_sw_compute8(const U8* const data_pu8, U16 len_u16);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initialise the CRC driver, enabling the hardware generator where the platform has one.
 *
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on platform-init failure.
 *
 * @note Not mandatory: rr_crc_compute32 works in software without it. On platforms with no CRC peripheral
 * 		 this still returns @c COM_HDR_RET_OK and the software path is used.
 */
U8 rr_crc_initialize(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;

#if defined(STM32)

	/* @note Reserved for STM32 CRC peripheral support */

#elif defined(NXP_S32K144_146)

	ret_u8 = rr_crc_nxp_initialize();

#elif defined(RENESAS)

	/* @note Reserved for Renesas RA CRC (r_crc) support */

#else

	/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	return ret_u8;
}

/**
 * @brief De-initialise the CRC driver and disable the hardware generator.
 *
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on platform de-init failure.
 */
U8 rr_crc_deinit(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;

#if defined(STM32)

	/* @note Reserved for STM32 CRC peripheral support */

#elif defined(NXP_S32K144_146)

	ret_u8 = rr_crc_nxp_deinit();

#elif defined(RENESAS)

	/* @note Reserved for Renesas RA CRC (r_crc) support */

#else

	/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	return ret_u8;
}

/**
 * @brief Compute a standard CRC-32 (IEEE 802.3) over a byte range.
 *
 * @param data_pu8 Source bytes; must not be @c COM_HDR_NULL_P.
 * @param size_u32 Byte count; must be greater than @c 0.
 * @return CRC-32 of the range, or @c 0U for a @c COM_HDR_NULL_P buffer or zero length.
 *
 * @note Uses the hardware generator once rr_crc_initialize has enabled it, otherwise the software CRC-32;
 * 		 both yield the same value.
 */
U32 rr_crc_compute32(const U8* const data_pu8, U32 size_u32)
{
	U32 crc_u32 = 0U;

	if ((COM_HDR_NULL_P != data_pu8) && (0U < size_u32))
	{

#if defined(NXP_S32K144_146)

		if (COM_HDR_TRUE == rr_crc_nxp_hw_ready())
		{
			crc_u32 = rr_crc_nxp_compute32(data_pu8, size_u32);
		}
		else
		{
			crc_u32 = rr_crc_sw_compute32(data_pu8, size_u32);
		}

#else

		/* No hardware CRC arm for this platform; the software CRC-32 is always available. */
		crc_u32 = rr_crc_sw_compute32(data_pu8, size_u32);

#endif /* NXP_S32K144_146 */

	}
	else
	{
		/* invalid arguments - crc_u32 stays 0U */
	}

	return crc_u32;
}

/**
 * @brief Compute the CRC-8 SMBus/PMBus Packet Error Code (PEC) over a byte range.
 *
 * @param data_pu8 Source bytes; must not be @c COM_HDR_NULL_P.
 * @param len_u16 Byte count; must be greater than @c 0.
 * @return The PEC byte of the range, or @c 0U for a @c COM_HDR_NULL_P buffer or zero length.
 *
 * @note Software-only on every arm (poly 0x07, MSB-first, init 0x00, no reflect, no final XOR); matches
 * 		 the BMS reference CRC8_Compute_crc_8bit() byte-for-byte.
 */
U8 rr_crc_compute8(const U8* const data_pu8, U16 len_u16)
{
	U8 crc_u8 = 0U;

	if ((COM_HDR_NULL_P != data_pu8) && (0U < len_u16))
	{
		crc_u8 = rr_crc_sw_compute8(data_pu8, len_u16);
	}
	else
	{
		/* invalid arguments - crc_u8 stays 0U */
	}

	return crc_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Compute a standard CRC-32 (IEEE 802.3) over a byte range in software.
 *
 * @param data_pu8 Source bytes; assumed non-NULL by the caller.
 * @param size_u32 Byte count; assumed greater than @c 0 by the caller.
 * @return The reflected CRC-32 of the range.
 *
 * @note Reflected algorithm (init @ref RR_CRC_SW_INIT, poly @ref RR_CRC_SW_POLY LSB-first, final XOR
 * 		 @ref RR_CRC_SW_XOROUT) - the house-standard CRC-32, matching the NXP hardware result.
 */
static U32 rr_crc_sw_compute32(const U8* const data_pu8, U32 size_u32)
{
	U32 crc_u32 = RR_CRC_SW_INIT;
	U32 i_u32;
	U8 bit_u8;

	for (i_u32 = 0U; i_u32 < size_u32; i_u32++)
	{
		crc_u32 ^= (U32)data_pu8[i_u32];

		for (bit_u8 = 0U; bit_u8 < RR_CRC_SW_BITS_PER_BYTE; bit_u8++)
		{
			if (0U != (crc_u32 & 1U))
			{
				crc_u32 = (crc_u32 >> 1U) ^ RR_CRC_SW_POLY;
			}
			else
			{
				crc_u32 = (crc_u32 >> 1U);
			}
		}
	}

	return (crc_u32 ^ RR_CRC_SW_XOROUT);
}

/**
 * @brief Compute the CRC-8 SMBus/PMBus PEC over a byte range using the lookup table.
 *
 * @param data_pu8 Source bytes; assumed non-NULL by the caller.
 * @param len_u16 Byte count; assumed greater than @c 0 by the caller.
 * @return The PEC byte of the range.
 *
 * @note Non-reflected algorithm (init @ref RR_CRC8_INIT, poly 0x07 MSB-first, no final XOR),
 * 		 table-driven equivalent of the BMS reference CRC8_Compute_crc_8bit() bitwise loop.
 */
static U8 rr_crc_sw_compute8(const U8* const data_pu8, U16 len_u16)
{
	U8 crc_u8 = (U8)RR_CRC8_INIT;
	U16 i_u16;

	for (i_u16 = 0U; i_u16 < len_u16; i_u16++)
	{
		crc_u8 = rr_crc8_table_au8[crc_u8 ^ data_pu8[i_u16]];
	}

	return crc_u8;
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
