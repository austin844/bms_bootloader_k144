/**
 * @file uds_utils.h
 * @author divyansh
 * @brief 
 * @date 23-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_UTILS_H_
#define MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_UTILS_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "common_header.h"

/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/
#define UDS_SET_BIT(var, bit) ((var) |= (1 << (bit)))
#define UDS_CLEAR_BIT(var, bit) ((var) &= ~(1 << (bit)))
#define UDS_GET_BIT(var, bit) (((var) >> (bit)) & 1)

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/

#endif /* MIDDLE_LAYER_COMMUNICATION_STACK_INC_UDS_UTILS_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
