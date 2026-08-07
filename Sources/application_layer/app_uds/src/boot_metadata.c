/**
 * @file boot_metadata.c
 * @author divyansh
 * @brief  Single-Bank Boot Metadata Management for S32K144
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Mobility Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/
#include <string.h>
#include "device_registers.h"
#include "interrupt_manager.h"
/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/services/inc/service_iflash.h"
#include "middle_layer/services/inc/service_wdog.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/
#include "application_layer/app_uds/inc/boot_metadata.h"

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------
 * CRC-32 (IEEE 802.3, polynomial 0xEDB88320, little-endian bit order)
 * --------------------------------------------------------------------------- */

U32 crc32_running(U32 crc, const U8 *data, U32 len)
{
    for (U32 i = 0U; i < len; i++) {
        crc ^= (U32)data[i];
        for (U8 bit = 0U; bit < 8U; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

U32 crc32_compute(const U8 *data, U32 len)
{
    return ~crc32_running(0xFFFFFFFFUL, data, len);
}

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

static U32 metadata_struct_crc(const boot_metadata_t *m)
{
    /* Cover every field except crc_self, which is the last U32 in the struct.
     * sizeof(boot_metadata_t) - sizeof(U32) equals offsetof(…, crc_self)
     * because the struct is naturally aligned and crc_self is the last member. */
    return crc32_compute((const U8 *)m,
                         (U32)(sizeof(boot_metadata_t) - sizeof(U32)));
}

/* ---------------------------------------------------------------------------
 * Public boot_metadata API
 * --------------------------------------------------------------------------- */

BOOL boot_metadata_read(boot_metadata_t *out)
{
    /* Metadata lives in Code Flash (ETX_METADATA_CF_ADDR). */
    if (service_iflash_read(IFLASH_KIND_PFLASH,
                        ETX_METADATA_CF_ADDR,
                        (U8 *)out,
                        (U32)sizeof(boot_metadata_t)) != COM_HDR_RET_OK)
    {
        return false;
    }
    if (out->magic != BOOT_METADATA_MAGIC)
    {
        return false;
    }
    return (out->crc_self == metadata_struct_crc(out));
}

BOOL boot_metadata_write(const boot_metadata_t *in)
{
    boot_metadata_t tmp;
    memcpy(&tmp, in, sizeof(boot_metadata_t));
    tmp.crc_self = metadata_struct_crc(&tmp);

    /* Erase the code-flash sector that holds the struct, then write it. */
    if (service_iflash_erase(IFLASH_KIND_PFLASH,
                             ETX_METADATA_CF_ADDR,
                             ETX_METADATA_CF_SIZE) != COM_HDR_RET_OK)
    {
        return false;
    }

    return (service_iflash_program(IFLASH_KIND_PFLASH,
                                   ETX_METADATA_CF_ADDR,
                                   (const U8 *)&tmp,
                                   (U32)sizeof(boot_metadata_t)) == COM_HDR_RET_OK);
}

void boot_metadata_init(void)
{
    boot_metadata_t m;
    if (!boot_metadata_read(&m))
    {
        memset(&m, 0, sizeof(m));
        m.magic          = BOOT_METADATA_MAGIC;
        m.boot_reason    = ETX_NORMAL_BOOT;
        m.stay_in_bootloader = 0;

        /*
         * Default secret: must be provisioned per unit in production via a
         * manufacturing tool writing to ETX_METADATA_CF_ADDR before first boot.
         * This fallback value is deliberately non-trivial but NOT secret — replace
         * with a unique value per device for production builds.
         */
        m.stored_secret  = 0xC3A5691EUL;
        boot_metadata_write(&m);
    }
}

/* ---------------------------------------------------------------------------
 * uds.c helpers
 * --------------------------------------------------------------------------- */
void PersistBootState(uint32_t boot_reason)
{
    boot_metadata_t m;
    if (!boot_metadata_read(&m)) {
        memset(&m, 0, sizeof(m));
        m.magic         = BOOT_METADATA_MAGIC;
        m.stored_secret = 0xC3A5691EUL;
    }
    m.boot_reason    = boot_reason;
    m.update_pending = 1U;
    boot_metadata_write(&m);
}

U32 Read_Reboot_Reason(void)
{
    boot_metadata_t m;
    if (!boot_metadata_read(&m)) {
        return ETX_NORMAL_BOOT;
    }
    return m.boot_reason;
}

/* ---------------------------------------------------------------------------
 * Flash operation wrappers (program flash)
 * --------------------------------------------------------------------------- */

BOOL Drv_S32K_FlashWriteData(U32 address, const U8 *data, U32 len)
{
    /* Determine if the address targets Data Flash or Code Flash */
    iflash_kind_te kind = (address >= ETX_APP_DATA_ADDR) ? IFLASH_KIND_DFLASH : IFLASH_KIND_PFLASH;

    return (service_iflash_program(kind, address, data, len) == COM_HDR_RET_OK);
}

BOOL EraseFlashMemory(U32 address, U32 size)
{
    if (size == 0U) {
        return false;
    }

    /* Determine if the address targets Data Flash or Code Flash */
    iflash_kind_te kind = (address >= ETX_APP_DATA_ADDR) ? IFLASH_KIND_DFLASH : IFLASH_KIND_PFLASH;

    return (service_iflash_erase(kind, address, size) == COM_HDR_RET_OK);
}

S32 check_AddressRangeValid(U32 address)
{
    /* Validation for the Single Application Bank */
    if ((address >= ETX_APP_BASE_ADDRESS) && (address < ETX_APP_BANK_A_END)) {
        return 1;
    }

    return 0;
}


BOOL verify_app_crc(const boot_metadata_t *m)
{
    U32 base = ETX_APP_BASE_ADDRESS;
    U32 size = m->fw_size; /* Assumes metadata struct is updated to remove arrays */

    if (size == 0U || size > ETX_APP_BANK_SIZE) {
        return false;
    }

    U32 computed = crc32_compute((const U8 *)base, size);
    return (computed == m->fw_crc32);
}

void jump_to_app(void)
{
    U32 app_base = ETX_APP_BASE_ADDRESS;
    const U32 *vtor = (const U32 *)app_base;

    U32 sp           = vtor[0];
    U32 reset_addr   = vtor[1];

    /* 
     * S32K144 SRAM Boundaries (64 KB total):
     * 0x1FFF8000 - 0x20007000
     */
    if (!(sp >= 0x1FFF8000UL && sp <= 0x20007000UL))
    {
        return;  /* Invalid Stack Pointer — do not jump */
    }

    if ((reset_addr < app_base) || (reset_addr >= (app_base + ETX_APP_BANK_SIZE))) {
        return;  /* Reset handler is outside the expected flash bank */
    }

    INT_SYS_DisableIRQGlobal();

    /* Update the Vector Table Offset Register (VTOR) */
    S32_SCB->VTOR = app_base;

    /* Set the Main Stack Pointer (MSP) */
    /* Using CMSIS standard function: */
    __asm volatile ("MSR msp, %0" : : "r" (sp) : );

    /* Cast the reset address to a function pointer and jump */
    void (*app_reset_handler)(void) = (void (*)(void))reset_addr;
    app_reset_handler();

    while (1) {}
}

BOOL try_boot_app(boot_metadata_t *m)
{
    // srv_watchdog_refresh();

    if (!verify_app_crc(m)) {
        m->app_valid = 0U; /* Assumes metadata struct is updated to remove arrays */
        return false;
    }

    m->boot_fail_count++;
    m->app_valid = 1U;
    boot_metadata_write(m);

    // srv_watchdog_refresh();
    jump_to_app();

    m->app_valid = 0U;
    return false;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/

/* EOF --------------------------------------------------------------------------------------------------------------------------*/