/**
 * @file app_main.c
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "system_S32K144.h"
#include "interrupt_manager.h"
#include "clock_manager.h"
#include "power_manager.h"
#include "clockMan1.h"
#include "pwrMan1.h"
#include "pin_mux.h"

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/communication_stack/inc/uds_can_transport.h"
#include "middle_layer/communication_stack/inc/uds_timer_lib.h"
#include "middle_layer/communication_stack/inc/can_rx_comm.h"
#include "middle_layer/services/inc/service_iflash.h"
#include "middle_layer/services/inc/service_timer.h"
#include "middle_layer/services/inc/service_wdog.h"
#include "middle_layer/services/inc/service_can.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/
#include "application_layer/app_uds/inc/uds.h"
#include "application_layer/app_main/inc/app_main.h"
#include "application_layer/app_uds/inc/boot_metadata.h"

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#define DEMCR       (*(volatile uint32_t *)0xE000EDFCUL)   /* Debug Exception and Monitor Control */
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)

#define DEMCR_TRCENA_Msk        (1UL << 24)
#define DWT_CTRL_CYCCNTENA_Msk  (1UL << 0)

#define PWM_PERIOD_US   500U    /* 2 kHz software PWM carrier, no visible flicker */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
typedef enum
{
    PERIPH_ID_FLASH  = 0x01U,
    PERIPH_ID_WDT    = 0x02U,
    PERIPH_ID_CAN    = 0x03U,
    PERIPH_ID_TMR    = 0x04U,
    UDS_BOOTLOADER   = 0x05U
} periph_id_te;

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
static inline void DWT_DelayUs(uint32_t us);
static void peripheral_init_error_handler(periph_id_te periph_id);
static void HeartbeatLedStep(void);
static void run_bootloader_loop(void);

/* Public Function Declaration ---------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
static const U8 heartbeatLut[64] = {
    255, 215, 181, 152, 127, 106,  88,  73,
     60,  49,  40,  32,  26,  21,  17,  13,
     10,   8,   6,   4,   3,   2,   1,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0
};

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/
void app_main_v(void)
{
    boot_metadata_t m;
    BOOL stay_in_bl = false;

    /* System clock initialization */
    CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT, g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
    CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_FORCIBLE);
    POWER_SYS_Init(&powerConfigsArr, POWER_MANAGER_CONFIG_CNT, &powerStaticCallbacksConfigsArr, POWER_MANAGER_CALLBACK_CNT);
    SystemCoreClockUpdate();

    /* SWT timer reset */
    DEMCR |= DEMCR_TRCENA_Msk;
	DWT_CYCCNT = 0U;
	DWT_CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Initialize GPIO pins */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

    /* Set interrupt priority */
    INT_SYS_SetPriority(CAN0_ORed_IRQn,          2U);
    INT_SYS_SetPriority(CAN0_Error_IRQn,         2U);
    INT_SYS_SetPriority(CAN0_Wake_Up_IRQn,       2U);
    INT_SYS_SetPriority(CAN0_ORed_0_15_MB_IRQn,  2U);
    INT_SYS_SetPriority(CAN0_ORed_16_31_MB_IRQn, 2U);

    /* Initialize watchdog */
    // if(srv_watchdog_init() != 0U)
    // {
    // 	peripheral_init_error_handler(PERIPH_ID_WDT);
    // }

    /* Initialize Core Internal Flash Hardware */
    if(service_iflash_init() != 0U)
    {
        peripheral_init_error_handler(PERIPH_ID_FLASH);
    }

    // srv_watchdog_refresh();

    /* --- Boot metadata -----------------------------------------------------
     * Validate the data-flash record; writes safe defaults on first boot or
     * when the CRC is corrupt. Safe to call unconditionally here because
     * flash init succeeded above.
     * ---------------------------------------------------------------------- */
    boot_metadata_init();

    if (!boot_metadata_read(&m))
    {
        stay_in_bl = true;
    }

    // /* First-time boot: no firmware written to either bank yet */
    // if (!stay_in_bl && (m.fw_size[0] == 0U) && (m.fw_size[1] == 0U))
    // {
    //     stay_in_bl = true;
    // }

    /* --- Boot decision ------------------------------------------------------
     * 1. Try the active bank.
     * 2. If rejected, try the fallback bank
     * 3. If both are corrupt, fall through to the bootloader loop so the
     *    tester can re-flash via UDS
     * ---------------------------------------------------------------------- */
    if (!stay_in_bl)
    {
        // U8 primary  = m.active_bank & 0x01U;
        // U8 fallback = (U8)(1U - primary);

        /* Try primary bank */
        if (m.boot_fail_count < BOOT_FAIL_COUNT_MAX)
        {
            // (void)try_boot_bank(primary, &m);

            try_boot_app(&m);
            /* Returns only if CRC or vector check failed — never on success */
        }
        
        // srv_watchdog_refresh();
        
        /* Try fallback bank */
        if (boot_metadata_read(&m))
        {
            m.boot_fail_count = 0U;
            // (void)try_boot_bank(fallback, &m);
            try_boot_app(&m);
        }

        /* Both banks rejected — reset metadata to Bank A for next recovery */
        if (boot_metadata_read(&m))
        {
//            m.active_bank     = 0U;
            m.boot_fail_count = 0U;
            boot_metadata_write(&m);
        }
    }

    /* No valid application found — stay in bootloader for UDS re-flash */
    run_bootloader_loop();
}
/* Private Function Definition --------------------------------------------------------------------------------------------------*/
static void run_bootloader_loop(void)
{
    /* Initialize Middle Layer Timer Service */
    if(service_timer_init() != COM_HDR_RET_OK)
    {
        peripheral_init_error_handler(PERIPH_ID_TMR);
    }

    /* Initialize Core CAN Hardware */
    if(service_can_init() != 0U)
    {
        peripheral_init_error_handler(PERIPH_ID_CAN);
    }

    /* Initialize Bare-Metal CAN ISR Dispatcher */
    if (can_rx_comm_init() != 0U)
    {
        peripheral_init_error_handler(PERIPH_ID_CAN);
    }

    /* Initialize Application UDS Layer */
    if(UDS_Bootloader_Initialize() != 0U)
    {
        peripheral_init_error_handler(UDS_BOOTLOADER);
    }

    while(1)
    {
    	/* LED blinking setup */
    	HeartbeatLedStep();

    	/* Bootloader state machine */
    	UDS_Bootloader_state_machine();

    	/* Refresh watchdog */
    	// srv_watchdog_refresh();
    }

}

