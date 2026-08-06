/**
 * @file service_iflash.c
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_crc.h"

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/services/inc/service_iflash.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
U8 service_iflash_init(void)
{
    /* Defined as static const so it resides in ROM/Flash permanently.*/
    static const iflash_cfg_tst service_iflash_cfg =
    {
        .eeprom_enable_u8 = COM_HDR_FALSE,
        .crc_compute32_pf = &rr_crc_compute32
    };

    /* Core layer initialization function calls */
    return rr_iflash_initialize(&service_iflash_cfg);
}

U8 service_iflash_deinit(void)
{
    return rr_iflash_deinit();
}

U8 service_iflash_read(iflash_kind_te kind_e, U32 off_u32, U8* const data_pu8, U32 size_u32)
{
    return rr_iflash_read(kind_e, off_u32, data_pu8, size_u32);
}

U8 service_iflash_erase(iflash_kind_te kind_e, U32 off_u32, U32 size_u32)
{
   U8 ret_u8 = COM_HDR_RET_ERR;

   ret_u8 = rr_iflash_erase(kind_e, off_u32, size_u32);

   return ret_u8;
}

U8 service_iflash_program(iflash_kind_te kind_e, U32 off_u32, const U8* const data_pu8, U32 size_u32)
{
    U8 ret_u8 = COM_HDR_RET_ERR;

    ret_u8 = rr_iflash_program(kind_e, off_u32, data_pu8, size_u32);

    return ret_u8;
}

U8 service_iflash_verify(iflash_kind_te kind_e, U32 off_u32, U32 size_u32, U32 expected_crc_u32)
{
    return rr_iflash_verifyProgram(kind_e, off_u32, size_u32, expected_crc_u32);
}

U8 service_iflash_eeProvision(U32 eeprom_bytes_u32)
{
    U8 ret_u8 = COM_HDR_RET_ERR;

    ret_u8 = rr_iflash_eeProvision(eeprom_bytes_u32);

    return ret_u8;
}
/* Private Function Definition --------------------------------------------------------------------------------------------------*/

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
