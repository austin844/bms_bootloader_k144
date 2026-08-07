/**
 * @file rr_timer.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction Timer driver interface driver
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 */

#ifndef CORE_LAYER_INC_RR_TIMER_H_
#define CORE_LAYER_INC_RR_TIMER_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP FTM clock-source / prescaler types are confined to the bsp/nxp port; the public API here
 * 		 uses only the vendor-neutral @ref timer_clk_src_te / @ref timer_prescaler_te tokens. */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -----------------------------------------------------------------------------------------------*/
#include "core_layer/config/generated_code/bms/inc/rr_hw_cfg.h"	/* Consolidated build-time hardware config for this product (gen_config.py) */

/* Application Layer Includes -----------------------------------------------------------------------------------------------*/

/* Public Macros -----------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* NXP FTM instance enable reservation macros */
#ifndef TIMER_INSTANCE_0
#define TIMER_INSTANCE_0	COM_HDR_ENABLED				/*!< Macro to Enable NXP FTM0 Instance Code */
#endif

#ifndef TIMER_INSTANCE_1
#define TIMER_INSTANCE_1	COM_HDR_DISABLED				/*!< Macro to Enable NXP FTM1 Instance Code */
#endif

#ifndef TIMER_INSTANCE_2
#define TIMER_INSTANCE_2	COM_HDR_DISABLED			/*!< Macro to Enable NXP FTM3 Instance Code */
#endif

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

#define TIMER_CHANNEL_MAX_COUNT	(RR_TIMER_CHANNEL_MAX)	/*!< Number of periodic output-compare channels driven
														 	 per timer instance */

#define TIMER_MS_TO_TICKS_DIVISOR	(1000UL)	/*!< Divisor applied when converting a period(ms) to ticks */

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/
/**
 * @brief Timer Instance Enum alias to refer to specific Timer Instance from Application.
 *
 * @note NXP index mapping: TIMER1_INST -> FTM instance 0, TIMER2_INST -> FTM instance 1, TIMER3_INST
 * 		 -> FTM instance 3 (FTM instance 2 is used by PWM2_INST; see rr_pwm). The actual FTM hardware
 * 		 index applied per instance is config-driven from RR_TIMER_INSTx_HW in rr_hw_cfg.h.
 */
typedef enum timer_inst_te_tag
{
#if TIMER_INSTANCE_0
	TIMER1_INST,
#endif
#if TIMER_INSTANCE_1
	TIMER2_INST,
#endif
#if TIMER_INSTANCE_2
	TIMER3_INST,
#endif
	TIMER_INST_MAX
}timer_inst_te;

/**
 * @brief Timer channel operating mode exposed to middleware.
 *
 */
typedef enum timer_chan_mode_te_tag
{
	TIMER_CHAN_MODE_CONTINUOUS = 0,	/**< Repeats: re-arms for another period after every compare match */
	TIMER_CHAN_MODE_ONESHOT,		/**< Fires once: auto-stops (channel interrupt disabled) at its own compare match */
	TIMER_CHAN_MODE_MAX
}timer_chan_mode_te;

/**
 * @brief Middleware channel-expiry callback prototype. Passed per-channel to @ref rr_timer_init_u8
 * 		  through the injected channel config table.
 *
 * @param timer_inst_e Timer instance the channel expired on.
 * @param chan_idx_u8 Channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) that expired.
 */
typedef void (*rr_timer_cb_t)(timer_inst_te timer_inst_e, U8 chan_idx_u8);

/**
 * @brief Single timer channel configuration entry exposed to middleware (which sizes/owns the array
 * 		  passed to @ref rr_timer_init_u8).
 *
 * @note One entry per channel; entry i is bound to channel i (index-not-address). callbackParam_pv is
 * 		 handed back unchanged to callback_pf and is otherwise untouched by the driver.
 */
typedef struct timer_chan_cfg_tst_tag
{
	U8                 channel_u8;		/**< Channel index (0..@ref TIMER_CHANNEL_MAX_COUNT - 1) */
	timer_chan_mode_te chanType_e;		/**< Channel operating mode */
	U32                period_ms_u32;	/**< Channel period in milliseconds, converted to ticks at
										 	 @ref rr_timer_start_base_v */
	rr_timer_cb_t      callback_pf;	/**< Middleware callback invoked from the ISR seam on expiry; NULL for none */
	void*              callbackParam_pv;	/**< Opaque value handed back unchanged to callback_pf; unused by the driver */
} timer_chan_cfg_tst;

