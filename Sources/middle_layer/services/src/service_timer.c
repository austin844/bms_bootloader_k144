/**
 * @file service_timer.c
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_timer.h"
/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/services/inc/service_timer.h"

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#define SERVICE_HW_BASE_TICK_MS  (1U)            /* 1 millisecond hardware tick */
#define SERVICE_HW_TIMER_INST    (TIMER1_INST)   /* Hardware timer instance to use */
#define SERVICE_HW_TIMER_CHAN    (0U)            /* Hardware timer channel to use */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/
/**
 * @brief Internal tracking structure for each software timer.
 */
typedef struct {
    U32                   period_ms;
    U32                   elapsed_ms;
    service_sw_timer_cb_t callback;
    service_timer_mode_te mode;
    BOOL                  is_running;
    BOOL                  is_allocated;
} service_sw_timer_tst;

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
static service_sw_timer_tst service_sw_timers_ast[SERVICE_TIMER_MAX_SW_TIMERS];
static timer_chan_cfg_tst   service_hw_chan_cfg_st;
static timer_ftm_ext_tst    service_hw_ext_cfg_st;

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
static void service_timer_hw_tick_cb(timer_inst_te timer_inst_e, U8 chan_idx_u8);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
U8 service_timer_init(void) {
    U8 ret_u8 = COM_HDR_RET_OK;

    /* 1. Clear out all software timers */
    for (U8 i = 0; i < SERVICE_TIMER_MAX_SW_TIMERS; i++)
    {
        service_sw_timers_ast[i].is_allocated = COM_HDR_FALSE;
        service_sw_timers_ast[i].is_running   = COM_HDR_FALSE;
        service_sw_timers_ast[i].elapsed_ms   = 0U;
    }

    /* 2. Configure the core layer hardware timer channel */
    service_hw_chan_cfg_st.channel_u8       = SERVICE_HW_TIMER_CHAN;
    service_hw_chan_cfg_st.chanType_e       = TIMER_CHAN_MODE_CONTINUOUS; /* Re-arms for another period after every compare match */
    service_hw_chan_cfg_st.period_ms_u32    = SERVICE_HW_BASE_TICK_MS;
    service_hw_chan_cfg_st.callback_pf      = service_timer_hw_tick_cb;   /* Handled by the ISR seam on expiry */
    service_hw_chan_cfg_st.callbackParam_pv = NULL;

    /* 3. Configure the core layer FTM extension settings */
    service_hw_ext_cfg_st.clockSelect_e  = TIMER_CLK_SRC_SYSTEM;          /* System clock source (legacy default) */
    service_hw_ext_cfg_st.prescaler_e    = TIMER_PRESCALE_8;              /* Divide counter clock by 1 (legacy default) */
    service_hw_ext_cfg_st.finalValue_u16 = 0xFFFFU;                       /* Max out the 16-bit register */

    /* 4. Initialize the underlying driver */
    ret_u8 = rr_timer_init_u8(SERVICE_HW_TIMER_INST, &service_hw_chan_cfg_st, 1U, &service_hw_ext_cfg_st); /* */

    /* 5. Start the timer base to begin generating ticks */
    if (ret_u8 == COM_HDR_RET_OK)
    {
        rr_timer_start_base_v(SERVICE_HW_TIMER_INST); /* Enables the per-channel compare interrupt */
    }

    return ret_u8;
}

U8 service_timer_register(U32 period_ms, service_timer_mode_te mode, service_sw_timer_cb_t callback, U8* timer_id_out) {
    U8 ret_u8 = COM_HDR_RET_ERR;

    if ((callback != NULL) && (timer_id_out != NULL) && (period_ms > 0))
    {
        for (U8 i = 0; i < SERVICE_TIMER_MAX_SW_TIMERS; i++)
        {
            if (!service_sw_timers_ast[i].is_allocated)
            {
                /* Allocate and assign the configuration */
                service_sw_timers_ast[i].period_ms    = period_ms;
                service_sw_timers_ast[i].callback     = callback;
                service_sw_timers_ast[i].mode         = mode;
                service_sw_timers_ast[i].is_allocated = COM_HDR_TRUE;
                service_sw_timers_ast[i].is_running   = COM_HDR_FALSE;
                service_sw_timers_ast[i].elapsed_ms   = 0U;

                *timer_id_out = i;
                ret_u8 = COM_HDR_RET_OK;
                break; /* Successfully registered, exit loop */
            }
        }
    }
    return ret_u8;
}

U8 service_timer_start(U8 timer_id) {
    U8 ret_u8 = COM_HDR_RET_ERR;

    if ((timer_id < SERVICE_TIMER_MAX_SW_TIMERS) && (service_sw_timers_ast[timer_id].is_allocated))
    {
        service_sw_timers_ast[timer_id].elapsed_ms = 0U; /* Reset count on start */
        service_sw_timers_ast[timer_id].is_running = COM_HDR_TRUE;
        ret_u8 = COM_HDR_RET_OK;
    }
    return ret_u8;
}

U8 service_timer_stop(U8 timer_id) {
    U8 ret_u8 = COM_HDR_RET_ERR;

    if (timer_id < SERVICE_TIMER_MAX_SW_TIMERS)
    {
        service_sw_timers_ast[timer_id].is_running = COM_HDR_FALSE;
        ret_u8 = COM_HDR_RET_OK;
    }
    return ret_u8;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
static void service_timer_hw_tick_cb(timer_inst_te timer_inst_e, U8 chan_idx_u8)
{
    COM_HDR_UNUSED(timer_inst_e);
    COM_HDR_UNUSED(chan_idx_u8);

    /* Iterate over software timers and process active ones */
    for (U8 i = 0; i < SERVICE_TIMER_MAX_SW_TIMERS; i++)
    {
        if (service_sw_timers_ast[i].is_allocated && service_sw_timers_ast[i].is_running)
        {
            service_sw_timers_ast[i].elapsed_ms += SERVICE_HW_BASE_TICK_MS;

            /* Check if the software timer period has been met */
            if (service_sw_timers_ast[i].elapsed_ms >= service_sw_timers_ast[i].period_ms)
            {

                /* Reset elapsed time for next cycle */
                service_sw_timers_ast[i].elapsed_ms = 0U;

                /* Fire application callback */
                service_sw_timers_ast[i].callback();

                /* Auto-stop if operating in one-shot mode */
                if (service_sw_timers_ast[i].mode == SERVICE_TIMER_MODE_ONESHOT)
                {
                    service_sw_timers_ast[i].is_running = COM_HDR_FALSE;
                }
            }
        }
    }
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