static void HeartbeatLedStep(void)
{
    static U8  lutIdx        = 0U;
    static U32 stepAccumUs   = 0U;

    /* Using the new heartbeat LUT */
    U32 duty    = heartbeatLut[lutIdx];
    U32 onTime  = (PWM_PERIOD_US * duty) / 255U;
    U32 offTime = PWM_PERIOD_US - onTime;

    if (onTime > 0U)
    {
        PINS_DRV_SetPins(PTE, ((U32) 1U << 16));
        DWT_DelayUs(onTime);
    }
    if (offTime > 0U)
    {
        PINS_DRV_ClearPins(PTE, ((U32) 1U << 16));
        DWT_DelayUs(offTime);
    }

    stepAccumUs += PWM_PERIOD_US;

    /* Advance the step exactly every 15,625 microseconds */
    if (stepAccumUs >= 15625U)
    {
        /* Subtract instead of resetting to 0 to prevent timing drift */
        stepAccumUs -= 15625U;

        lutIdx = (lutIdx + 1U) & 0x3FU; /* Wraps around back to 0 after 63 */
    }
}

static inline void DWT_DelayUs(uint32_t us)
{
	uint32_t cycles = us * (SystemCoreClock / 1000000U);
	uint32_t start = DWT_CYCCNT;
	while ((DWT_CYCCNT - start) < cycles) { }
}

/*
 * Private: peripheral init error handler
 *
 * Called when a mandatory peripheral fails to open. Disables all interrupts
 * and enters an infinite spin so the hardware WDT (if enabled) can force a
 * clean reset. The periph_id argument is kept visible to a debugger via the
 * volatile local � it is not used at runtime.
 */
static void peripheral_init_error_handler(periph_id_te periph_id)
{
    volatile periph_id_te failed_periph = periph_id;   /* readable in debugger */
    (void)failed_periph;

    INT_SYS_DisableIRQGlobal();

    /* Spin forever � hardware WDT will reset the MCU after its timeout.
     * If WDT is not auto-started and this is a flash-open failure the system
     * cannot recover without an external reset (power cycle or debug probe). */
    while (1)
    {
        ; /* intentional infinite loop � do not remove */
    }
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
