/**
 * @file uds_timer_lib.c
 * @author divyansh
 * @brief 
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/communication_stack/inc/uds_timer_lib.h"
#include "middle_layer/services/inc/service_timer.h"
#include "middle_layer/communication_stack/inc/uds_utils.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#define UDS_TIMER_TICK_MS (10U) /* 10 ms tick period for UDS timers */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
typedef struct {
    uint8_t  type;                               /* Bitmask of active UDS timers */
    uint8_t  lock;                               /* Mutex lock state */
    uint8_t  count;                              /* Mutex lock count */
    uint8_t  sw_timer_id;                        /* Service timer ID assigned during registration */
    uint32_t period_val[UDS_TIMER_TYPE_MAX];     /* Elapsed time counters */
} UdsTmrLib_DataHandleTypeDef_t;

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
static UdsTmrLib_DataHandleTypeDef_t uds_timer_handle = {0};

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
static void uds_timer_lib_callback(void);
static void TimerLock(void);
static void TimerUnlock(void);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
U8 uds_timer_lib_initialize(void) 
{
    U8 stat = COM_HDR_RET_ERR;

    /* Register a 10ms continuous software timer with the service_timer layer */
    stat = service_timer_register(UDS_TIMER_TICK_MS,
                                  SERVICE_TIMER_MODE_CONTINUOUS,
                                  uds_timer_lib_callback,
                                  &uds_timer_handle.sw_timer_id);

    return stat;
}

void uds_timer_lib_start_and_stop_timer(uint8_t state, UdsTimerTypeDef_te type) 
{
    if (state) 
    {
        /* If no UDS timers were previously active, start the registered software timer */
        if (!uds_timer_handle.type) {
            service_timer_start(uds_timer_handle.sw_timer_id);
        }
        UDS_SET_BIT(uds_timer_handle.type, type);
    } 
    else 
    {
        UDS_CLEAR_BIT(uds_timer_handle.type, type);

        /* If no UDS timers are active anymore, stop the registered software timer */
        if (!uds_timer_handle.type) 
        {
            service_timer_stop(uds_timer_handle.sw_timer_id);
        }
    }

    /* Reset the specific timer's counter */
    uds_timer_handle.period_val[type] = 0;
}

uint32_t uds_timer_lib_get_timer_msec(UdsTimerTypeDef_te type) 
{
    uint32_t val = 0;
    TimerLock();
    val = uds_timer_handle.period_val[type];
    TimerUnlock();
    return val;
}

/* Private Function Definitions --------------------------------------------------------------*/

static void TimerLock(void) 
{
    if (uds_timer_handle.lock == 0) {
        uds_timer_handle.lock = 1;
        uds_timer_handle.count = 1;
    } else {
        uds_timer_handle.count++;
    }
}

static void TimerUnlock(void) 
{
    if (uds_timer_handle.lock == 1) {
        uds_timer_handle.count--;
        if (uds_timer_handle.count == 0) {
            uds_timer_handle.lock = 0;
        }
    }
}

/**
 * @brief Multiplexing callback. Triggered every 10ms by the service_timer layer.
 */
static void uds_timer_lib_callback(void) 
{
    TimerLock();

    if (UDS_GET_BIT(uds_timer_handle.type, UDS_TIMER_TYPE_ECURESET) == 1) 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_ECURESET] += UDS_TIMER_TICK_MS;
    } 
    else 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_ECURESET] = 0;
    }

    if (UDS_GET_BIT(uds_timer_handle.type, UDS_TIMER_TYPE_S3TIMER) == 1) 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_S3TIMER] += UDS_TIMER_TICK_MS;
    } 
    else 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_S3TIMER] = 0;
    }

    if (UDS_GET_BIT(uds_timer_handle.type, UDS_TIMER_TYPE_P2TIMER) == 1) 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_P2TIMER] += UDS_TIMER_TICK_MS;
    } 
    else 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_P2TIMER] = 0;
    }

    if (UDS_GET_BIT(uds_timer_handle.type, UDS_TIMER_TYPE_TRANSPORT) == 1) 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_TRANSPORT] += UDS_TIMER_TICK_MS;
    } 
    else 
    {
        uds_timer_handle.period_val[UDS_TIMER_TYPE_TRANSPORT] = 0;
    }

    TimerUnlock();
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
