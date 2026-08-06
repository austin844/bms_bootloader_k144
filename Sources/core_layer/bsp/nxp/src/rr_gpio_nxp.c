/**
 * @file rr_gpio_nxp.c
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction GPIO driver (PINS_DRV_* pin access, generated pin_mux
 * 		  electrical config).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Controller-specific realisation of the rr_gpio hardware path. Resolves each logical port/pin through
 * 		 index-not-address LUTs and drives PINS_DRV_* for pin state, mux and pin-level interrupt configuration;
 * 		 electrical setup (mux/pull/drive) comes from the generated pin_mux table applied once at
 * 		 rr_gpio_nxp_initialize. The wrapper (rr_gpio.c) owns argument/channel validation, the injected channel
 * 		 table/callback storage and the platform dispatch; this port owns the vendor calls.
 */

/* Common Includes --------------------------------------------------------------------------------------------------------------*/

/* Core Layer Includes ----------------------------------------------------------------------------------------------------------*/
#include "core_layer/bsp/nxp/inc/rr_gpio_nxp.h"	/* This port's helper API, the wrapper's public/seam types and the
				   extern wrapper-owned state */

#if defined(NXP_S32K144_146)

#include "pins_driver.h"	/* PINS_DRV_* pin access; GPIO_Type/PORT_Type bases via device_registers.h */
#include "pin_mux.h"	/* Generated electrical pin config: NUM_OF_CONFIGURED_PINS / g_pin_mux_InitConfigArr */

/* Configuration Layer Includes -------------------------------------------------------------------------------------------------*/

/* Application Layer Includes ---------------------------------------------------------------------------------------------------*/

/* Private Macros ---------------------------------------------------------------------------------------------------------------*/

/* Private TypeDefs -------------------------------------------------------------------------------------------------------------*/

/* Private Variable Declaration -------------------------------------------------------------------------------------------------*/
/** @brief Logical gpio_port_te index -> vendor GPIO_Type base; replaces the legacy per-port switch duplication */
static GPIO_Type* const nxp_gpio_base_apst[GPIO_PORT_MAX] = { PTA, PTB, PTC, PTD, PTE };

/** @brief Logical gpio_port_te index -> vendor PORT_Type base; companion table to nxp_gpio_base_apst */
static PORT_Type* const nxp_port_base_apst[GPIO_PORT_MAX] = { PORTA, PORTB, PORTC, PORTD, PORTE };

/** @brief gpio_irq_edge_te index -> PCR IRQC field; index validated against GPIO_IRQ_EDGE_MAX before use */
static const port_interrupt_config_t nxp_irqc_cfg_ae[GPIO_IRQ_EDGE_MAX] =
{ PORT_DMA_INT_DISABLED, PORT_INT_RISING_EDGE, PORT_INT_FALLING_EDGE, PORT_INT_EITHER_EDGE };

/* Public Variable Declaration --------------------------------------------------------------------------------------------------*/

/* Private Function Declaration -------------------------------------------------------------------------------------------------*/
/* @note Port-internal helpers not dispatched by the wrapper: the two deinit/initialize steps and the
 * 		 single-pin mask helper. Forward-declared here because each is used before its definition below.
 * 		 Every other rr_gpio_nxp_* function is declared in rr_gpio_nxp.h (uniform port signatures). */
static U8 rr_gpio_nxp_deinitPin(U8 ch_index_u8);

static U8 rr_gpio_nxp_check_pinmux(U8 ch_index_u8);

static inline pins_channel_type_t gpio_pin_mask(U8 pin_u8);

/* Public Function Definition ---------------------------------------------------------------------------------------------------*/
/**
 * @brief NXP implementation of rr_gpio_initialize: applies the generated electrical pin config,
 * 		  drives output defaults, and registers the middleware callback.
 *
 * @param cfg_pst Channel table (pointer already stored by the public function).
 * @param count_u8 Number of entries in @p cfg_pst.
 * @param cb_pf Middleware callback; may be @c COM_HDR_NULL_P only when no channel declares an
 * 		  interrupt edge.
 * @return @c COM_HDR_RET_OK on success, @c COM_HDR_RET_ERR if PINS_DRV_Init fails, any entry is
 * 		   invalid, or interrupt channels are present without a callback.
 *
 * @note Generated pin_mux owns the electrical config (mux/pull/direction) - same pattern as
 * 		 rr_can + can_pal. On PINS_DRV_Init failure the injected table and count are reset to
 * 		 their uninitialized values before returning so the driver stays in a safe idle state.
 */
