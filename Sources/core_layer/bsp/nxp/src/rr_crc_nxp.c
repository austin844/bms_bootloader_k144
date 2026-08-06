/**
 * @file rr_crc_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction CRC driver (on-chip CRC-32 generator via CRC_DRV_*).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_crc hardware path. Configures the NXP CRC generator for a
 * 		 standard CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320) so it reproduces the wrapper's software
 * 		 CRC-32 bit-for-bit. The only direct register access is the PCC clock gate (the SDK ships no clock
 * 		 config); all CRC datapath access goes through CRC_DRV_*. The wrapper (rr_crc.c) owns argument
 * 		 validation, the hardware-vs-software branch and the software fallback; this port owns the vendor calls.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_crc_nxp.h"	/* This port's helper API and fixed-width types */

#if defined(NXP_S32K144_146)

#include "crc_driver.h"	/* NXP CRC peripheral driver: CRC_DRV_*, crc_user_config_t, status_t; pulls
				   device_registers.h for the PCC clock gate (PCC->, PCC_PCCn_*, PCC_CRC_INDEX) */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
/** @brief CRC peripheral instance index (single CRC module on this family). */
#define RR_CRC_NXP_INSTANCE			(0U)
/** @brief CRC-32 generator polynomial in non-reflected form for the GPOLY register. */
#define RR_CRC_NXP_GPOLY			(0x04C11DB7U)
/** @brief Initial CRC seed (the driver loads it under CTRL.WAS). */
#define RR_CRC_NXP_SEED				(0xFFFFFFFFU)

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/** @brief @c COM_HDR_TRUE once the hardware generator is enabled; selects HW vs software path. */
static U8 rr_crc_hw_ready_u8 = COM_HDR_FALSE;

/** @brief Standard CRC-32 (IEEE 802.3) parameters for the NXP CRC generator: 32-bit width, non-reflected
 *  	   GPOLY 0x04C11DB7, input bit-reflection (write transpose = bits within byte), output bit+byte
 *  	   reflection (read transpose = bits and bytes), output complement and a 0xFFFFFFFF seed. These
 *  	   reproduce the wrapper's rr_crc_sw_compute32 bit-for-bit. */
static const crc_user_config_t rr_crc_nxp_cfg_st =
{
	.crcWidth			= CRC_BITS_32,
	.polynomial			= RR_CRC_NXP_GPOLY,
	.readTranspose		= CRC_TRANSPOSE_BITS_AND_BYTES,
	.writeTranspose		= CRC_TRANSPOSE_BITS,
	.complementChecksum	= true,
	.seed				= RR_CRC_NXP_SEED,
};

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Enable the CRC peripheral clock, configure the generator and select the hardware path.
 *
 * @return @c COM_HDR_RET_OK once the generator is configured; @c COM_HDR_RET_ERR if CRC_DRV_Init fails.
 *
 * @note Enables the PCC clock gate (the SDK ships no clock config) then hands the standard CRC-32
 * 		 configuration to CRC_DRV_Init. On failure the clock gate is released so the software path is used.
 */
U8 rr_crc_nxp_initialize(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	/* Enable the CRC peripheral clock gate - the only direct register access in this driver. */
	PCC->PCCn[PCC_CRC_INDEX] |= PCC_PCCn_CGC_MASK;

	if (STATUS_SUCCESS == CRC_DRV_Init(RR_CRC_NXP_INSTANCE, &rr_crc_nxp_cfg_st))
	{
		rr_crc_hw_ready_u8 = COM_HDR_TRUE;
	}
	else
	{
		PCC->PCCn[PCC_CRC_INDEX] &= ~PCC_PCCn_CGC_MASK;
		ret_u8 = COM_HDR_RET_ERR;
	}

	return ret_u8;
}

/**
 * @brief De-initialise the CRC generator, disable its clock and revert to the software path.
 *
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if CRC_DRV_Deinit fails.
 */
U8 rr_crc_nxp_deinit(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	rr_crc_hw_ready_u8 = COM_HDR_FALSE;

	if (STATUS_SUCCESS != CRC_DRV_Deinit(RR_CRC_NXP_INSTANCE))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* generator reset to its default configuration */
	}

	/* Release the CRC peripheral clock gate - the only direct register access in this driver. */
	PCC->PCCn[PCC_CRC_INDEX] &= ~PCC_PCCn_CGC_MASK;

	return ret_u8;
}

/**
 * @brief Report whether the hardware CRC generator is enabled and ready.
 *
 * @return @c COM_HDR_TRUE once rr_crc_nxp_initialize has enabled the generator; @c COM_HDR_FALSE otherwise.
 *
 * @note The wrapper uses this to choose the hardware path over its software CRC-32 fallback.
 */
U8 rr_crc_nxp_hw_ready(void)
{
	return rr_crc_hw_ready_u8;
}

/**
 * @brief Compute a standard CRC-32 (IEEE 802.3) over a byte range using the hardware generator.
 *
 * @param data_pu8 Source bytes; assumed non-NULL by the caller.
 * @param size_u32 Byte count; assumed greater than @c 0 by the caller.
 * @return The CRC-32 of the range, identical to the wrapper's rr_crc_sw_compute32.
 *
 * @note CRC_DRV_Configure reloads the configuration and seed (so each call is a fresh calculation, with
 * 		 CTRL.WAS cleared before the data phase), CRC_DRV_WriteData feeds the range one byte at a time, and
 * 		 CRC_DRV_GetCrcResult returns the reflected, complemented result - no software post-processing.
 */
U32 rr_crc_nxp_compute32(const U8* const data_pu8, U32 size_u32)
{
	(void)CRC_DRV_Configure(RR_CRC_NXP_INSTANCE, &rr_crc_nxp_cfg_st);
	CRC_DRV_WriteData(RR_CRC_NXP_INSTANCE, data_pu8, size_u32);

	return CRC_DRV_GetCrcResult(RR_CRC_NXP_INSTANCE);
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
