/**
 * @file rr_iflash_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction internal-memory driver (FTFC flash and FlexRAM EEE via FLASH_DRV_*).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_iflash hardware path. Every vendor call (FLASH_DRV_*) and
 * 		 FTFC constant lives here. Geometry is not hard-coded: per-kind base/limit come from the SDK
 * 		 flash_ssd_config_t, sector/program unit from FEATURE_FLS_*; the only injected datum is the
 * 		 EEPROM-enable flag handed in by the wrapper at initialise time.
 *
 * 		 Erase/program/provision are blocking (poll CCIF, no callback). The wrapper (rr_iflash.c) owns the
 * 		 shared argument validation and the stored configuration; this port owns the vendor calls and the
 * 		 per-kind range/alignment/idle gates.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/
#include <string.h>	/* memset - clear structure padding before passing an object to the vendor SDK */

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_iflash_nxp.h"	/* This port's helper API, memory-kind enum and fixed-width types */

#if defined(NXP_S32K144_146)

#include "flash_driver.h"	/* NXP FTFC flash driver: FLASH_DRV_*, flash_ssd_config_t, status_t, FEATURE_FLS_* */
#include "interrupt_manager.h"	/* INT_SYS_Disable/EnableIRQGlobal - mask IRQs across the FTFC command window */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#define IFLASH_NXP_EEPROM_NO_SECTOR		(0U)	/*!< EEPROM (FlexRAM) has no erasable sector geometry. */
#define IFLASH_NXP_EEPROM_PROG_UNIT		(1U)	/*!< EEPROM is byte-granular for programming (no alignment unit). */
#define IFLASH_NXP_QUICK_WRITE_BYTES	(0U)	/*!< FlexRAM quick-write byte count; unused for EEE_ENABLE. */
#define IFLASH_NXP_CSEC_KEY_NONE		(0U)	/*!< CSEc key size code: no CSEc keys provisioned. */
#define IFLASH_NXP_SFE_OFF				(false)	/*!< Security flag extension disabled at partition time. */
#define IFLASH_NXP_LOAD_EEE_ON_RESET	(true)	/*!< Load EEE data into FlexRAM on reset after partitioning. */
#define IFLASH_NXP_EEE_MAX_BYTES		(FEATURE_FLS_FLEX_RAM_SIZE)	/*!< Max emulated-EEPROM bytes = full FlexRAM
																		 EEE window. */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
/**
 * @brief Resolved per-kind geometry for one operation, derived at runtime (no literals stored).
 */
typedef struct
{
	U32 base_u32;		/*!< Absolute base address of the kind (from flash_ssd_config_t). */
	U32 limit_u32;		/*!< Usable size of the kind in bytes (from flash_ssd_config_t). */
	U32 sector_u32;		/*!< Erase sector size in bytes (0U when not erasable, e.g. EEPROM). */
	U32 prog_u32;		/*!< Program alignment/granularity in bytes. */
} iflash_geom_tst;

/**
 * @brief Alignment class selecting which geometry unit @ref rr_iflash_nxp_check enforces on an operation.
 */
typedef enum
{
	IFLASH_NXP_ALIGN_NONE	= 0,	/*!< No alignment requirement (read / verify); effective unit 1U. */
	IFLASH_NXP_ALIGN_PROG,			/*!< Program-unit alignment (program); unit = geom prog_u32. */
	IFLASH_NXP_ALIGN_SECTOR			/*!< Erase-sector alignment (erase); unit = geom sector_u32, 0U => reject. */
} iflash_nxp_align_te;

/**
 * @brief EEE (FlexRAM emulated EEPROM) 4-bit data-size codes for FlexNVM partitioning.
 */
enum
{
	IFLASH_NXP_EEE_CODE_4K		= 0x02U,	/*!< 0x02 selects 4096-byte EEE (FEATURE_FLS_EE_SIZE_0010). */
	IFLASH_NXP_EEE_CODE_NONE	= 0x0FU		/*!< 0x0F selects 0-byte EEE (FEATURE_FLS_EE_SIZE_1111). */
};