U8 rr_gpio_nxp_initialize(const gpio_channel_cfg_tst* const cfg_pst, U8 count_u8, rr_gpio_irq_cb_t const cb_pf)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 ch_index_u8;
	U8 irq_present_u8 = COM_HDR_FALSE;

	/* Generated pin_mux owns the electrical config (mux/pull/direction) - same pattern as rr_can + can_pal */
	if(STATUS_SUCCESS != PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr))
	{
		gpio_channel_cfg_pst  = COM_HDR_NULL_P;
		gpio_channel_count_u8 = 0U;

		ret_u8 = COM_HDR_RET_ERR;
	}
	else
	{
		for(ch_index_u8 = 0U; ch_index_u8 < count_u8; ch_index_u8++)
		{
			/* Cross-check the channel against the generated pin_mux: a table entry for a pin that
			   PINS_DRV_Init never configured (or configured with the wrong direction/mux) would
			   otherwise pass validation silently. This catches drift between the middleware
			   channel table and the generated electrical config at init.
			   @note The generated array describes the INITIAL electrical config only - this detects
			   drift at init, not any runtime mux/direction reconfiguration done after this point */
			if(COM_HDR_RET_OK != rr_gpio_nxp_check_pinmux(ch_index_u8))
			{
				ret_u8 = COM_HDR_RET_ERR;
			}

			if(GPIO_DIR_OUTPUT == cfg_pst[ch_index_u8].dir_e)
			{
				/* Drive the configured logical default; rr_gpio_nxp_writePin always returns
				   COM_HDR_RET_OK (PINS_DRV set/clear have no failure return), so there is no error to
				   check here */
				(void)rr_gpio_nxp_writePin(ch_index_u8, cfg_pst[ch_index_u8].default_state_e);
			}
			else if(COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8))
			{
				ret_u8 = COM_HDR_RET_ERR;
			}
			else
			{
				/* Inputs and alt-func pins need no default drive */
			}

			if(GPIO_IRQ_NONE != cfg_pst[ch_index_u8].irq_edge_e)
			{
				irq_present_u8 = COM_HDR_TRUE;
			}
		}

		/* Interrupt pins configured but no callback supplied: fail - a NULL cb with live edges is
		   a caller bug; a NULL cb is accepted only when no channel declares an interrupt edge */
		if((COM_HDR_TRUE == irq_present_u8) && (COM_HDR_NULL_P == cb_pf))
		{
			ret_u8 = COM_HDR_RET_ERR;
		}

		/* Store unconditionally; enableInterrupt guards against NULL before arming */
		gpio_irq_cb_pf = cb_pf;
	}

	return ret_u8;
}

/**
 * @brief NXP implementation of rr_gpio_deinit: iterates the channel table and returns every
 * 		  channel to its reset state (interrupt disarmed, pin mux disabled).
 *
 * @return @c COM_HDR_RET_OK if every channel deinitialized, @c COM_HDR_RET_ERR if any channel
 * 		   failed validation.
 *
 * @note Each channel is validated, its pin interrupt disarmed first so a pending edge cannot
 * 		 dispatch into a dead channel, then the mux is disabled via rr_gpio_nxp_deinitPin. One
 * 		 bad entry does not strand the rest. The public rr_gpio_deinit clears the table
 * 		 pointer, count, and callback after this function returns regardless of the result.
 */
U8 rr_gpio_nxp_deinit(void)
{
	U8 ret_u8 = COM_HDR_RET_OK;
	U8 ch_index_u8;

	for(ch_index_u8 = 0U; ch_index_u8 < gpio_channel_count_u8; ch_index_u8++)
	{
		if(COM_HDR_RET_OK != rr_gpio_check_channel(ch_index_u8))
		{
			ret_u8 = COM_HDR_RET_ERR;
		}
		else
		{
			/* Disarm the pin interrupt first so a pending edge cannot dispatch into a dead channel */
			(void)rr_gpio_disableInterrupt(ch_index_u8);

			(void)rr_gpio_nxp_deinitPin(ch_index_u8);
		}
	}

	return ret_u8;
}

