/**
 * @file rr_can.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction CAN driver interface driver
 * @date 04-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 */

#ifndef CORE_LAYER_INC_RR_CAN_H_
#define CORE_LAYER_INC_RR_CAN_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver headers */

#elif defined(NXP_S32K144_146)

#include "can_pal.h"	/* NXP CAN-PAL types used in the public API (can_msg / filter mapping) */

#elif defined(RENESAS)

/* @note Reserved for Renesas CAN driver headers */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -----------------------------------------------------------------------------------------------*/

/* Public Macros -----------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 FDCAN driver macros */

#elif defined(NXP_S32K144_146)

/* NXP FlexCAN instance enable reservation macros */
#ifndef CAN_INSTANCE_0
#define CAN_INSTANCE_0	COM_HDR_ENABLED				/*!< Macro to Enable NXP CAN0 Instance Code */
#endif

#ifndef CAN_INSTANCE_1
#define CAN_INSTANCE_1	COM_HDR_DISABLED				/*!< Macro to Enable NXP CAN1 Instance Code */
#endif

#ifndef CAN_INSTANCE_2
#define CAN_INSTANCE_2	COM_HDR_DISABLED			/*!< Macro to Enable NXP CAN2 Instance Code */
#endif

#elif defined(RENESAS)

/* @note Reserved for Renesas CAN driver macros */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

#define CAN_STD_ID_MASK	(0x7FFUL)	/*!< 11-bit standard CAN identifier mask (exact-match mask) */

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/
/**
 * @brief CAN Standard Data Index Enum.
 *
 */
typedef enum can_data_index_te_tag
{
	CAN_DATABYTE1 = 0,
	CAN_DATABYTE2,
	CAN_DATABYTE3,
	CAN_DATABYTE4,
	CAN_DATABYTE5,
	CAN_DATABYTE6,
	CAN_DATABYTE7,
	CAN_DATABYTE8,
	CAN_DATABYTE_MAX
}can_data_index_te;

/**
 * @brief CAN Instance Enum alias to refer to specific CAN Instance from Application.
 *
 * @note NXP index mapping: CAN1_INST -> can_pal_0_instance, CAN2_INST -> can_pal_1_instance,
 * 		 CAN3_INST -> can_pal_2_instance (SBC).
 */
typedef enum can_inst_te_tag
{
#if CAN_INSTANCE_0
	CAN1_INST,
#endif
#if CAN_INSTANCE_1
	CAN2_INST,
#endif
#if CAN_INSTANCE_2
	CAN3_INST,
#endif
	CAN_INST_MAX
}can_inst_te;

/**
 * @brief CAN transmit mode selected per message by middleware (see @ref rr_can_transmit).
 *
 */
typedef enum can_tx_mode_te_tag
{
	CAN_TX_BLOCKING = 0,	/**< Blocking send: returns after completion or the supplied timeout */
	CAN_TX_INTERRUPT,		/**< Interrupt / non-blocking send: returns immediately */
	CAN_TX_MODE_MAX
}can_tx_mode_te;

/**
 * @brief Enum of CAN bus Error flags reported to the registered error callback (bit-mask values).
 *
 */
typedef enum can_error_flag_te_tag
{
	CAN_ERR_NONE          = 0,
	CAN_ERR_TX_WARNING    = (1U << 0),	/**< Tx error warning threshold reached */
	CAN_ERR_RX_WARNING    = (1U << 1),	/**< Rx error warning threshold reached */
	CAN_ERR_BUS_OFF       = (1U << 2),	/**< Bus-off condition detected */
	CAN_ERR_ERROR_WARNING = (1U << 3),	/**< Generic error-warning interrupt */
	CAN_ERR_MAX           = (1U << 4)	/**< Sentinel: first unused flag bit (guard / bound) */
}can_error_flag_te;

/**
 * @brief Structure to define a CAN Message
 *
 */
typedef struct __attribute__((packed)) can_msg_tst_tag
{
	U32 id_u32;      				/**< Identifier of the CAN message */
	U8 length_u8;    				/**< Length of the CAN message data payload */
	U8 rsvd_au8[3];					/**< Reserved to maintain alignment */
	U8 data_au8[CAN_DATABYTE_MAX];	/**< Data Payload of the CAN message */
} can_msg_tst;

/**
 * @brief CAN acceptance filter type exposed to middleware.
 *
 */
typedef enum can_filter_type_te_tag
{
	CAN_FILTER_ID = 0,	/**< Exact single identifier (id_start_u32) */
	CAN_FILTER_RANGE,	/**< Contiguous range id_start_u32 .. id_end_u32 */
	CAN_FILTER_TYPE_MAX
}can_filter_type_te;

/**
 * @brief Single CAN acceptance filter entry exposed to middleware (which sizes the config array).
 *
 * @note One entry per Rx filter reserved at @ref rr_can_initialize; the Rx mailbox binding is
 * 		 implicit — entry i is bound to Rx mailbox i.
 * 		 CAN_FILTER_ID uses id_start_u32 only; CAN_FILTER_RANGE uses both bounds.
 */
typedef struct can_filter_cfg_tst_tag
{
	can_filter_type_te filter_type_e;	/**< Filter type: exact ID or range */
	U32                id_start_u32;	/**< Exact ID, or range start identifier */
	U32                id_end_u32;		/**< Range end identifier (used only for CAN_FILTER_RANGE) */
} can_filter_cfg_tst;

/**
 * @brief Middleware RX callback prototype. Passed to @ref rr_can_initialize.
 *
 * @param can_inst_e CAN instance the frame was received on.
 * @param can_rx_msg_st received frame; valid only for the duration of the callback.
 */
typedef void (*rr_can_rx_cb_t)(can_inst_te can_inst_e, can_msg_tst* can_rx_msg_st);

/**
 * @brief Middleware Error callback prototype. Passed to @ref rr_can_initialize.
 *
 * @param can_inst_e CAN instance reporting the error.
 * @param error_flags_u8 Bit-mask of @ref can_error_flag_te. The middleware system error handler
 * 		  decides what recovery (if any) to perform.
 */
typedef void (*rr_can_err_cb_t)(can_inst_te can_inst_e, U8 error_flags_u8);

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every API below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR if any
 * 		 argument/operation check failed. Run-time bus errors are not reported here; they are
 * 		 dispatched to the registered error callback for the middleware system error handler.
 */
U8 rr_can_initialize(can_inst_te can_inst_e, U8 can_filters_nos_u8, rr_can_rx_cb_t const rx_cb_pf, rr_can_err_cb_t const err_cb_pf);
U8 rr_can_configMailbox(can_inst_te can_inst_e, const can_filter_cfg_tst* const cfg_arr_pst);
U8 rr_can_deinit(can_inst_te can_inst_e);

U8 rr_can_start(can_inst_te can_inst_e);
U8 rr_can_stop(can_inst_te can_inst_e);

U8 rr_can_enterSleep(can_inst_te can_inst_e);
U8 rr_can_wakeup(can_inst_te can_inst_e);

U8 rr_can_transmit(can_inst_te can_inst_e, const can_msg_tst* const can_tx_msg_st, can_tx_mode_te tx_mode_e, U32 timeout_ms_u32);

#endif /* CORE_LAYER_INC_RR_CAN_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
