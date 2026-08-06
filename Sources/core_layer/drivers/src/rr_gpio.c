/**
 * @file rr_gpio.c
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction GPIO driver interface driver.
 * @date 11-Jun-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note The functions that other code calls are the same on every chip. Inside each function the
 * 		 \#if defined(STM32) / NXP_S32K144_146 / RENESAS lines pick the code for the chip being built.
 * 		 The configuration layer injects its channel table at rr_gpio_initialize(); the driver keeps
 * 		 only the table pointer and ships no board pin list. Active-low pins are declared once in
 * 		 that table and every read/write maps logical<->physical levels here, so callers never
 * 		 invert. Electrical pin setup (mux/pull/drive) stays in the platform's generated pin config
 * 		 (NXP pin_mux). Interrupt pins are armed with the edge declared in the table via
 * 		 rr_gpio_enableInterrupt() and dispatched from rr_gpio_irqHandler(), which the vector
 * 		 layer (rr_nvic in Phase 2, project startup code until then) must call; this file
 * 		 configures only the pin-level interrupt source and touches no NVIC registers.
 *
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_gpio.h"	/* This module's public API, channel table and state types */

#if defined(STM32)

/* @note Reserved for STM32 HAL GPIO driver headers */

#elif defined(NXP_S32K144_146)

#include "core_layer/bsp/nxp/inc/rr_gpio_nxp.h"	/* NXP port: rr_gpio_nxp_* PINS_DRV-backed helpers (PINS_DRV_* confined to bsp/nxp) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA r_ioport driver headers */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 GPIO driver macros */
#define GPIO_PINS_PER_PORT	(0U)	/*!< No channel is valid until the STM32 branch is implemented */

#elif defined(NXP_S32K144_146)

/* @note GPIO_PINS_PER_PORT is provided by rr_gpio_nxp.h; the pin-mask helper lives in the bsp/nxp port (rr_gpio_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA GPIO driver macros */
#define GPIO_PINS_PER_PORT	(0U)	/*!< No channel is valid until the RENESAS branch is implemented */

#else

#define GPIO_PINS_PER_PORT	(0U)	/*!< No platform: every channel check fails */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 GPIO port lookup tables */

#elif defined(NXP_S32K144_146)

/* @note NXP port/pin base LUTs and the IRQC edge table live in the bsp/nxp port (rr_gpio_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA GPIO port lookup tables */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Channel table injected by the configuration layer at rr_gpio_initialize(); external linkage so the
   bsp/nxp port can reach it through the extern declarations in rr_gpio_nxp.h */
const gpio_channel_cfg_tst* gpio_channel_cfg_pst = COM_HDR_NULL_P;

U8 gpio_channel_count_u8 = 0U;

/* Single middleware callback, registered at init, read from ISR context; external linkage - see above */
rr_gpio_irq_cb_t volatile gpio_irq_cb_pf = COM_HDR_NULL_P;

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 HAL GPIO private helpers */

#elif defined(NXP_S32K144_146)

/* @note NXP versions of each function (rr_gpio_nxp_* helpers, the public functions dispatch to these)
 * 		 are declared in rr_gpio_nxp.h and defined in the bsp/nxp port (rr_gpio_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA GPIO private helpers */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

#if !defined(NXP_S32K144_146)

/* External linkage (not static): the active port calls these through the extern declarations in its
   seam header to validate channels and map logical<->physical levels through the same implementation.
   On NXP the single declaration lives in rr_gpio_nxp.h (included above), so declaring them here too
   would spread the external declaration across two files (MISRA C:2012 Rule 8.5); this forward
   declaration is therefore compiled only for builds whose seam header is not yet present. */
U8 rr_gpio_check_channel(U8 ch_index_u8);

U8 rr_gpio_map_level(U8 level_u8, gpio_polarity_te polarity_e);

#endif /* !NXP_S32K144_146 */

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief Initialize the GPIO driver with the configuration layer's channel table and apply
 * 		  the platform's generated electrical pin configuration.
 *
 * @param cfg_pst Channel table injected by the configuration layer; must stay valid (flash or
 * 		  static storage) for the whole driver lifetime - the driver keeps only the pointer.
 * @param count_u8 Number of entries in @p cfg_pst; must be non-zero.
 * @param cb_pf Middleware callback dispatched from ISR context for every interrupt-enabled
 * 		  channel; may be @c COM_HDR_NULL_P only if no channel in the table configures an
 * 		  interrupt edge.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR if any argument or entry is invalid or
 * 		   the platform pin init failed.
 *
 * @note Output channels are driven to their @c default_state_e here; one bad entry does not
 * 		 stop the remaining entries but fails the return value. The callback check runs after
 * 		 pin configuration completes.
 */
