/**
 * @file boot_metadata.h
 * @author divyansh
 * @brief  Single-Bank Boot Metadata Management for S32K144
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Mobility Pvt Ltd. All Rights Reserved 2026
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
 * S32K144 512 KB Single-Bank Configuration
 * Bootloader: 64 KB (0x00000000 - 0x0000FFFF)
 * Application: 444 KB (0x00010000 - 0x0007EFFF)
 * Metadata: 4 KB (0x0007F000 - 0x0007FFFF)
 *
 * Data flash: 0x10000000 to 0x1000FFFF 64 KB (S32K144 FlexNVM)
 * --------------------------------------------------------------------------- */

#define ETX_APP_BASE_ADDRESS       (0x00010000UL)   /* App start after 64 KB Bootloader */
#define ETX_APP_BANK_SIZE          (0x0004B000UL)   /* 300 KB for the single app slot */
#define ETX_APP_BANK_A_END         (ETX_APP_BASE_ADDRESS + ETX_APP_BANK_SIZE) /* 0x0005B000 */

/* Boot metadata block: Moved back by one 4 KB sector */
#define ETX_METADATA_CF_ADDR       (0x0007E000UL)   /* Fixed at 504 KB mark */
#define ETX_METADATA_CF_SIZE       (0x00001000UL)   /* One 4 KB sector */

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
 * Persistent boot state structure (stored in code flash — see ETX_METADATA_CF_ADDR)
 * Fields are ordered for natural 4-byte alignment — no packed attribute needed.
 * ---------------------------------------------------------------------------*/
typedef struct {
    U32 magic;                  /* BOOT_METADATA_MAGIC when struct is valid   */
    U32 fw_crc32;               /* CRC-32 of the application firmware image   */
    U32 fw_size;                /* Byte count of the application firmware     */
    U32 boot_reason;            /* ETX_NORMAL_BOOT or ETX_REPROG_REQ_FROM_UDS */
    U32 stored_secret;          /* 32-bit secret for 0x27 key derivation      */
    U8  app_valid;              /* non-zero = boot-time CRC check passed      */
    U8  update_pending;         /* set after 0x37, cleared upon boot success  */
    U8  sec_access_fail_count;  /* 0x27 attempt counter (survives ECUReset)   */
    U8  boot_fail_count;        /* incremented before jump, app clears it     */
    U8 stay_in_bootloader;
    U8  reserved[3];            /* PADDING: Forces struct size to 32 bytes    */
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

/* Persist boot_reason; sets update_pending = 1. */
void PersistBootState(U32 boot_reason);

/* Return the boot_reason field stored in code flash (ETX_NORMAL_BOOT if unreadable). */
U32 Read_Reboot_Reason(void);

/* Write firmware data to program flash via rr_internal_flash.
   Returns true on success. */
BOOL Drv_S32K_FlashWriteData(U32 address, const U8 *data, U32 len);

/* Erase [address, address+size) in program flash via rr_internal_flash.
   Returns true on success. */
BOOL EraseFlashMemory(U32 address, U32 size);

/* Returns 1 if address falls within the Application region,
   0 otherwise (rejects bootloader region and anything outside flash). */
S32 check_AddressRangeValid(U32 address);

/* Update a running CRC register with new data. Start with crc = 0xFFFFFFFF.
   Finalise with result = ~crc. */
U32 crc32_running(U32 crc, const U8 *data, U32 len);

/* Convenience: compute complete CRC-32 over [data, data+len). */
U32 crc32_compute(const U8 *data, U32 len);

/* Verify the CRC of the single application bank */
BOOL verify_app_crc(const boot_metadata_t *m);

/* Jump to the single application bank */
void jump_to_app(void);

/* Attempt to boot the application and track failure counts */
BOOL try_boot_app(boot_metadata_t *m);

#endif /* APPLICATION_LAYER_APP_UDS_INC_BOOT_METADATA_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/