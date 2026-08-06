/**
 * @file rr_iflash.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction internal-memory driver interface (program flash, data flash, EEPROM).
 * @date 18-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note One platform-abstracted API serves every internal non-volatile memory: program flash,
 * 		 data flash and emulated EEPROM. An operation is selected by a memory @ref iflash_kind_te
 * 		 plus an offset within that memory. The driver owns all silicon geometry (bases, sector size,
 * 		 program unit, total size); the configuration layer injects only an EEPROM-enable flag at
 * 		 rr_iflash_initialize(). There is no banking and no geometry table here. The logical section
 * 		 map (bootloader/application/calibration/...) and any bounds or read-only enforcement belong
 * 		 to the configuration layer, not this driver.
 *
 * 		 The driver is blocking: erase, program and provision launch the vendor command and poll the
 * 		 controller to completion before returning, so no callback is registered. read and
 * 		 verifyProgram are memory-mapped only; verifyProgram computes a CRC32 over the region and
 * 		 compares it to a caller-supplied value. Provisioning (FlexNVM/EEPROM partitioning) is
 * 		 platform specific and implemented only for the NXP target.
 *
 */

#ifndef CORE_LAYER_INC_RR_IFLASH_H_
#define CORE_LAYER_INC_RR_IFLASH_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/
/**
 * @brief Internal-memory class selector. Names the physical memory an operation targets; the driver
 * 		  resolves that memory's base, erase sector and program unit internally from the silicon.
 */
typedef enum iflash_kind_te_tag
{
	IFLASH_KIND_PFLASH = 0,	/**< Program flash: code/constant store; sector-erase then phrase-program */
	IFLASH_KIND_DFLASH,		/**< Data flash (FlexNVM): sector-erase then phrase-program */
	IFLASH_KIND_EEPROM,		/**< Emulated EEPROM (FlexRAM/EEE): byte-granular rewrite, no client erase */
	IFLASH_KIND_MAX			/**< Sentinel: number of memory kinds */
} iflash_kind_te;

/**
 * @brief CRC-32 compute callback injected at @ref rr_iflash_initialize, matching @c rr_crc_compute32's
 * 		  signature. Lets @ref rr_iflash_verifyProgram obtain a CRC-32 without the iflash driver
 * 		  including or calling the CRC driver directly, removing the upward/lateral dependency.
 */
typedef U32 (*iflash_crc32_cb_t)(const U8* data_pu8, U32 size_u32);

/**
 * @brief Board configuration injected at @ref rr_iflash_initialize. The EEPROM-enable flag is the
 * 		  only board-specific datum the driver needs; all silicon geometry (bases, sector size,
 * 		  program unit, total size) is sourced internally from the SDK config struct and device
 * 		  feature macros after initialisation.
 */
typedef struct iflash_cfg_tst_tag
{
	U8 eeprom_enable_u8;			/**< COM_HDR_TRUE => enable FlexRAM EEE at init and reflect the FlexNVM split into
										 the D-flash limit */
	iflash_crc32_cb_t crc_compute32_pf;	/**< CRC-32 compute callback used by verifyProgram; must not be COM_HDR_NULL_P */
} iflash_cfg_tst;

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every API below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on any failure
 * 		 (uninitialized, bad argument, out-of-bounds offset, misalignment, controller busy, or a
 * 		 vendor error). The command operations (@ref rr_iflash_erase / @ref rr_iflash_program /
 * 		 @ref rr_iflash_eeProvision) are blocking: they launch the flash command and poll it to
 * 		 completion before returning. @ref rr_iflash_verifyProgram returns @c COM_HDR_RET_OK only when
 * 		 the region CRC32 matches @p expected_crc_u32. Each function is documented above its
 * 		 definition in rr_iflash.c.
 */
U8 rr_iflash_initialize(const iflash_cfg_tst* const cfg_pst);
U8 rr_iflash_deinit(void);

U8 rr_iflash_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32);
U8 rr_iflash_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32);
U8 rr_iflash_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32);
U8 rr_iflash_verifyProgram(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32);

U8 rr_iflash_eeProvision(U32 eeprom_bytes_u32);

#endif /* CORE_LAYER_INC_RR_IFLASH_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
