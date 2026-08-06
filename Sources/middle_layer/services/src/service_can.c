/**
 * @file service_can.c
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

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/
U8 sel_can_inst_u8 = CAN_APP_INST_1;

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
/* Forward Declarations of Private Callback Interrupt Handlers */
static void service_can_rx_callback(can_inst_te can_inst_e, can_msg_tst* rx_msg_pst);
static void service_can_err_callback(can_inst_te can_inst_e, U8 error_flags_u8);

/* Upper-Layer Function Pointer Registries */
static volatile can_app_rx_cb_t app_rx_cb_apf[CAN_APP_INST_MAX] = { NULL };
static volatile can_app_err_cb_t app_err_cb_apf[CAN_APP_INST_MAX] = { NULL };

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/**
 * @brief Hardware acceptance filter array mapping monitored network ranges.
 */
#if CAN_INSTANCE_0
static can_filter_cfg_tst app_inst_1_filters_ast[] =
{
    { .filter_type_e = CAN_FILTER_RANGE, .id_start_u32 = 0x030UL, .id_end_u32 = 0x077UL },
    { .filter_type_e = CAN_FILTER_RANGE, .id_start_u32 = 0x040UL, .id_end_u32 = 0x14FUL },
    { .filter_type_e = CAN_FILTER_RANGE, .id_start_u32 = 0x220UL, .id_end_u32 = 0x27FUL },
    { .filter_type_e = CAN_FILTER_RANGE, .id_start_u32 = 0x400UL, .id_end_u32 = 0x43FUL },
    { .filter_type_e = CAN_FILTER_ID,    .id_start_u32 = 0x666UL, .id_end_u32 = 0x666UL },
    { .filter_type_e = CAN_FILTER_RANGE, .id_start_u32 = 0x700UL, .id_end_u32 = 0x7FFUL }
};
#endif

/**
 * @brief CAN Configuration Table.
 * @details Holds application-layer configurations, mapped hardware filters,
 * and underlying driver-level interrupt routing parameters.
 */
static const can_app_channel_config_tst can_config_table_ast[] =
{
#if CAN_INSTANCE_0
    {
        .channel_e        = CAN_APP_INST_1,
        .filter_count_u8  = (U8)(sizeof(app_inst_1_filters_ast) / sizeof(app_inst_1_filters_ast[0])),
        .filter_array_pst = app_inst_1_filters_ast,
        .rx_callback_pf   = service_can_rx_callback,
        .err_callback_pf  = service_can_err_callback
    },
#endif
};

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initializes all configured CAN channels sequentially.
 * @details Loops through the configuration table, initializes the core hardware channel,
 * configures filter mailboxes, and boots the peripheral onto the active bus.
 * @return U8 COM_HDR_RET_OK on complete success, COM_HDR_RET_ERR if any stage fails.
 */
U8 service_can_init(void)
{
    U8 ret_u8 = COM_HDR_RET_OK;
    U8 num_configured_channels = (sizeof(can_config_table_ast) / sizeof(can_config_table_ast[0]));

    for(U8 i_u8 = 0; i_u8 < num_configured_channels; i_u8++)
    {
        const can_app_channel_config_tst* chan_pst = &can_config_table_ast[i_u8];
        can_inst_te core_inst_e = (can_inst_te)chan_pst->channel_e;

        /* Initialize the core instance channels */
        if(COM_HDR_RET_OK != rr_can_initialize(core_inst_e,
                                               chan_pst->filter_count_u8,
                                               chan_pst->rx_callback_pf,
                                               chan_pst->err_callback_pf))
        {
            ret_u8 = COM_HDR_RET_ERR;
        }

        /* Configure the mailbox setup using the assigned array */
        if((ret_u8 == COM_HDR_RET_OK) && (chan_pst->filter_array_pst != NULL))
        {
            if(COM_HDR_RET_OK != rr_can_configMailbox(core_inst_e, chan_pst->filter_array_pst))
            {
                ret_u8 = COM_HDR_RET_ERR;
            }
        }

        if(ret_u8 == COM_HDR_RET_OK)
        {
            if(COM_HDR_RET_OK != rr_can_start(core_inst_e))
            {
                ret_u8 = COM_HDR_RET_ERR;
            }
        }
    }
    return ret_u8;
}

/**
 * @brief Transmits a structured frame over the selected CAN channel.
 * @details Performs defensive boundary checks, packs application payloads into core driver
 * layouts, cleans reserved data space, and invokes the underlying hardware link layer.
 * @param can_channel_u8 Application channel index.
 * @param tx_msg_pst      Pointer to source frame components (ID, DLC, Data).
 * @param tx_mode_e       Blocking or Interrupt-driven transmission mode selector.
 * @param timeout_ms_u32  Maximum time allocation window for blocking cycles.
 * @return U8 COM_HDR_RET_OK on transmission dispatch, CAN_APP_ERR on invalid bounds or driver fault.
 */
