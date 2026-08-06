/**
 * @file uds_timer_lib.h
 * @author divyansh
 * @brief 
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_TIMER_LIB_H_
#define MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_TIMER_LIB_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"

/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/
typedef enum UdsTimerTypeDef_te_tag
{
    UDS_TIMER_TYPE_ECURESET = 0,
    UDS_TIMER_TYPE_S3TIMER,
    UDS_TIMER_TYPE_P2TIMER,
    UDS_TIMER_TYPE_TRANSPORT,
    UDS_TIMER_TYPE_MAX
} UdsTimerTypeDef_te;

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @brief Initializes the UDS timer library and registers it with the service_timer layer.
 * @return U8 COM_HDR_RET_OK on success, COM_HDR_RET_ERR on failure.
 */
U8 uds_timer_lib_initialize(void);

/**
 * @brief Starts or stops a specific UDS timer.
 * @param state 1 to start, 0 to stop.
 * @param type The UDS timer type to configure.
 */
void uds_timer_lib_start_and_stop_timer(uint8_t state, UdsTimerTypeDef_te type);

/**
 * @brief Retrieves the elapsed time in milliseconds for a specific UDS timer.
 * @param type The UDS timer type to read.
 * @return uint32_t Elapsed time in milliseconds.
 */
uint32_t uds_timer_lib_get_timer_msec(UdsTimerTypeDef_te type);

#endif /* MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_TIMER_LIB_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