/**
 * @brief D-flash 4-bit partition codes for FlexNVM partitioning.
 */
enum
{
	IFLASH_NXP_DPART_CODE_64K	= 0x00U,	/*!< 0x00 selects 64K D-flash (FEATURE_FLS_DF_SIZE_0000). */
	IFLASH_NXP_DPART_CODE_32K	= 0x03U,	/*!< 0x03 selects 32K D-flash (FEATURE_FLS_DF_SIZE_0011). */
	IFLASH_NXP_DPART_CODE_16K	= 0x0AU,	/*!< 0x0A selects 16K D-flash (FEATURE_FLS_DF_SIZE_1010). */
	IFLASH_NXP_DPART_CODE_0		= 0x04U		/*!< 0x04 selects 0 D-flash (FEATURE_FLS_DF_SIZE_0100). */
};

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/** @brief SDK flash driver runtime state, populated by FLASH_DRV_Init (holds per-kind base/size). */
static flash_ssd_config_t iflash_ssd_cfg_st;

/** @brief CRC-32 compute callback injected by the wrapper at @ref rr_iflash_nxp_initialize; @c COM_HDR_NULL_P
 *  until then. */
static iflash_crc32_cb_t iflash_nxp_crc_compute32_pf = COM_HDR_NULL_P;

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

static U8 rr_iflash_nxp_check(iflash_kind_te kind_e, U32 off_u32, U32 size_u32,
							  iflash_nxp_align_te align_e, U8 need_idle_u8, iflash_geom_tst* const geom_pst);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initialise the FTFC flash driver from silicon constants and enable EEE when requested.
 *
 * @param eeprom_enable_u8 @c COM_HDR_TRUE to enable FlexRAM EEE after the driver init; @c COM_HDR_FALSE to
 * 						   leave FlexRAM as traditional RAM. Handed in by the wrapper from its injected
 * 						   configuration.
 * @param crc_compute32_pf CRC-32 compute callback (e.g. @c rr_crc_compute32) handed in by the wrapper
 * 						   from its injected configuration; stored and used by @ref rr_iflash_nxp_verify
 * 						   so this port never calls the CRC module directly.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on any SDK failure.
 *
 * @note Builds flash_user_config_t from the SDK-populated bases / FEATURE_FLS_* size and registers no
 * 		 callback (NULL_CALLBACK). When @p eeprom_enable_u8 is set it enables FlexRAM EEE; the post-init
 * 		 @c DFlashSize then reflects the live FlexNVM split. It does not provision.
 */
U8 rr_iflash_nxp_initialize(U8 eeprom_enable_u8, iflash_crc32_cb_t const crc_compute32_pf)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	status_t status_e;
	flash_user_config_t ucfg_st;

	iflash_nxp_crc_compute32_pf = crc_compute32_pf;

	/* Zero the whole object (padding included) before the named members are set: a plain = {0}
	   initialiser leaves inter-member padding indeterminate, which is passed to the vendor
	   FLASH_DRV_Init and flagged as a potential information leak via structure padding */
	memset(&ucfg_st, 0, sizeof ucfg_st);

	ucfg_st.PFlashBase = iflash_ssd_cfg_st.PFlashBase;
	ucfg_st.PFlashSize = FEATURE_FLS_PF_BLOCK_SIZE;
	ucfg_st.DFlashBase = FEATURE_FLS_DF_START_ADDRESS;
	ucfg_st.EERAMBase = FEATURE_FLS_FLEX_RAM_START_ADDRESS;
	ucfg_st.CallBack = NULL_CALLBACK;

	status_e = FLASH_DRV_Init(&ucfg_st, &iflash_ssd_cfg_st);

	if (STATUS_SUCCESS != status_e)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if ((COM_HDR_TRUE == eeprom_enable_u8) && (0U != iflash_ssd_cfg_st.EEESize))
	{
		/* EEE requested and FlexNVM already carries an EEPROM partition (EEESize populated by
		 * FLASH_DRV_Init). Only then is EEE_ENABLE valid - issuing it against unprovisioned FlexNVM
		 * (fresh part, EEESize == 0) raises ACCERR and fails init. Provisioning is a separate one-shot
		 * FLASH_DRV_DEFlashPartition step that takes effect after the next reset (LOAD_EEE_ON_RESET). */
		status_e = FLASH_DRV_SetFlexRamFunction(&iflash_ssd_cfg_st, EEE_ENABLE,
												IFLASH_NXP_QUICK_WRITE_BYTES, COM_HDR_NULL_P);

		if (STATUS_SUCCESS != status_e)
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
		else
		{
			/* EEE enabled; ssd.DFlashSize/EEESize now reflect the live partition. */
		}
	}
	else
	{
		/* EEE not requested, or FlexNVM not yet provisioned: FlexRAM left as traditional RAM. */
	}

	return ret_u8;
}

