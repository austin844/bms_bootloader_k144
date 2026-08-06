/**
 * @file rr_gpio.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction GPIO driver interface driver
 * @date 11-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 */

#ifndef CORE_LAYER_INC_RR_GPIO_H_
#define CORE_LAYER_INC_RR_GPIO_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/

/* Configuration Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -----------------------------------------------------------------------------------------------*/

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/
/**
 * @brief Logical GPIO port index; rr_gpio.c resolves it to the vendor port base address.
 *
 */
typedef enum gpio_port_te_tag
{
#if defined(STM32)

	/* @note Reserved for STM32 port indices */
	GPIO_PORT_MAX

#elif defined(NXP_S32K144_146)

	GPIO_PORT_PTA = 0,	/**< NXP port A (PTA) */
	GPIO_PORT_PTB,		/**< NXP port B (PTB) */
	GPIO_PORT_PTC,		/**< NXP port C (PTC) */
	GPIO_PORT_PTD,		/**< NXP port D (PTD) */
	GPIO_PORT_PTE,		/**< NXP port E (PTE) */
	GPIO_PORT_MAX

#elif defined(RENESAS)

	/* @note Reserved for Renesas RA port indices */
	GPIO_PORT_MAX

#else

	/* No platform defined: only the sentinel, every channel check fails */
	GPIO_PORT_MAX

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
}gpio_port_te;

/**
 * @brief Pin direction of one logical channel.
 *
 */
typedef enum gpio_dir_te_tag
{
	GPIO_DIR_INPUT = 0,	/**< Digital input */
	GPIO_DIR_OUTPUT,	/**< Driven digital output */
	GPIO_DIR_ALTFUNC,	/**< Pin owned by a peripheral function; rr_gpio only validates the entry */
	GPIO_DIR_MAX
}gpio_dir_te;

/**
 * @brief Logical pin state used by the read/write API; polarity mapping to the
 * 		  physical level is done inside the driver.
 *
 */
typedef enum gpio_pin_state_te_tag
{
	GPIO_STATE_LOW = 0,	/**< Logical de-asserted state */
	GPIO_STATE_HIGH = 1,	/**< Logical asserted state */
	GPIO_STATE_ERROR,	/**< Returned by @ref rr_gpio_readPin on any validation failure */
	GPIO_STATE_MAX
}gpio_pin_state_te;

/**
 * @brief Electrical polarity of a channel; declared once here so callers never invert.
 *
 */
typedef enum gpio_polarity_te_tag
{
	GPIO_ACTIVE_HIGH = 0,	/**< GPIO_STATE_HIGH corresponds to a high pin level */
	GPIO_ACTIVE_LOW,	/**< GPIO_STATE_HIGH corresponds to a low pin level */
	GPIO_POLARITY_MAX
}gpio_polarity_te;

/**
 * @brief Interrupt edge selection per channel; configured in the table now, consumed by
 * 		  the Phase-2 interrupt API together with rr_nvic.
 *
 */
typedef enum gpio_irq_edge_te_tag
{
	GPIO_IRQ_NONE = 0,	/**< No interrupt on this channel (polled pin) */
	GPIO_IRQ_RISING,	/**< Interrupt on rising edge */
	GPIO_IRQ_FALLING,	/**< Interrupt on falling edge */
	GPIO_IRQ_BOTH,		/**< Interrupt on both edges */
	GPIO_IRQ_EDGE_MAX
}gpio_irq_edge_te;

/**
 * @brief One logical GPIO channel. The configuration layer injects an array of these at
 * 		  @ref rr_gpio_initialize; the index in that array is the public handle of the pin.
 *
 * @note Electrical configuration (mux/pull/drive strength) lives in the platform's
 * 		 generated pin configuration (NXP pin_mux / Renesas FSP pin cfg); this table holds
 * 		 only the logical view of each pin.
 */
typedef struct gpio_channel_cfg_tst_tag
{
	gpio_port_te      port_e;		/**< Logical port index; never a base address */
	gpio_dir_te       dir_e;		/**< Channel direction; writes/toggles fail on non-outputs */
	gpio_polarity_te  polarity_e;		/**< Active level of the pin */
	gpio_pin_state_te default_state_e;	/**< Logical state driven at init (outputs only) */
	gpio_irq_edge_te  irq_edge_e;		/**< Interrupt edge; GPIO_IRQ_NONE for polled pins */
	U8                pin_u8;		/**< Pin number within the port (not a bitmask); last to avoid interior padding */

#if defined(STM32)

	/* @note Reserved for STM32 per-pin electrical fields (STM32 has no generated pin-mux) */

#elif defined(NXP_S32K144_146)

	/* Electrical config comes from the generated pin_mux table; nothing extra per channel */

#elif defined(RENESAS)

	/* @note Reserved for Renesas RA ICU interrupt-channel field */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
} gpio_channel_cfg_tst;

/**
 * @brief Single middleware callback registered once via @ref rr_gpio_initialize and dispatched
 * 		  (in ISR context) for every interrupt-enabled channel; middleware demuxes by
 * 		  ch_index_u8 using its own copy of the channel table.
 *
 * @param ch_index_u8 Channel index (into the injected table) whose configured edge fired.
 * @param state_e Logical (polarity-mapped) pin state sampled when the callback is dispatched.
 *
 * @note Runs in ISR context: keep it short, no blocking calls.
 */
typedef void (*rr_gpio_irq_cb_t)(U8 ch_index_u8, gpio_pin_state_te state_e);

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every API below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR if any
 * 		 argument/channel check failed, except @ref rr_gpio_readPin (returns the pin state or
 * 		 @c GPIO_STATE_ERROR) and @ref rr_gpio_irqHandler (void ISR body). Each function is
 * 		 documented above its definition in rr_gpio.c.
 */
U8 rr_gpio_initialize(const gpio_channel_cfg_tst* const cfg_pst, U8 count_u8, rr_gpio_irq_cb_t const cb_pf);
U8 rr_gpio_deinit(void);

U8 rr_gpio_writePin(U8 ch_index_u8, gpio_pin_state_te state_e);
gpio_pin_state_te rr_gpio_readPin(U8 ch_index_u8);
U8 rr_gpio_togglePin(U8 ch_index_u8);

U8 rr_gpio_enableInterrupt(U8 ch_index_u8);
U8 rr_gpio_disableInterrupt(U8 ch_index_u8);
void rr_gpio_irqHandler(gpio_port_te port_e);

#endif /* CORE_LAYER_INC_RR_GPIO_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
