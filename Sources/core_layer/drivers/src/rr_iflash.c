/**
 * @file rr_iflash.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction internal-memory driver (program flash, data flash, EEPROM emulation).
 * @date 19-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note One platform-abstracted API serves program flash, data flash and emulated EEPROM, each
 * 		 operation selected by a memory kind (@ref iflash_kind_te) and an offset within it. The generic
 * 		 layer validates arguments and delegates; every vendor call (FLASH_DRV_*) and FTFC constant stays
 * 		 inside the \#if defined(NXP_S32K144_146) region. Geometry is not hard-coded: per-kind base/limit
 * 		 come from the SDK flash_ssd_config_t, sector/program unit from FEATURE_FLS_*; the only injected
 * 		 datum is the EEPROM-enable flag.
 *
 * 		 Erase/program/provision are blocking (poll CCIF, no callback). Global-IRQ guarding and FlexNVM
 * 		 provisioning are the middleware's job. Implemented for the NXP target only.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_iflash.h"	/* This module's public API, memory-kind enum and injected config type */

#if defined(STM32)

/* @note Reserved for STM32 HAL flash driver headers */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_iflash_nxp.h"	/* NXP port: rr_iflash_nxp_* FTFC flash/EEE helpers (FLASH_DRV_* confined to bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA flash (r_flash_hp / EEL) headers */

#else

/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/** @brief Stored configuration pointer; @c COM_HDR_NULL_P until @ref rr_iflash_initialize succeeds. */
static const iflash_cfg_tst* iflash_cfg_pst = COM_HDR_NULL_P;

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

static U8 rr_iflash_args_ok(iflash_kind_te kind_e, U32 size_u32, const void* const buf_pcv, U8 need_buf_u8);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initialise the internal-memory driver and store the caller's configuration.
 *
 * @param cfg_pst Pointer to the driver configuration; must not be @c COM_HDR_NULL_P. @c eeprom_enable_u8
 * 		  is @c COM_HDR_TRUE to enable FlexRAM EEE at init or @c COM_HDR_FALSE to leave it disabled.
 * 		  @c crc_compute32_pf is the CRC-32 compute callback (e.g. @c rr_crc_compute32) forwarded to the
 * 		  platform port for @ref rr_iflash_verifyProgram; the driver never calls the CRC module directly.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if @p cfg_pst is @c COM_HDR_NULL_P or the
 * 		   platform initialisation fails.
 *
 * @note On a platform-init failure the stored pointer is cleared so the driver stays idle. This call
 * 		 does not provision FlexNVM; the D-flash limit it later reports reflects whatever split a
 * 		 prior @ref rr_iflash_eeProvision left in place.
 */
U8 rr_iflash_initialize(const iflash_cfg_tst* const cfg_pst)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (COM_HDR_NULL_P != cfg_pst)
	{
		iflash_cfg_pst = cfg_pst;

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_initialize(iflash_cfg_pst->eeprom_enable_u8, iflash_cfg_pst->crc_compute32_pf);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

		if (COM_HDR_RET_ERR == ret_u8)
		{
			iflash_cfg_pst = COM_HDR_NULL_P;
		}
		else
		{
			/* ret_u8 already set by platform delegate */
		}
	}
	else
	{
		/* cfg_pst invalid - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief De-initialise the internal-memory driver and clear the stored configuration.
 *
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the platform de-initialisation fails.
 *
 * @note The configuration pointer is cleared unconditionally, even when the platform de-init
 * 		 returns @c COM_HDR_RET_ERR.
 */
U8 rr_iflash_deinit(void)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

#if defined(STM32)

	/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

	ret_u8 = rr_iflash_nxp_deinit();

#elif defined(RENESAS)

	/* @note Reserved for Renesas RA data-flash/EEL support */

#else

	/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	iflash_cfg_pst = COM_HDR_NULL_P;

	return ret_u8;
}