/**
 * @brief NXP implementation of rr_gpio_writePin: maps the logical state through polarity and
 * 		  drives the physical pin via PINS_DRV_SetPins / PINS_DRV_ClearPins.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated by the public
 * 		  function).
 * @param state_e Logical state to drive (@c GPIO_STATE_LOW or @c GPIO_STATE_HIGH).
 * @return @c COM_HDR_RET_OK always (PINS_DRV set/clear have no failure return).
 *
 * @note Active-low channels: @c GPIO_STATE_HIGH drives PDOR low (pin de-asserted at the pad)
 * 		 and vice versa. The XOR in rr_gpio_map_level handles the inversion transparently.
 */
U8 rr_gpio_nxp_writePin(U8 ch_index_u8, gpio_pin_state_te state_e)
{
	const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];

	if(0U != rr_gpio_map_level((U8)state_e, channel_pst->polarity_e))
	{
		PINS_DRV_SetPins(nxp_gpio_base_apst[channel_pst->port_e], gpio_pin_mask(channel_pst->pin_u8));
	}
	else
	{
		PINS_DRV_ClearPins(nxp_gpio_base_apst[channel_pst->port_e], gpio_pin_mask(channel_pst->pin_u8));
	}

	return COM_HDR_RET_OK;
}

/**
 * @brief NXP implementation of rr_gpio_readPin: samples the output latch (PDOR) for output
 * 		  channels and the pin input register (PDIR) for input channels, then applies polarity.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated by the public
 * 		  function).
 * @return @c GPIO_STATE_HIGH or @c GPIO_STATE_LOW after polarity mapping.
 *
 * @note Outputs report the commanded latch (PDOR), not the pin voltage (PDIR) - a shorted
 * 		 pin must not feed a wrong level back into control logic (legacy read PDIR for both).
 */
gpio_pin_state_te rr_gpio_nxp_readPin(U8 ch_index_u8)
{
	const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];
	pins_channel_type_t port_pins_u32;
	U8 pin_level_u8;

	if(GPIO_DIR_OUTPUT == channel_pst->dir_e)
	{
		/* Outputs report the commanded latch (PDOR), not the pin voltage (PDIR) - a shorted
		   pin must not feed a wrong level back into control logic (legacy read PDIR for both) */
		port_pins_u32 = PINS_DRV_GetPinsOutput(nxp_gpio_base_apst[channel_pst->port_e]);
	}
	else
	{
		port_pins_u32 = PINS_DRV_ReadPins(nxp_gpio_base_apst[channel_pst->port_e]);
	}

	pin_level_u8 = (U8)((port_pins_u32 >> channel_pst->pin_u8) & 1U);

	return (0U != rr_gpio_map_level(pin_level_u8, channel_pst->polarity_e)) ? GPIO_STATE_HIGH : GPIO_STATE_LOW;
}

/**
 * @brief NXP implementation of rr_gpio_togglePin: flips the output latch via
 * 		  PINS_DRV_TogglePins.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated by the public
 * 		  function).
 * @return @c COM_HDR_RET_OK always (PINS_DRV_TogglePins has no failure return).
 *
 * @note Toggling flips the physical latch; the operation is polarity-symmetric, no mapping
 * 		 needed.
 */
U8 rr_gpio_nxp_togglePin(U8 ch_index_u8)
{
	/* Toggling flips the physical latch; the operation is polarity-symmetric, no mapping needed */
	PINS_DRV_TogglePins(nxp_gpio_base_apst[gpio_channel_cfg_pst[ch_index_u8].port_e],
			    gpio_pin_mask(gpio_channel_cfg_pst[ch_index_u8].pin_u8));

	return COM_HDR_RET_OK;
}

/**
 * @brief NXP implementation of rr_gpio_enableInterrupt: clears any stale edge flag then
 * 		  configures the PCR IRQC field for the declared edge.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated, edge checked,
 * 		  and callback confirmed non-NULL by the public function).
 * @return @c COM_HDR_RET_OK always (PINS_DRV interrupt config calls have no failure return).
 *
 * @note Controller-level enable (vector, priority, NVIC line) is rr_nvic's job - never done
 * 		 here. The stale-flag clear prevents a latched edge from firing immediately after arming.
 */
U8 rr_gpio_nxp_enableInterrupt(U8 ch_index_u8)
{
	const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];

	/* Drop a stale edge latched while disarmed */
	PINS_DRV_ClearPinIntFlagCmd(nxp_port_base_apst[channel_pst->port_e], (U32)channel_pst->pin_u8);

	/* Select the PCR IRQC edge. Controller-level enable (vector, priority, NVIC line) is
	   rr_nvic's job - never done here */
	PINS_DRV_SetPinIntSel(nxp_port_base_apst[channel_pst->port_e], (U32)channel_pst->pin_u8,
			      nxp_irqc_cfg_ae[channel_pst->irq_edge_e]);

	return COM_HDR_RET_OK;
}