U8 rr_gpio_initialize(const gpio_channel_cfg_tst* const cfg_pst, U8 count_u8, rr_gpio_irq_cb_t const cb_pf)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((COM_HDR_NULL_P == cfg_pst) || (0U == count_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		gpio_channel_cfg_pst  = cfg_pst;
		gpio_channel_count_u8 = count_u8;

#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_initialize(cfg_pst, count_u8, cb_pf);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return ret_u8;
}

/**
 * @brief Deinitialize every channel, clear the middleware callback, and forget the injected
 * 		  table; the driver is unusable until @ref rr_gpio_initialize is called again.
 *
 * @return @c COM_HDR_RET_OK if every channel deinitialized, @c COM_HDR_RET_ERR if the driver was
 * 		   not initialized or any channel failed.
 */
U8 rr_gpio_deinit(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(COM_HDR_NULL_P == gpio_channel_cfg_pst)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_deinit();

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */

#else

		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

		/* Forget the injected table and callback even if a pin failed; driver is unusable past this point */
		gpio_channel_cfg_pst  = COM_HDR_NULL_P;
		gpio_channel_count_u8 = 0U;
		gpio_irq_cb_pf        = COM_HDR_NULL_P;
	}

	return ret_u8;
}

/**
 * @brief Drive an output channel to a logical state; the driver maps it to the physical
 * 		  level per the channel's polarity.
 *
 * @param ch_index_u8 Channel index into the injected table; must be @c GPIO_DIR_OUTPUT.
 * @param state_e @c GPIO_STATE_LOW or @c GPIO_STATE_HIGH only.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on validation failure or a write to a
 * 		   non-output channel.
 */
