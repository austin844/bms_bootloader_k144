/**
 * @file can_rx_comm.c
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/

/* Middle Layer Includes -------------------------------------------------------------------------------------------------*/
#include "middle_layer/services/inc/service_can.h"
#include "middle_layer/communication_stack/inc/can_rx_comm.h"
#include "middle_layer/communication_stack/inc/uds_can_transport.h"
/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
static void can_rx_comm_callback(can_app_inst_te channel_e, const service_can_msg_tst* rx_msg_pst);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Allocates and binds the low-level rx callback.
 * @details Registers the bare-metal interrupt callback to the ECU Abstraction Layer.
 * @return U8 CAN_APP_OK on success.
 */
U8 can_rx_comm_init(void)
{
    /* Bind the callback directly to the ECU Abstraction Layer */
	service_can_register_rx_callback(CAN_APP_INST_1, can_rx_comm_callback);

    return CAN_APP_OK;
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Hard-interrupt abstraction callback bound directly to the hardware peripheral driver.
 * @details Executes in strict ISR context. Identifies source network channels,
 * filters UDS requests, and routes payload directly to the ISO-TP stack.
 */
static void can_rx_comm_callback(can_app_inst_te channel_e, const service_can_msg_tst* rx_msg_pst)
{
    if (rx_msg_pst == NULL)
    {
        return;
    }

    if (CAN_APP_INST_1 == channel_e)
    {
        /* Filter IDs meant for the Bootloader ISO-TP stack */
        if ((rx_msg_pst->id_u32 == BOOTLOADER_RX_DATA_ID) 		||
            (rx_msg_pst->id_u32 == BOOTLOADER_RX_BOOT_CMD_ID) 	||
            (rx_msg_pst->id_u32 == BOOTLOADER_RX_EN_CMD_ID)		||
			(rx_msg_pst->id_u32 == BOOTLOADER_RX_FUNCTIONAL_CMD_ID))
        {
            /* DIRECT EXECUTION: Feed ISO-TP stack immediately inside the ISR */
            isotp_can_message_recvhndlr(rx_msg_pst->data_au8, rx_msg_pst->length_u8);
        }
    }
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