/**
 * @brief NXP implementation of rr_gpio_disableInterrupt: sets IRQC to disabled then clears
 * 		  any flag that latched between the disarm and the clear.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated by the public
 * 		  function).
 * @return @c COM_HDR_RET_OK always (PINS_DRV interrupt config calls have no failure return).
 *
 * @note The NVIC line itself stays untouched; line-level bookkeeping is rr_nvic's job.
 */
U8 rr_gpio_nxp_disableInterrupt(U8 ch_index_u8)
{
	const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];

	/* Disarm the edge first */
	PINS_DRV_SetPinIntSel(nxp_port_base_apst[channel_pst->port_e], (U32)channel_pst->pin_u8,
			      PORT_DMA_INT_DISABLED);

	/* Then drop a flag that may have latched in between */
	PINS_DRV_ClearPinIntFlagCmd(nxp_port_base_apst[channel_pst->port_e], (U32)channel_pst->pin_u8);

	return COM_HDR_RET_OK;
}

/**
 * @brief NXP implementation of rr_gpio_irqHandler: walks the channel table for the given
 * 		  port, clears each matched flag before dispatch, and calls the middleware callback.
 *
 * @param port_e Logical port whose vector fired (already validated by the public function).
 *
 * @note The callback is snapshotted once at handler entry to guard against a concurrent
 * 		 deinit from a higher-priority context (TOCTOU). Each flag is cleared before the
 * 		 callback so a new edge during the callback latches a fresh flag. Flags on pins
 * 		 outside the channel table are bulk-cleared to prevent IRQ re-entry. Flags are
 * 		 cleared even when the callback snapshot is NULL.
 */
void rr_gpio_nxp_irqHandler(gpio_port_te port_e)
{
	/* Single volatile read guards against a concurrent deinit from a higher-priority context */
	rr_gpio_irq_cb_t const cb_pf = gpio_irq_cb_pf;

	U8 ch_index_u8;
	U32 port_flags_u32 = PINS_DRV_GetPortIntFlag(nxp_port_base_apst[port_e]);

	for(ch_index_u8 = 0U; ch_index_u8 < gpio_channel_count_u8; ch_index_u8++)
	{
		/* Stop once every latched flag has been consumed (was the loop's second clause) */
		if(0U == port_flags_u32)
		{
			break;
		}

		const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];

		if((port_e == channel_pst->port_e) && (0U != (port_flags_u32 & gpio_pin_mask(channel_pst->pin_u8))))
		{
			/* Clear before dispatch: a new edge during the callback must latch a new flag.
			   Flags are cleared even when cb is NULL to prevent IRQ re-entry */
			PINS_DRV_ClearPinIntFlagCmd(nxp_port_base_apst[port_e], (U32)channel_pst->pin_u8);

			port_flags_u32 &= ~gpio_pin_mask(channel_pst->pin_u8);

			if(COM_HDR_NULL_P != cb_pf)
			{
				cb_pf(ch_index_u8, rr_gpio_readPin(ch_index_u8));
			}
		}
	}

	if(0U != port_flags_u32)
	{
		/* Flags on pins outside the channel table: clear them or the line re-enters forever */
		PINS_DRV_ClearPortIntFlagCmd(nxp_port_base_apst[port_e]);
	}
}

/* Private Function Definition --------------------------------------------------------------------------------------------------*/
/* @note rr_gpio_check_channel / rr_gpio_map_level (the shared seam helpers this port calls) are defined in
 * 		 rr_gpio.c and declared extern in rr_gpio_nxp.h. */

/**
 * @brief Return one NXP pin to its reset state: sets the pin mux to disabled (analog) so the
 * 		  pin consumes no digital power and drives no signal.
 *
 * @param ch_index_u8 Channel index into the injected table (already validated by the caller).
 * @return @c COM_HDR_RET_OK always (PINS_DRV_SetMuxModeSel has no failure return).
 *
 * @note rr_gpio_nxp_deinit disarms the pin interrupt before calling here so a pending edge
 * 		 cannot dispatch into a dead channel.
 */
