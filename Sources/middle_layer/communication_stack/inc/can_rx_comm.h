/**
 * @file can_rx_comm.h
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_COMMUNICATION_STACK_INC_CAN_RX_COMM_H_
#define MIDDLE_LAYER_COMMUNICATION_STACK_INC_CAN_RX_COMM_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"

/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @brief Initializes the CAN RX communication mechanism.
 * @return U8 CAN_APP_OK on successful callback registration.
 */
U8 can_rx_comm_init(void);

#endif /* MIDDLE_LAYER_COMMUNICATION_STACK_INC_CAN_RX_COMM_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
