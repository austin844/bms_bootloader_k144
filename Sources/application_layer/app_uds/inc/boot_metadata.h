/**
 * @file boot_metadata.h
 * @author divyansh
 * @brief 
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef APPLICATION_LAYER_APP_UDS_INC_BOOT_METADATA_H_
#define APPLICATION_LAYER_APP_UDS_INC_BOOT_METADATA_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"

/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------
 * S32K146 1 MB flash layout
 * ---------------------------------------------------------------------------
 * Code flash  : 0x00000000 to 0x000FFFFF  (FLASH_START = 0x00000000,
 *                                          FLASH_LENGTH = 0x100000)
 *
 *   Bootloader  : 0x00000000 to 0x0001FFFF   128 KB
 *   App1/Bank A : 0x00020000 to 0x00087FFF   416 KB
 *   App2/Bank B : 0x00088000 to 0x000EFFFF   416 KB
 *
 *   Boot metadata: 0x000F0000 to 0x000F7FFF   32 KB
 *   Unused       : 0x000F8000 t0 0x000FFFFF   32 KB
 *
 * Data flash  : 0x10000000 t0 0x1000FFFF   64 KB (S32K146 FlexNVM)
 * --------------------------------------------------------------------------- */
#define ETX_APP_BASE_ADDRESS       (0x00020000UL)   /* App1/Bank A start (after 128 KB BL) */
#define ETX_APP_BANK_SIZE          (0x00068000UL)   /* 416 KB per app slot */
#define ETX_APP_BANK_B_ADDRESS     (ETX_APP_BASE_ADDRESS   + ETX_APP_BANK_SIZE)  /* 0x00088000 */
#define ETX_APP_BANK_A_END         (ETX_APP_BASE_ADDRESS   + ETX_APP_BANK_SIZE)
#define ETX_APP_BANK_B_END         (ETX_APP_BANK_B_ADDRESS + ETX_APP_BANK_SIZE)  /* 0x000F0000 */

/* Boot metadata block: Bootloader-owned � the application MUST NOT use this block. */
#define ETX_METADATA_CF_ADDR       (ETX_APP_BANK_B_ADDRESS + ETX_APP_BANK_SIZE)  /* 0x000F0000 */
#define ETX_METADATA_CF_SIZE       (0x00008000UL)   /* one 32 KB block */

/* App-data region: Mapped to FlexNVM Data Flash. */
#define ETX_APP_DATA_ADDR          (0x10000000UL)
#define ETX_APP_DATA_END           (ETX_APP_DATA_ADDR + (64 * 1024UL))
#define ETX_MAX_DATA_SIZE_ERASE    (4096U) /* Common sector erase size for FlexNVM */
#define ETX_MAX_DATA_SIZE_WRITE    (8U)    /* Phrase size write */

/* boot_reason field values */
#define ETX_NORMAL_BOOT            (0x4E4F524DUL)  /* 'NORM' */
#define ETX_REPROG_REQ_FROM_UDS    (0x55445300UL)  /* 'UDS\0' */

#define BOOT_METADATA_MAGIC        (0xBEEFCAFEUL)

/* Application stops trying a failing bank after this many attempts */
#define BOOT_FAIL_COUNT_MAX        (3U)

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------
 * Persistent boot state structure (stored in code flash � see ETX_METADATA_CF_ADDR)
 * Fields are ordered for natural 4-byte alignment � no packed attribute needed.
 * ---------------------------------------------------------------------------*/
typedef struct {
    U32 magic;                  /* BOOT_METADATA_MAGIC when struct is valid  */
    U32 fw_crc32[2];            /* CRC-32 of each bank's firmware image       */
    U32 fw_size[2];             /* Byte count of each bank's firmware image   */
    U32 boot_reason;            /* ETX_NORMAL_BOOT or ETX_REPROG_REQ_FROM_UDS */
    U32 stored_secret;          /* 32-bit secret for 0x27 key derivation      */
    U8  active_bank;            /* 0 = Bank A, 1 = Bank B                     */
    U8  bank_valid[2];          /* non-zero = boot-time CRC check passed      */
    U8  update_pending;         /* set after 0x37, cleared after bank swap    */
    U8  sec_access_fail_count;  /* 0x27 attempt counter (survives ECUReset)   */
    U8  boot_fail_count;        /* incremented before jump, app clears it     */
    U8  _pad[2];                /* alignment pad                              */
    U32 crc_self;               /* CRC-32 of all fields above this one        */
} boot_metadata_t;

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/* Initialise metadata: reads from code flash; writes defaults if blank/corrupt. */
void boot_metadata_init(void);

/* Read the current metadata into *out. Returns false if magic or CRC is bad. */
BOOL boot_metadata_read(boot_metadata_t *out);

/* Erase + re-write the metadata block. Recomputes crc_self before writing.
   Returns false on flash error. */
BOOL boot_metadata_write(const boot_metadata_t *in);

/* Persist boot_reason and active_slot; sets update_pending = 1. */
void PersistBootState(U32 boot_reason, U8 active_slot);

/* Return the boot_reason field stored in code flash (ETX_NORMAL_BOOT if unreadable). */
U32 Read_Reboot_Reason(void);

/* Return the active_bank field (0 = Bank A on error). */
U8 Read_ActiveSlotForApplication(void);

/* Write firmware data to program flash via rr_internal_flash.
   Returns true on success. */
BOOL Drv_S32K_FlashWriteData(U32 address, const U8 *data, U32 len);

/* Erase [address, address+size) in program flash via rr_internal_flash.
   Returns true on success. */
BOOL EraseFlashMemory(U32 address, U32 size);

/* Returns 1 if address falls within Bank A or Bank B application region,
   0 otherwise (rejects bootloader region and anything outside flash). */
S32 check_AddressRangeValid(U32 address);

/* Update a running CRC register with new data. Start with crc = 0xFFFFFFFF.
   Finalise with result = ~crc. */
U32 crc32_running(U32 crc, const U8 *data, U32 len);

/* Convenience: compute complete CRC-32 over [data, data+len). */
U32 crc32_compute(const U8 *data, U32 len);

BOOL verify_bank_crc(U8 bank, const boot_metadata_t *m);

void jump_to_bank(U8 bank);

BOOL try_boot_bank(U8 bank, boot_metadata_t *m);

BOOL boot_metadata_swap_active_bank(void);

#endif /* APPLICATION_LAYER_APP_UDS_INC_BOOT_METADATA_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