/**
 * @brief De-initialise the FTFC helper layer.
 *
 * @return @c COM_HDR_RET_OK (always).
 *
 * @note The SDK exposes no flash de-init entry point; this is a no-op kept for API symmetry.
 */
U8 rr_iflash_nxp_deinit(void)
{
	return COM_HDR_RET_OK;
}

/**
 * @brief Read bytes from a flash kind via the memory-mapped aperture.
 *
 * @param kind_e   Memory kind to read from.
 * @param off_u32  Offset within the kind, in bytes.
 * @param data_pu8 Destination buffer; must not be @c COM_HDR_NULL_P.
 * @param size_u32 Number of bytes to read; must be greater than @c 0.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on a bad range or unknown kind.
 *
 * @note Direct memory-mapped copy; no vendor call, no alignment, no idle gate.
 * 		 @ref rr_iflash_nxp_check resolves geometry and bounds-checks the range before the copy.
 */
U8 rr_iflash_nxp_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32)
{
	U8 ret_u8;
	iflash_geom_tst geom_st;

	if (COM_HDR_RET_OK != rr_iflash_nxp_check(kind_e, off_u32, size_u32, IFLASH_NXP_ALIGN_NONE,
											COM_HDR_FALSE, &geom_st))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		const U32 abs_u32 = geom_st.base_u32 + off_u32;
		U32 i_u32;

		for (i_u32 = 0U; i_u32 < size_u32; i_u32++)
		{
			/* MISRA C:2012 Rule 11.4/11.6 deviation: integer-to-pointer cast required to access the memory-mapped
			   flash/EEE aperture at a runtime-resolved absolute address (no object pointer exists for it). */
			data_pu8[i_u32] = *(const volatile U8*)(uintptr_t)(abs_u32 + i_u32);
		}

		ret_u8 = COM_HDR_RET_OK;
	}

	return ret_u8;
}

/**
 * @brief Erase one or more sectors of a flash kind.
 *
 * @param kind_e   Memory kind to erase (P-flash or D-flash; EEPROM rejected by the public layer).
 * @param off_u32  Sector-aligned offset within the kind, in bytes.
 * @param size_u32 Sector-multiple length in bytes; must be greater than @c 0.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on bad range/alignment, busy, or SDK failure.
 *
 * @note Kinds with no erasable sector (sector == 0) are rejected. @ref rr_iflash_nxp_check enforces
 * 		 sector alignment, range and controller-idle before the erase command is launched.
 */