U8 service_can_transmit(U8 can_channel_u8, const service_can_msg_tst* tx_msg_pst, can_app_tx_mode_te tx_mode_e, U32 timeout_ms_u32)
{
    U8 ret_u8 = COM_HDR_RET_OK;
    can_msg_tst core_msg = {0};
    can_tx_mode_te core_mode;

    /* Validate incoming application boundaries defensively */
    if ((can_channel_u8 >= CAN_APP_INST_MAX) || (tx_msg_pst == NULL))
    {
        ret_u8 = CAN_APP_ERR;
    }

    /* Safe direct cast protects against missing enum variants across layers */
    can_inst_te core_inst_e = (can_inst_te)can_channel_u8;

    /* Translate application message fields into core driver layout structure */
    core_msg.id_u32 = tx_msg_pst->id_u32;
    core_msg.length_u8 = (U8)((tx_msg_pst->length_u8 > 8U) ? 8U : tx_msg_pst->length_u8);

    core_msg.rsvd_au8[0] = 0U;
    core_msg.rsvd_au8[1] = 0U;
    core_msg.rsvd_au8[2] = 0U;

    for (U8 i_u8 = 0; i_u8 < core_msg.length_u8; i_u8++)
    {
        core_msg.data_au8[i_u8] = tx_msg_pst->data_au8[i_u8];
    }

    core_mode = (tx_mode_e == CAN_APP_TX_BLOCKING) ? CAN_TX_BLOCKING : CAN_TX_INTERRUPT;

    if (COM_HDR_RET_OK != rr_can_transmit(core_inst_e, &core_msg, core_mode, timeout_ms_u32))
    {
        ret_u8 = CAN_APP_ERR;
    }

    return ret_u8;
}

/**
 * @brief Subscribes an upper-layer module callback to received frame data events.
 * @param channel_e Target application channel index.
 * @param rx_cb_pf  Function pointer matching the application-layer receive signature.
 */
void service_can_register_rx_callback(can_app_inst_te channel_e, can_app_rx_cb_t rx_cb_pf)
{
    if (channel_e < CAN_APP_INST_MAX)
    {
        app_rx_cb_apf[channel_e] = rx_cb_pf;
    }
}

/**
 * @brief Internal driver callback intercepted on successful hardware packet reception.
 * @details Unpacks raw wire-level metrics out of driver contexts, maps instances safely,
 * and bubbles the frame structure up to high-priority application routines.
 */
void service_can_register_err_callback(can_app_inst_te channel_e, can_app_err_cb_t err_cb_pf)
{
    if (channel_e < CAN_APP_INST_MAX)
    {
        app_err_cb_apf[channel_e] = err_cb_pf;
    }
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/**
 * @brief Internal driver callback intercepted on successful hardware packet reception.
 * @details Unpacks raw wire-level metrics out of driver contexts, maps instances safely,
 * and bubbles the frame structure up to high-priority application routines.
 */
static void service_can_rx_callback(can_inst_te can_inst_e, can_msg_tst* rx_msg_pst)
{
    can_app_inst_te app_inst_e;
    service_can_msg_tst app_msg;

    app_inst_e = (can_app_inst_te)can_inst_e;

    if ((app_inst_e >= CAN_APP_INST_MAX) || (rx_msg_pst == NULL))
    {
        return;
    }

    /* Unpack received data into local structure */
    app_msg.id_u32    = rx_msg_pst->id_u32;
    app_msg.length_u8 = (U8)rx_msg_pst->length_u8;

    for (U8 i_u8 = 0; i_u8 < app_msg.length_u8; i_u8++)
    {
        app_msg.data_au8[i_u8] = rx_msg_pst->data_au8[i_u8];
    }

    can_app_rx_cb_t app_callback = app_rx_cb_apf[app_inst_e];
    if (app_callback != NULL)
    {
        app_callback(app_inst_e, &app_msg);
    }
}

/**
 * @brief Internal driver callback intercepted during critical controller error loops.
 * @details Normalizes vendor-specific hardware registers into decoupled, standard
 * application bitwise flags (Warnings, Bus-Off, etc.) and updates managers.
 */
static void service_can_err_callback(can_inst_te can_inst_e, U8 error_flags_u8)
{
    can_app_inst_te app_inst_e;
    U8 app_error_flags_u8 = (U8)CAN_APP_ERR_NONE;

    app_inst_e = (can_app_inst_te)can_inst_e;

    if (app_inst_e >= CAN_APP_INST_MAX)
    {
        return;
    }

    if ((error_flags_u8 & (U8)CAN_ERR_TX_WARNING) != 0U)
    {
        app_error_flags_u8 |= (U8)CAN_APP_ERR_TX_WARNING;
    }
    if ((error_flags_u8 & (U8)CAN_ERR_RX_WARNING) != 0U)
    {
        app_error_flags_u8 |= (U8)CAN_APP_ERR_RX_WARNING;
    }
    if ((error_flags_u8 & (U8)CAN_ERR_BUS_OFF) != 0U)
    {
        app_error_flags_u8 |= (U8)CAN_APP_ERR_BUS_OFF;
    }
    if ((error_flags_u8 & (U8)CAN_ERR_ERROR_WARNING) != 0U)
    {
        app_error_flags_u8 |= (U8)CAN_APP_ERR_ERROR_WARNING;
    }

    can_app_err_cb_t app_callback = app_err_cb_apf[app_inst_e];
    if (app_callback != NULL)
    {
        app_callback(app_inst_e, app_error_flags_u8);
    }
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
