/**
 * @file service_wdog.c
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_watchdog.h"

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/services/inc/service_wdog.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#define SRV_WDOG_TIMEOUT_MS (5000U)

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/**
 * @brief Static configuration injected into the core layer driver.
 */
static const wdog_cfg_tst srv_wdog_config =
{
    .timeout_ms_u32       = SRV_WDOG_TIMEOUT_MS,
    .window_enable_u8     = COM_HDR_FALSE, /* Window mode disabled for simpler bootloader timing */
    .window_ms_u32        = 0U,
    .debug_mode_enable_u8 = COM_HDR_FALSE, /* Pause watchdog when core is halted in debug mode */
    .wait_mode_enable_u8  = COM_HDR_FALSE,  /* Keep watchdog active in wait mode */
    .stop_mode_enable_u8  = COM_HDR_FALSE, /* Pause watchdog in stop mode */
    .int_enable_u8        = COM_HDR_FALSE, /* No early warning interrupt needed for bootloader */
    .cb_pf                = COM_HDR_NULL_P
};

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
U8 srv_watchdog_init(void)
{
    U8 status = 0;

    /* Call the core layer initialize API for the primary watchdog instance */
    status = rr_watchdog_initialize(WATCHDOG1_INST, &srv_wdog_config);

    return status; /* Failure */
}

void srv_watchdog_refresh(void)
{
    /* Refresh the primary watchdog instance */
    rr_watchdog_refresh(WATCHDOG1_INST);
}

U8 srv_watchdog_deinit(void)
{
    U8 status;

    /* De-initialize the primary watchdog instance */
    status = rr_watchdog_deinit(WATCHDOG1_INST);

    if (status == COM_HDR_RET_OK)
    {
        return 0U; /* Success */
    }

    return 1U; /* Failure */
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