U8 rr_gpio_writePin(U8 ch_index_u8, gpio_pin_state_te state_e)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8)) || (state_e > GPIO_STATE_HIGH))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if(GPIO_DIR_OUTPUT != gpio_channel_cfg_pst[ch_index_u8].dir_e)
	{
		/* Writing an input/alt-func pin is a caller bug; fail instead of silently succeeding */
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(state_e);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_writePin(ch_index_u8, state_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(state_e);

#else

		COM_HDR_UNUSED(state_e);
		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return ret_u8;
}

/**
 * @brief Read the logical state of a channel.
 *
 * @param ch_index_u8 Channel index into the injected table.
 * @return @c GPIO_STATE_HIGH / @c GPIO_STATE_LOW, or @c GPIO_STATE_ERROR on any validation
 * 		   failure.
 *
 * @note Outputs report the commanded output latch (a shorted pin cannot feed a wrong level
 * 		 back into control logic); inputs report the sampled pin level. Polarity is applied
 * 		 to both.
 */
gpio_pin_state_te rr_gpio_readPin(U8 ch_index_u8)
{
	gpio_pin_state_te state_e = GPIO_STATE_ERROR;

	if(COM_HDR_RET_OK == rr_gpio_check_channel(ch_index_u8))
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(ch_index_u8);

#elif defined(NXP_S32K144_146)

		state_e = rr_gpio_nxp_readPin(ch_index_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(ch_index_u8);

#else

		COM_HDR_UNUSED(ch_index_u8);

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return state_e;
}

/**
 * @brief Toggle an output channel; polarity-symmetric, no level mapping involved.
 *
 * @param ch_index_u8 Channel index into the injected table; must be @c GPIO_DIR_OUTPUT.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on validation failure or a toggle of a
 * 		   non-output channel.
 */
U8 rr_gpio_togglePin(U8 ch_index_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if(GPIO_DIR_OUTPUT != gpio_channel_cfg_pst[ch_index_u8].dir_e)
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(ch_index_u8);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_togglePin(ch_index_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(ch_index_u8);

#else

		COM_HDR_UNUSED(ch_index_u8);
		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return ret_u8;
}

/**
 * @brief Arm the pin-level interrupt for a channel using the edge declared in the channel
 * 		  table (@c irq_edge_e); the middleware callback registered at @ref rr_gpio_initialize
 * 		  is used for dispatch.
 *
 * @param ch_index_u8 Channel index into the injected table; its @c irq_edge_e must not be
 * 		  @c GPIO_IRQ_NONE.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on any validation failure.
 *
 * @note Pin-level only: the NVIC side (vector install, priority, line enable) is owned by
 * 		 rr_nvic (Phase 2). Until rr_nvic exists, the project's startup code must route the
 * 		 port vector to @ref rr_gpio_irqHandler and enable the line itself. Returns
 * 		 @c COM_HDR_RET_ERR if the global callback has not been registered (NULL).
 */
U8 rr_gpio_enableInterrupt(U8 ch_index_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if((GPIO_IRQ_NONE == gpio_channel_cfg_pst[ch_index_u8].irq_edge_e) ||
		(gpio_channel_cfg_pst[ch_index_u8].irq_edge_e >= GPIO_IRQ_EDGE_MAX))
	{
		/* The table is the single source of edge truth; a pin not declared as interrupt cannot be armed */
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if(COM_HDR_NULL_P == gpio_irq_cb_pf)
	{
		/* Never arm an edge with no callback registered - would produce unhandled IRQ re-entry */
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(ch_index_u8);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_enableInterrupt(ch_index_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(ch_index_u8);

#else

		COM_HDR_UNUSED(ch_index_u8);
		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return ret_u8;
}

/**
 * @brief Disarm a channel's pin-level interrupt and drop any pending flag.
 *
 * @param ch_index_u8 Channel index into the injected table.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR on any validation failure.
 *
 * @note The NVIC line itself stays untouched (other channels may share it); line-level
 * 		 bookkeeping is rr_nvic's job. The global middleware callback is not cleared here;
 * 		 use @ref rr_gpio_deinit to tear down the whole driver.
 */
U8 rr_gpio_disableInterrupt(U8 ch_index_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if(COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(ch_index_u8);

#elif defined(NXP_S32K144_146)

		ret_u8 = rr_gpio_nxp_disableInterrupt(ch_index_u8);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(ch_index_u8);

#else

		COM_HDR_UNUSED(ch_index_u8);
		ret_u8 = COM_HDR_RET_ERR;

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}

	return ret_u8;
}

/**
 * @brief Port-level interrupt service body and the rr_nvic integration seam: decodes the
 * 		  port's pending pin flags, clears them and dispatches the single middleware callback.
 *
 * @param port_e Logical port whose vector fired.
 *
 * @note Call from the port's ISR (e.g. PORTC_IRQHandler on NXP) - wired by rr_nvic in
 * 		 Phase 2, by the project's startup code until then. Each flag is cleared before the
 * 		 callback runs so a new edge during the callback latches a new flag. Flags on pins
 * 		 outside the channel table are cleared too, otherwise the line would re-enter forever.
 * 		 The middleware callback is snapshotted once at handler entry; flags are cleared even
 * 		 when the callback is NULL to prevent IRQ re-entry.
 */
void rr_gpio_irqHandler(gpio_port_te port_e)
{
	if((COM_HDR_NULL_P != gpio_channel_cfg_pst) && (port_e < GPIO_PORT_MAX))
	{
#if defined(STM32)

		/* @note Reserved for STM32 HAL GPIO implementation */
		COM_HDR_UNUSED(port_e);

#elif defined(NXP_S32K144_146)

		rr_gpio_nxp_irqHandler(port_e);

#elif defined(RENESAS)

		/* @note Reserved for Renesas RA implementation */
		COM_HDR_UNUSED(port_e);

#else

		COM_HDR_UNUSED(port_e);

#endif /* STM32 / NXP_S32K144_146 / RENESAS */
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 HAL GPIO implementation */

#elif defined(NXP_S32K144_146)

/* @note NXP implementation lives in the bsp/nxp port (rr_gpio_nxp.c); rr_gpio_nxp_check_pinmux there
 * 		 additionally cross-validates each channel against the generated pin_mux table at init. */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA GPIO implementation */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/**
 * @brief Validate driver state and one channel-table entry before any hardware access.
 *
 * @param ch_index_u8 Channel index into the injected table.
 * @return @c COM_HDR_RET_OK if the driver is initialized and the entry is usable.
 */
U8 rr_gpio_check_channel(U8 ch_index_u8)
{
	U8 ret_u8 = COM_HDR_RET_OK;

	if((COM_HDR_NULL_P == gpio_channel_cfg_pst) || (ch_index_u8 >= gpio_channel_count_u8))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else if((gpio_channel_cfg_pst[ch_index_u8].port_e >= GPIO_PORT_MAX) ||
		(gpio_channel_cfg_pst[ch_index_u8].pin_u8 >= GPIO_PINS_PER_PORT) ||
		(gpio_channel_cfg_pst[ch_index_u8].dir_e >= GPIO_DIR_MAX) ||
		(gpio_channel_cfg_pst[ch_index_u8].polarity_e >= GPIO_POLARITY_MAX))
	{
		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		/* Entry valid */
	}

	return ret_u8;
}

/**
 * @brief Polarity mapping between logical state and physical pin level.
 *
 * @note The mapping is a symmetric inversion: the same XOR converts logical->physical on
 * 		 writes and physical->logical on reads.
 *
 * @param level_u8 Input level/state (0 or 1).
 * @param polarity_e Channel polarity from the injected table.
 * @return Mapped level/state (0 or 1).
 */
U8 rr_gpio_map_level(U8 level_u8, gpio_polarity_te polarity_e)
{
	return (U8)(level_u8 ^ ((GPIO_ACTIVE_LOW == polarity_e) ? 1U : 0U));
}

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
