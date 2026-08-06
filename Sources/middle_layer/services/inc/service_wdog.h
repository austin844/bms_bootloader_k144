/**
 * @file service_wdog.h
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_SERVICES_INC_SERVICE_WDOG_H_
#define MIDDLE_LAYER_SERVICES_INC_SERVICE_WDOG_H_

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
 * @brief Initializes the watchdog service.
 * @details Injects a predefined bootloader configuration (5-second timeout,
 *          no window mode) into the core watchdog driver and starts the timer[cite: 3].
 * @return 0 on success, non-zero on failure.
 */
U8 srv_watchdog_init(void);

/**
 * @brief Refreshes (services) the watchdog timer.
 * @details Reloads the watchdog counter for the primary instance to prevent
 *          a system reset[cite: 3]. Must be called periodically before the
 *          timeout period expires.
 */
void srv_watchdog_refresh(void);

/**
 * @brief De-initializes the watchdog service.
 * @details Disables the primary watchdog instance[cite: 3], provided the
 *          hardware allows for the watchdog to be disabled after initialization.
 * @return 0 on success, non-zero on failure.
 */
U8 srv_watchdog_deinit(void);

#endif /* MIDDLE_LAYER_SERVICES_INC_SERVICE_WDOG_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