static U8 rr_gpio_nxp_deinitPin(U8 ch_index_u8)
{
	/* Back to the reset state: mux disabled, pin treated as analog */
	PINS_DRV_SetMuxModeSel(nxp_port_base_apst[gpio_channel_cfg_pst[ch_index_u8].port_e],
			       (U32)gpio_channel_cfg_pst[ch_index_u8].pin_u8, PORT_PIN_DISABLED);

	return COM_HDR_RET_OK;
}

/**
 * @brief Cross-validate one channel against the generated pin_mux config: confirm the pin was
 * 		  configured by PINS_DRV_Init and that its direction and mux agree with the channel's
 * 		  declared direction.
 *
 * @param ch_index_u8 Channel index into the injected table.
 * @return @c COM_HDR_RET_OK if the channel agrees with the generated config, @c COM_HDR_RET_ERR if
 * 		   the entry is invalid, the pin is absent from the generated array, or its direction or
 * 		   mux disagrees with the channel.
 *
 * @note Maps the channel's logical port to its PORT_Type base through the existing
 * 		 nxp_port_base_apst lookup table, then linear-searches g_pin_mux_InitConfigArr for an
 * 		 entry matching that base and pin number. The generated array describes the INITIAL
 * 		 electrical config only: this catches drift between the channel table and the generated
 * 		 pin_mux at init, not any runtime reconfiguration of mux or direction done afterwards.
 */
static U8 rr_gpio_nxp_check_pinmux(U8 ch_index_u8)
{
	U8 ret_u8 = COM_HDR_RET_ERR;
	U16 cfg_index_u16;

	if(COM_HDR_RET_OK == rr_gpio_check_channel(ch_index_u8))
	{
		const gpio_channel_cfg_tst* const channel_pst = &gpio_channel_cfg_pst[ch_index_u8];

		/* Reuses the existing logical-port to PORT base lookup table, never a second mapping */
		const PORT_Type* const port_base_pst = nxp_port_base_apst[channel_pst->port_e];

		/* (a) Existence: find the generated entry for this port base and pin number */
		for(cfg_index_u16 = 0U; cfg_index_u16 < (U16)NUM_OF_CONFIGURED_PINS; cfg_index_u16++)
		{
			const pin_settings_config_t* const pin_cfg_pst = &g_pin_mux_InitConfigArr[cfg_index_u16];

			if((port_base_pst == pin_cfg_pst->base) &&
			   ((U32)channel_pst->pin_u8 == pin_cfg_pst->pinPortIdx))
			{
				ret_u8 = COM_HDR_RET_OK;

				/* (b) Direction agreement (alt-func owns no fixed GPIO direction, so skip it) */
				if(GPIO_DIR_OUTPUT == channel_pst->dir_e)
				{
					if(GPIO_OUTPUT_DIRECTION != pin_cfg_pst->direction)
					{
						ret_u8 = COM_HDR_RET_ERR;
					}
				}
				else if(GPIO_DIR_INPUT == channel_pst->dir_e)
				{
					if(GPIO_INPUT_DIRECTION != pin_cfg_pst->direction)
					{
						ret_u8 = COM_HDR_RET_ERR;
					}
				}
				else
				{
					/* Alt-func direction - the peripheral owns the direction, so no check is done */
				}

				/* (c) Mux agreement: GPIO directions need PORT_MUX_AS_GPIO, alt-func needs a non-GPIO mux */
				if(GPIO_DIR_ALTFUNC == channel_pst->dir_e)
				{
					if(PORT_MUX_AS_GPIO == pin_cfg_pst->mux)
					{
						ret_u8 = COM_HDR_RET_ERR;
					}
				}
				else if(PORT_MUX_AS_GPIO != pin_cfg_pst->mux)
				{
					ret_u8 = COM_HDR_RET_ERR;
				}
				else
				{
					/* GPIO direction paired with a GPIO mux, so the mux agrees */
				}

				break;
			}
			else
			{
				/* Not this entry; keep searching */
			}
		}
	}

	return ret_u8;
}

/**
 * @brief Single-pin mask for the PINS_DRV set/clear/toggle/flag calls.
 *
 * @param pin_u8 Pin number within the port.
 * @return Mask with only the bit for @p pin_u8 set.
 *
 * @note Replaces the former GPIO_PIN_MASK function-like macro (MISRA C:2012 Dir 4.9); behaviour is identical.
 */
static inline pins_channel_type_t gpio_pin_mask(U8 pin_u8)
{
	return (pins_channel_type_t)(1UL << pin_u8);
}

#endif /* NXP_S32K144_146 */

/* EOF --------------------------------------------------------------------------------------------------------------------------*/
