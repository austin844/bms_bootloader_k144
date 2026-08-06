/**
 * @file rr_iflash_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction internal-memory driver (FTFC flash and FlexRAM EEE via FLASH_DRV_*).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_iflash hardware path. Included
 * 		 only by the rr_iflash wrapper's NXP arm and by rr_iflash_nxp.c. The whole body folds to empty on
 * 		 non-NXP targets, so it may be included unconditionally. The public internal-memory API, its
 * 		 argument validation and the stored configuration live in rr_iflash.h / rr_iflash.c; this header
 * 		 exposes only the vendor-backed helpers the wrapper dispatches to.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_IFLASH_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_IFLASH_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */
#include "core_layer/drivers/inc/rr_iflash.h"	/* Memory-kind enum (iflash_kind_te) shared with the wrapper */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every helper below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR on any failure (bad
 * 		 range, misalignment, controller busy, or a vendor error). The wrapper (rr_iflash.c) owns the shared
 * 		 argument validation and the stored configuration; these helpers own the vendor calls and the
 * 		 per-kind geometry gates. Each function is documented above its definition in rr_iflash_nxp.c.
 */
U8 rr_iflash_nxp_initialize(U8 eeprom_enable_u8, iflash_crc32_cb_t const crc_compute32_pf);
U8 rr_iflash_nxp_deinit(void);

U8 rr_iflash_nxp_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32);
U8 rr_iflash_nxp_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32);
U8 rr_iflash_nxp_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32);
U8 rr_iflash_nxp_verify(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32);

U8 rr_iflash_nxp_eeProvision(U32 eeprom_bytes_u32);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_IFLASH_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
