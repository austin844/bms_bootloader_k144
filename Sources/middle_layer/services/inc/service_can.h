/**
 * @file service_can.h
 * @author divyansh
 * @brief 
 * @date 22-Jul-2026
 * 
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 * 
 */

#ifndef MIDDLE_LAYER_SERVICES_INC_SERVICE_CAN_H_
#define MIDDLE_LAYER_SERVICES_INC_SERVICE_CAN_H_

/* Common Includes ------------------------------------------------------------------------------------------------------------*/
#include "common_header.h"

/* Core Layer Includes --------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_can.h"

/* Middle Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -------------------------------------------------------------------------------------------------*/

/* Public Macros --------------------------------------------------------------------------------------------------------------*/
#define CAN_APP_OK                  (0x00U)
#define CAN_APP_ERR                 (0x01U)

/* Public TypeDefs ------------------------------------------------------------------------------------------------------------*/
typedef enum can_app_inst_te_tag
{
#if CAN_INSTANCE_0
    CAN_APP_INST_1,
#endif
#if CAN_INSTANCE_1
    CAN_APP_INST_2,
#endif
#if CAN_INSTANCE_2
    CAN_APP_INST_3,
#endif
    CAN_APP_INST_MAX
} can_app_inst_te;

typedef struct can_app_channel_config_tst_tag
{
    can_app_inst_te     channel_e;
    U8                  filter_count_u8;
    can_filter_cfg_tst* filter_array_pst;
    rr_can_rx_cb_t      rx_callback_pf;
    rr_can_err_cb_t     err_callback_pf;
} can_app_channel_config_tst;

typedef enum can_app_tx_mode_te_tag
{
    CAN_APP_TX_BLOCKING = 0,
    CAN_APP_TX_INTERRUPT
} can_app_tx_mode_te;

typedef struct service_can_msg_tst_tag
{
    U32 id_u32;
    U8  length_u8;
    U8  data_au8[8];
} service_can_msg_tst;

typedef enum can_app_error_flag_te_tag
{
    CAN_APP_ERR_NONE          = 0,
    CAN_APP_ERR_TX_WARNING    = (1U << 0),
    CAN_APP_ERR_RX_WARNING    = (1U << 1),
    CAN_APP_ERR_BUS_OFF       = (1U << 2),
    CAN_APP_ERR_ERROR_WARNING = (1U << 3)
} can_app_error_flag_te;

/* Application Layer Callback Function Pointer Prototyping */
typedef void (*can_app_rx_cb_t)(can_app_inst_te channel_e, const service_can_msg_tst* rx_msg_pst);
typedef void (*can_app_err_cb_t)(can_app_inst_te channel_e, U8 error_flags_u8);

/* Public Variable Declaration ------------------------------------------------------------------------------------------------*/
extern U8 sel_can_inst_u8;

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
U8 service_can_init(void);
U8 service_can_transmit(U8 can_channel_u8, const service_can_msg_tst* tx_msg_pst, can_app_tx_mode_te tx_mode_e, U32 timeout_ms_u32);
void service_can_register_rx_callback(can_app_inst_te channel_e, can_app_rx_cb_t rx_cb_pf);
void service_can_register_err_callback(can_app_inst_te channel_e, can_app_err_cb_t err_cb_pf);

#endif /* MIDDLE_LAYER_SERVICES_INC_SERVICE_CAN_H_ */

/* EOF ------------------------------------------------------------------------------------------------------------------------*/
