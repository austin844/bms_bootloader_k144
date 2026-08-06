/**
 * @file service_timer.h
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_SERVICES_INC_SERVICE_TIMER_H_
#define MIDDLE_LAYER_SERVICES_INC_SERVICE_TIMER_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"
/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/
#define SERVICE_TIMER_MAX_SW_TIMERS (10U) /* Maximum number of SW timers the application can register */

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/
/**
 * @brief Software timer operating modes.
 */
typedef enum service_timer_mode_te_tag
{
    SERVICE_TIMER_MODE_ONESHOT = 0,    /**< Timer fires once and auto-stops */
    SERVICE_TIMER_MODE_CONTINUOUS      /**< Timer re-arms automatically after firing */
} service_timer_mode_te;

/**
 * @brief Application callback prototype for software timers.
 */
typedef void (*service_sw_timer_cb_t)(void);

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @brief Initializes the service layer and configures the core hardware timer tick.
 *        The application layer only needs to call this once.
 * @return U8 COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_timer_init(void);

/**
 * @brief Registers a new software timer.
 * @param period_ms   Timeout period in milliseconds.
 * @param mode        Continuous or One-shot mode.
 * @param callback    Function to execute when the timer expires.
 * @param timer_id_out Pointer to store the allocated timer ID.
 * @return U8 COM_HDR_RET_OK on success, COM_HDR_RET_ERR if registration is full.
 */
U8 service_timer_register(U32 period_ms, service_timer_mode_te mode, service_sw_timer_cb_t callback, U8* timer_id_out);

/**
 * @brief Starts an active software timer.
 * @param timer_id ID of the registered software timer.
 * @return U8 COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_timer_start(U8 timer_id);

/**
 * @brief Stops a software timer.
 * @param timer_id ID of the registered software timer.
 * @return U8 COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 service_timer_stop(U8 timer_id);
#endif /* MIDDLE_LAYER_SERVICES_INC_SERVICE_TIMER_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