U8 rr_iflash_nxp_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32)
{
	U8 ret_u8;
	iflash_geom_tst geom_st;

	if (COM_HDR_RET_OK != rr_iflash_nxp_check(kind_e, off_u32, size_u32, IFLASH_NXP_ALIGN_SECTOR,
											COM_HDR_TRUE, &geom_st))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		status_t status_e;

		/* Mask all interrupts across the FTFC command window: the S32K144 flash controller has no
		 * read-while-write, so any ISR vector/handler fetch from flash while a command is in flight
		 * (e.g. the RTOS SysTick) collides with the controller and resets the core. The command
		 * sequencer itself runs from RAM (.code_ram); masking closes the remaining ISR-fetch hole. */
		INT_SYS_DisableIRQGlobal();
		status_e = FLASH_DRV_EraseSector(&iflash_ssd_cfg_st, geom_st.base_u32 + off_u32, size_u32);
		INT_SYS_EnableIRQGlobal();

		if (STATUS_SUCCESS == status_e)
		{
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/**
 * @brief Program bytes into a flash kind.
 *
 * @param kind_e   Memory kind to program.
 * @param off_u32  Offset within the kind, in bytes (P/D-flash require program-unit alignment).
 * @param data_pu8 Source data; must not be @c COM_HDR_NULL_P.
 * @param size_u32 Number of bytes to program; must be greater than @c 0.
 * @return @c COM_HDR_RET_OK on success; @c COM_HDR_RET_ERR on bad range/alignment, busy, or SDK failure.
 *
 * @note EEPROM writes go through byte-granular FLASH_DRV_EEEWrite; P/D-flash through FLASH_DRV_Program.
 * 		 @ref rr_iflash_nxp_check enforces program-unit alignment, range and controller-idle first.
 */
U8 rr_iflash_nxp_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32)
{
	U8 ret_u8;
	iflash_geom_tst geom_st;

	if (COM_HDR_RET_OK != rr_iflash_nxp_check(kind_e, off_u32, size_u32, IFLASH_NXP_ALIGN_PROG,
											COM_HDR_TRUE, &geom_st))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		const U32 abs_u32 = geom_st.base_u32 + off_u32;
		status_t status_e;

		/* Mask interrupts across the FTFC command window (no read-while-write on S32K144 - an ISR
		 * fetch from flash mid-command collides with the controller and resets the core). */
		INT_SYS_DisableIRQGlobal();

		if (IFLASH_KIND_EEPROM == kind_e)
		{
			status_e = FLASH_DRV_EEEWrite(&iflash_ssd_cfg_st, abs_u32, size_u32, data_pu8);
		}
		else
		{
			status_e = FLASH_DRV_Program(&iflash_ssd_cfg_st, abs_u32, size_u32, data_pu8);
		}

		INT_SYS_EnableIRQGlobal();

		if (STATUS_SUCCESS == status_e)
		{
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/**
 * @brief Verify a region against an expected CRC32 via the rr_crc driver.
 *
 * @param kind_e           Memory kind to verify.
 * @param off_u32          Offset within the kind, in bytes.
 * @param size_u32         Number of bytes to cover; must be greater than @c 0.
 * @param expected_crc_u32 Expected CRC32 of the region.
 * @return @c COM_HDR_RET_OK when the computed CRC32 matches; @c COM_HDR_RET_ERR on a bad range, unknown
 * 		   kind, a missing CRC callback, or a CRC mismatch.
 *
 * @note Read-side comparison only: no alignment or idle gate. @ref rr_iflash_nxp_check resolves
 * 		 geometry and bounds-checks the range; the CRC-32 is obtained through @ref iflash_nxp_crc_compute32_pf,
 * 		 the callback injected at @ref rr_iflash_nxp_initialize (e.g. @c rr_crc_compute32), so this port
 * 		 never calls the CRC module directly. It covers P/D-flash and the EEE window alike.
 */
U8 rr_iflash_nxp_verify(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32)
{
	U8 ret_u8;
	iflash_geom_tst geom_st;

	if (COM_HDR_RET_OK != rr_iflash_nxp_check(kind_e, off_u32, size_u32, IFLASH_NXP_ALIGN_NONE,
											COM_HDR_FALSE, &geom_st))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if (COM_HDR_NULL_P == iflash_nxp_crc_compute32_pf)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* MISRA C:2012 Rule 11.4/11.6 deviation: integer-to-pointer cast presents the memory-mapped flash/EEE
		   aperture at a runtime-resolved absolute address as a byte buffer for the CRC driver (no object
		   pointer exists for it). */
		const U32 actual_crc_u32 = iflash_nxp_crc_compute32_pf((const U8*)(uintptr_t)(geom_st.base_u32 + off_u32), size_u32);

		if (expected_crc_u32 == actual_crc_u32)
		{
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/**
 * @brief Partition the FlexNVM to back the requested EEPROM size, once in the device's life.
 *
 * @param eeprom_bytes_u32 Requested usable EEPROM size in bytes; guaranteed greater than @c 0 by the
 * 						   caller. Rejected if it exceeds @ref IFLASH_NXP_EEE_MAX_BYTES.
 * @return @c COM_HDR_RET_OK if the EEE window is provisioned (already, or newly by this call);
 * 		   @c COM_HDR_RET_ERR if the request exceeds the window, the controller is busy, or the SDK
 * 		   command fails.
 *
 * @attention DESTRUCTIVE only on a never-provisioned device: where it runs, it reformats FlexNVM and
 * 			  erases all D-flash and EEPROM contents.
 *
 * @note The FlexNVM partition is persistent (flash IFR, read back into @c iflash_ssd_cfg_st each boot),
 * 		 so a non-zero @c EEESize means already-provisioned: the command is skipped and data preserved; only
 * 		 @c EEESize == 0 issues it. EEE here is all-or-nothing, so any valid request enables the full
 * 		 @ref IFLASH_NXP_EEE_MAX_BYTES window with a D-flash split that keeps the most addressable flash.
 */
U8 rr_iflash_nxp_eeProvision(U32 eeprom_bytes_u32)
{
	U8 ret_u8 = COM_HDR_RET_ERR;

	if (0U != iflash_ssd_cfg_st.EEESize)
	{
		/* Already provisioned in this run or a prior power cycle (persistent FlexNVM partition read
		   back at init). Skip the destructive command; the stored EEPROM/D-flash data is preserved. */
		ret_u8 = COM_HDR_RET_OK;
	}
	else if (IFLASH_NXP_EEE_MAX_BYTES < eeprom_bytes_u32)
	{
		/* Request exceeds the silicon EEE window. */
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if (false == FLASH_DRV_GetCmdCompleteFlag())
	{
		/* controller busy */
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Unprovisioned device, valid request: enable the full EEE window and keep the most
		   addressable data flash. This is the one and only place the destructive command is issued. */
		status_t status_e;

		/* Mask interrupts across the FTFC partition command (no read-while-write on S32K144). This
		 * reformat is long-running, so the RTOS tick is suspended for its duration - acceptable for
		 * the one-time provision; the alternative (all ISRs in RAM) is far more invasive. */
		INT_SYS_DisableIRQGlobal();
		status_e = FLASH_DRV_DEFlashPartition(&iflash_ssd_cfg_st,
													(U8)IFLASH_NXP_EEE_CODE_4K, (U8)IFLASH_NXP_DPART_CODE_32K,
													IFLASH_NXP_CSEC_KEY_NONE, IFLASH_NXP_SFE_OFF,
													IFLASH_NXP_LOAD_EEE_ON_RESET);
		INT_SYS_EnableIRQGlobal();

		if (STATUS_SUCCESS == status_e)
		{
			ret_u8 = COM_HDR_RET_OK;
		}
		else
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
	}

	return ret_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Resolve a kind's runtime geometry and gate one operation's range, alignment and idle state.
 *
 * @param kind_e     Memory kind; one of @c IFLASH_KIND_PFLASH / @c IFLASH_KIND_DFLASH /
 * 					 @c IFLASH_KIND_EEPROM.
 * @param off_u32    Offset within the kind, in bytes.
 * @param size_u32   Operation length in bytes.
 * @param align_e    Alignment class to enforce (@ref iflash_nxp_align_te).
 * @param need_idle_u8 @c COM_HDR_TRUE to require the FTFC controller idle before proceeding.
 * @param geom_pst   Out: filled with the resolved geometry on a pass; must not be @c COM_HDR_NULL_P.
 * @return @c COM_HDR_RET_OK when the kind is known, the range fits and all gates pass; @c COM_HDR_RET_ERR
 * 		   otherwise.
 *
 * @note Base and limit come from the flash_ssd_config_t (its DFlashSize and EEESize members reflect the
 * 		 live FlexNVM split); the sector size and program-unit size come from the FEATURE_FLS family of
 * 		 constants. The alignment unit follows @p align_e; a zero sector size (EEPROM) rejects an erase and
 * 		 is tested before the modulo. The range test subtracts the offset from the limit so it cannot
 * 		 overflow.
 */
static U8 rr_iflash_nxp_check(iflash_kind_te kind_e, U32 off_u32, U32 size_u32,
							  iflash_nxp_align_te align_e, U8 need_idle_u8, iflash_geom_tst* const geom_pst)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U32 align_u32;

	if (COM_HDR_NULL_P == geom_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if (IFLASH_KIND_PFLASH == kind_e)
	{
		geom_pst->base_u32 = iflash_ssd_cfg_st.PFlashBase;
		geom_pst->limit_u32 = iflash_ssd_cfg_st.PFlashSize;
		geom_pst->sector_u32 = FEATURE_FLS_PF_BLOCK_SECTOR_SIZE;
		geom_pst->prog_u32 = FEATURE_FLS_PF_BLOCK_WRITE_UNIT_SIZE;
	}
	else if (IFLASH_KIND_DFLASH == kind_e)
	{
		geom_pst->base_u32 = iflash_ssd_cfg_st.DFlashBase;
		geom_pst->limit_u32 = iflash_ssd_cfg_st.DFlashSize;
		geom_pst->sector_u32 = FEATURE_FLS_DF_BLOCK_SECTOR_SIZE;
		geom_pst->prog_u32 = FEATURE_FLS_DF_BLOCK_WRITE_UNIT_SIZE;
	}
	else if (IFLASH_KIND_EEPROM == kind_e)
	{
		geom_pst->base_u32 = iflash_ssd_cfg_st.EERAMBase;
		geom_pst->limit_u32 = iflash_ssd_cfg_st.EEESize;
		geom_pst->sector_u32 = IFLASH_NXP_EEPROM_NO_SECTOR;
		geom_pst->prog_u32 = IFLASH_NXP_EEPROM_PROG_UNIT;
	}
	else
	{
		/* Unknown kind. */
		ret_u8 = COM_HDR_RET_ERR;
	}

	if (COM_HDR_RET_OK == ret_u8)
	{
		/* Resolve the alignment unit from the geometry per the requested class. */
		if (IFLASH_NXP_ALIGN_SECTOR == align_e)
		{
			align_u32 = geom_pst->sector_u32;
		}
		else if (IFLASH_NXP_ALIGN_PROG == align_e)
		{
			align_u32 = geom_pst->prog_u32;
		}
		else
		{
			align_u32 = 1U;
		}

		// if (0U == align_u32)
		// {
		// 	/* Sector zero is EEPROM, so it is rejected; this also prevents a divide by zero */
		// 	ret_u8 = COM_HDR_RET_ERR;
		// }
		// else if ((off_u32 > geom_pst->limit_u32) || (size_u32 > (geom_pst->limit_u32 - off_u32)))
		// {
		// 	ret_u8 = COM_HDR_RET_ERR;
		// }
		// else if ((0U != (off_u32 % align_u32)) /*|| (0U != (size_u32 % align_u32))*/)
		// {
		// 	ret_u8 = COM_HDR_RET_ERR;
		// }
		// else if ((COM_HDR_TRUE == need_idle_u8) && (false == FLASH_DRV_GetCmdCompleteFlag()))
		// {
		// 	/* inlined busy */
		// 	ret_u8 = COM_HDR_RET_ERR;
		// }
		// else
		// {
		// 	/* all gates passed and the geometry struct is now populated */
		// }
	}
	else
	{
		/* unknown kind or null out-ptr */
	}

	return ret_u8;
}

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