/**
 * @brief Read bytes from the selected internal-memory kind.
 *
 * @param kind_e   Memory kind to read; one of @c IFLASH_KIND_PFLASH / @c IFLASH_KIND_DFLASH /
 * 				   @c IFLASH_KIND_EEPROM (must be less than @c IFLASH_KIND_MAX).
 * @param off_u32  Byte offset within the selected memory.
 * @param data_pu8 Destination buffer; must not be @c COM_HDR_NULL_P.
 * @param size_u32 Number of bytes to read; must be greater than @c 0.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the driver is uninitialised, @p kind_e is
 * 		   out of range, @p data_pu8 is @c COM_HDR_NULL_P, @p size_u32 is zero, the range is invalid,
 * 		   or the read fails.
 */
U8 rr_iflash_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (COM_HDR_RET_OK == rr_iflash_args_ok(kind_e, size_u32, (const void*)data_pu8, COM_HDR_TRUE))
	{

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_read(kind_e, off_u32, data_pu8, size_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* invalid arguments or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Erase a region of the selected internal-memory kind.
 *
 * @param kind_e   Memory kind to erase; must be @c IFLASH_KIND_PFLASH or @c IFLASH_KIND_DFLASH.
 * 				   @c IFLASH_KIND_EEPROM is rejected (EEE has no client erase).
 * @param off_u32  Byte offset within the selected memory; must be sector-aligned.
 * @param size_u32 Number of bytes to erase; must be greater than @c 0 and a sector multiple.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the driver is uninitialised, @p kind_e is
 * 		   @c IFLASH_KIND_EEPROM or out of range, @p size_u32 is zero, or the erase fails.
 */
U8 rr_iflash_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((COM_HDR_RET_OK == rr_iflash_args_ok(kind_e, size_u32, COM_HDR_NULL_P, COM_HDR_FALSE)) &&
		(IFLASH_KIND_EEPROM != kind_e))
	{

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_erase(kind_e, off_u32, size_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* invalid arguments, EEPROM, or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Program bytes into the selected internal-memory kind.
 *
 * @param kind_e   Memory kind to program; one of @c IFLASH_KIND_PFLASH / @c IFLASH_KIND_DFLASH /
 * 				   @c IFLASH_KIND_EEPROM (must be less than @c IFLASH_KIND_MAX).
 * @param off_u32  Byte offset within the selected memory; P/D-flash require program-unit alignment.
 * @param data_pu8 Source buffer; must not be @c COM_HDR_NULL_P.
 * @param size_u32 Number of bytes to program; must be greater than @c 0.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR if the driver is uninitialised, @p kind_e is
 * 		   out of range, @p data_pu8 is @c COM_HDR_NULL_P, @p size_u32 is zero, or the program fails.
 *
 * @note The @c IFLASH_KIND_EEPROM kind is byte-granular and is routed to the EEE write path.
 */
U8 rr_iflash_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (COM_HDR_RET_OK == rr_iflash_args_ok(kind_e, size_u32, (const void*)data_pu8, COM_HDR_TRUE))
	{

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_program(kind_e, off_u32, data_pu8, size_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* invalid arguments or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Verify a programmed region against a caller-supplied CRC32.
 *
 * @param kind_e           Memory kind to verify; one of @c IFLASH_KIND_PFLASH / @c IFLASH_KIND_DFLASH /
 * 						   @c IFLASH_KIND_EEPROM (must be less than @c IFLASH_KIND_MAX).
 * @param off_u32          Byte offset within the selected memory.
 * @param size_u32         Number of bytes to cover; must be greater than @c 0.
 * @param expected_crc_u32 Expected CRC32 (IEEE 802.3) of the region @c [base+off, base+off+size).
 * @return @c COM_HDR_RET_OK if the computed CRC32 equals @p expected_crc_u32; @c COM_HDR_RET_ERR if the
 * 		   driver is uninitialised, @p kind_e is out of range, @p size_u32 is zero, the range is
 * 		   invalid, or the CRC mismatches.
 *
 * @note The CRC32 is computed by the rr_crc driver (hardware where available, software otherwise) and
 * 		 covers P-flash, D-flash and the EEE window alike.
 */
U8 rr_iflash_verifyProgram(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (COM_HDR_RET_OK == rr_iflash_args_ok(kind_e, size_u32, COM_HDR_NULL_P, COM_HDR_FALSE))
	{

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_verify(kind_e, off_u32, size_u32, expected_crc_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* invalid arguments or uninitialised - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/**
 * @brief Provision the internal EEPROM to at least the requested size, once in the device's life.
 *
 * @param eeprom_bytes_u32 Requested usable EEPROM size in bytes; must be greater than @c 0 and within
 * 						   the silicon's capacity. A request of @c 0 is rejected.
 * @return @c COM_HDR_RET_OK if the EEPROM is provisioned (either newly, or already from a prior call /
 * 		   power cycle); @c COM_HDR_RET_ERR if the driver is uninitialised, @p eeprom_bytes_u32 is @c 0
 * 		   or exceeds the silicon's capacity, or the underlying provision command fails.
 *
 * @attention DESTRUCTIVE but one-time and self-guarding: the reformat (erasing all data-flash and
 * 		 EEPROM) runs only on a never-provisioned device. Later calls, including after any reset, return
 * 		 @c COM_HDR_RET_OK without touching stored data, so it is safe on the boot path.
 *
 * @note The caller names only the size it needs; how it is backed and how "already provisioned" is
 * 		 detected are platform-specific. Implemented only for the NXP target; other platforms reject it.
 */
U8 rr_iflash_eeProvision(U32 eeprom_bytes_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((COM_HDR_NULL_P != iflash_cfg_pst) && (0U < eeprom_bytes_u32))
	{

#if defined(STM32)

		/* @note Reserved for STM32 EEPROM/flash support (rare HW, software-emulated) */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_iflash_nxp_eeProvision(eeprom_bytes_u32);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA data-flash/EEL support */

#else

		/* @note Reserved: no platform selected */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

	}
	else
	{
		/* driver not initialised or zero-size request - ret_u8 stays COM_HDR_RET_ERR */
	}

	return ret_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Validate the argument preconditions shared by the public read/erase/program/verify entry points.
 *
 * @param kind_e     Memory kind requested by the caller; must be less than @c IFLASH_KIND_MAX.
 * @param size_u32   Operation length in bytes; must be greater than @c 0.
 * @param buf_pcv    Caller buffer (source or destination); checked only when @p need_buf_u8 is set.
 * @param need_buf_u8 @c COM_HDR_TRUE to require @p buf_pcv non-NULL; @c COM_HDR_FALSE to skip the buffer
 * 					 check (operations that take no buffer, e.g. erase / verify).
 * @return @c COM_HDR_RET_OK when every checked precondition holds; @c COM_HDR_RET_ERR otherwise.
 *
 * @note Shared precondition check for the read/erase/program/verify entry points: driver initialised
 * 		 (@c iflash_cfg_pst non-NULL), @p kind_e in range, @p size_u32 non-zero. @p buf_pcv is checked
 * 		 only when @p need_buf_u8 is @c COM_HDR_TRUE.
 */
static U8 rr_iflash_args_ok(iflash_kind_te kind_e, U32 size_u32, const void* const buf_pcv, U8 need_buf_u8)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if ((COM_HDR_NULL_P != iflash_cfg_pst) && (IFLASH_KIND_MAX > kind_e) && (0U < size_u32) &&
		((COM_HDR_FALSE == need_buf_u8) || (COM_HDR_NULL_P != buf_pcv)))
	{
		ret_u8 = COM_HDR_RET_OK;
	}
	else
	{
		/* one or more public-layer preconditions failed */
	}

	return ret_u8;
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
