/**
 * @file service_iflash.h
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_SERVICES_INC_SERVICE_IFLASH_H_
#define MIDDLE_LAYER_SERVICES_INC_SERVICE_IFLASH_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"
/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_iflash.h"

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @brief Initializes the internal flash CDD and the underlying core driver.
 * @param cfg_pst Pointer to the driver configuration.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_init(void);

/**
 * @brief De-initializes the internal flash CDD.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_deinit(void);

/**
 * @brief Reads data from the specified internal memory kind.
 * @param kind_e Memory kind (PFLASH, DFLASH, EEPROM).
 * @param off_u32 Byte offset within the memory.
 * @param data_pu8 Destination buffer.
 * @param size_u32 Number of bytes to read.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32);

/**
 * @brief Erases a region of the specified internal memory kind.
 * @note This is a blocking operation. Global IRQs are disabled during execution.
 * @param kind_e Memory kind (PFLASH or DFLASH).
 * @param off_u32 Byte offset (must be sector-aligned).
 * @param size_u32 Number of bytes to erase.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32);

/**
 * @brief Programs bytes into the specified internal memory kind.
 * @note This is a blocking operation. Global IRQs are disabled during execution.
 * @param kind_e Memory kind (PFLASH, DFLASH, EEPROM).
 * @param off_u32 Byte offset.
 * @param data_pu8 Source buffer.
 * @param size_u32 Number of bytes to program.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32);

/**
 * @brief Verifies a programmed region against an expected CRC32.
 * @param kind_e Memory kind (PFLASH, DFLASH, EEPROM).
 * @param off_u32 Byte offset.
 * @param size_u32 Number of bytes to cover.
 * @param expected_crc_u32 Expected CRC32 value.
 * @return COM_HDR_RET_OK on match, COM_HDR_RET_ERR on mismatch or failure.
 */
U8 service_iflash_verify(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32);

/**
 * @brief Provisions the internal EEPROM.
 * @note Destructive, one-time operation. Global IRQs are disabled during execution.
 * @param eeprom_bytes_u32 Requested EEPROM size in bytes.
 * @return COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_iflash_eeProvision(U32 eeprom_bytes_u32);
#endif /* MIDDLE_LAYER_SERVICES_INC_SERVICE_IFLASH_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