/**
 * @brief Vendor-neutral timer counter clock source token.
 *
 * @note Enumerator value 0 (@ref TIMER_CLK_SRC_SYSTEM) encodes the legacy implicit default, so a
 * 		 zero-initialised configuration keeps today's behaviour. The platform port maps each token to
 * 		 its vendor clock-source constant.
 */
typedef enum timer_clk_src_te_tag
{
	TIMER_CLK_SRC_SYSTEM = 0,	/**< System clock source (legacy default) */
	TIMER_CLK_SRC_FIXED,		/**< Fixed clock source */
	TIMER_CLK_SRC_EXTERNAL,		/**< External clock source */
	TIMER_CLK_SRC_MAX
}timer_clk_src_te;

/**
 * @brief Vendor-neutral timer counter clock prescaler (divider) token.
 *
 * @note Enumerator value 0 (@ref TIMER_PRESCALE_1) encodes divide-by-1, the legacy implicit default.
 * 		 The platform port maps each token to its vendor prescaler constant.
 */
typedef enum timer_prescaler_te_tag
{
	TIMER_PRESCALE_1 = 0,	/**< Divide counter clock by 1 (legacy default) */
	TIMER_PRESCALE_2,		/**< Divide counter clock by 2 */
	TIMER_PRESCALE_4,		/**< Divide counter clock by 4 */
	TIMER_PRESCALE_8,		/**< Divide counter clock by 8 */
	TIMER_PRESCALE_16,		/**< Divide counter clock by 16 */
	TIMER_PRESCALE_32,		/**< Divide counter clock by 32 */
	TIMER_PRESCALE_64,		/**< Divide counter clock by 64 */
	TIMER_PRESCALE_128,		/**< Divide counter clock by 128 */
	TIMER_PRESCALE_MAX
}timer_prescaler_te;

/**
 * @brief Timer clock/prescaler/period extension appended to the injected config, resolved by the
 * 		  platform port into the vendor timer user-config at @ref rr_timer_init_u8.
 *
 */
typedef struct timer_ftm_ext_tst_tag
{
	timer_clk_src_te   clockSelect_e;	/**< Timer counter clock source token */
	timer_prescaler_te prescaler_e;		/**< Timer counter clock prescaler token */
	U16                finalValue_u16;	/**< Timer period (modulo) register value in ticks */
} timer_ftm_ext_tst;

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every API below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR if any
 * 		 argument/operation check failed, EXCEPT @ref rr_timer_start_base_v / @ref rr_timer_stop_base_v
 * 		 (void) and @ref rr_timer_irqHandler (void), whose failures are silently absorbed by validation
 * 		 guards. Channel-expiry events are not reported here; they are dispatched to the per-channel
 * 		 callback registered in @p chan_cfg_apst via the ISR seam. @ref rr_timer_arm_channel /
 * 		 @ref rr_timer_cancel_channel are the per-channel counterpart of start/stop, meant for
 * 		 @ref TIMER_CHAN_MODE_ONESHOT channels (re-arming after their one-shot auto-stop, or cancelling
 * 		 before they fire); calling them on a @ref TIMER_CHAN_MODE_CONTINUOUS channel is also valid.
 */
U8 rr_timer_init_u8(timer_inst_te timer_inst_e, const timer_chan_cfg_tst* const chan_cfg_apst, U8 chan_cnt_u8,
					 const timer_ftm_ext_tst* const ext_pst);
U8 rr_timer_deinit(timer_inst_te timer_inst_e);

void rr_timer_start_base_v(timer_inst_te timer_inst_e);
void rr_timer_stop_base_v(timer_inst_te timer_inst_e);

U8 rr_timer_arm_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8);
U8 rr_timer_cancel_channel(timer_inst_te timer_inst_e, U8 chan_idx_u8);

void rr_timer_irqHandler(timer_inst_te timer_inst_e);

#endif /* CORE_LAYER_INC_RR_TIMER_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
