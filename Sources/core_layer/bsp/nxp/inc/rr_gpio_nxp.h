/**
 * @file rr_gpio_nxp.h
 * @author vishalagarwal_rideri
 * @brief NXP target port of the ECU Abstraction GPIO driver (PINS_DRV_* pin access, generated pin_mux
 * 		  electrical config).
 * @date 02-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note Internal port header: the controller-specific realisation of the rr_gpio hardware path. Included only
 * 		 by the rr_gpio wrapper's NXP arm and by rr_gpio_nxp.c. The whole body folds to empty on non-NXP
 * 		 targets, so it may be included unconditionally. The public GPIO API, its argument/channel validation
 * 		 and the injected channel table/callback storage live in rr_gpio.h / rr_gpio.c; this header exposes
 * 		 the vendor-backed helpers the wrapper dispatches to, the NXP pin-count macro the wrapper's channel
 * 		 validation consumes, the extern view of the wrapper-owned table/count/callback slots the port uses
 * 		 across the seam, and the wrapper's generic seam helpers the port calls.
 */

#ifndef CORE_LAYER_BSP_NXP_INC_RR_GPIO_NXP_H_
#define CORE_LAYER_BSP_NXP_INC_RR_GPIO_NXP_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#include "core_layer/drivers/inc/rr_gpio.h"	/* Wrapper's public types: gpio_channel_cfg_tst, gpio_port_te,
						   gpio_pin_state_te, gpio_polarity_te, rr_gpio_irq_cb_t */

#if defined(NXP_S32K144_146)

/* Public Macros -----------------------------------------------------------------------------------------------*/
#define GPIO_PINS_PER_PORT	(32U)	/*!< PTA..PTE are 32-pin ports; consumed by the wrapper's channel validation */

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/
/** @brief Wrapper-owned channel table injected by the configuration layer (defined in rr_gpio.c);
 *  	   @c COM_HDR_NULL_P until @c rr_gpio_initialize stores it. Read by every port helper; reset by the
 *  	   port only when PINS_DRV_Init fails so the driver stays in a safe idle state. */
extern const gpio_channel_cfg_tst* gpio_channel_cfg_pst;

/** @brief Wrapper-owned number of entries in the injected channel table (defined in rr_gpio.c); bounds the
 *  	   port's deinit and IRQ-handler channel walks. */
extern U8 gpio_channel_count_u8;

/** @brief Wrapper-owned middleware callback slot (defined in rr_gpio.c); stored by the port at initialize and
 *  	   snapshotted by the port's IRQ handler in ISR context. */
extern rr_gpio_irq_cb_t volatile gpio_irq_cb_pf;

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note NXP versions of each rr_gpio public function; the wrapper (rr_gpio.c) owns argument/channel validation
 * 		 and dispatches to them from its NXP arm. rr_gpio_nxp_readPin returns the polarity-mapped pin state and
 * 		 rr_gpio_nxp_irqHandler is the void ISR body; every other helper returns @c COM_HDR_RET_OK on success
 * 		 or @c COM_HDR_RET_ERR on failure. rr_gpio_nxp_deinitPin and rr_gpio_nxp_check_pinmux are port-internal
 * 		 to rr_gpio_nxp.c (static, never dispatched by the wrapper). Each function is documented above its
 * 		 definition in rr_gpio_nxp.c.
 */
U8 rr_gpio_nxp_initialize(const gpio_channel_cfg_tst* const cfg_pst, U8 count_u8, rr_gpio_irq_cb_t const cb_pf);
U8 rr_gpio_nxp_deinit(void);

U8 rr_gpio_nxp_writePin(U8 ch_index_u8, gpio_pin_state_te state_e);
gpio_pin_state_te rr_gpio_nxp_readPin(U8 ch_index_u8);
U8 rr_gpio_nxp_togglePin(U8 ch_index_u8);

U8 rr_gpio_nxp_enableInterrupt(U8 ch_index_u8);
U8 rr_gpio_nxp_disableInterrupt(U8 ch_index_u8);
void rr_gpio_nxp_irqHandler(gpio_port_te port_e);

/**
 * @note Wrapper-owned generic seam helpers, defined in rr_gpio.c (external linkage only so this port can
 * 		 validate channels and map logical<->physical levels through the same single implementations).
 * 		 Documented above their definitions in rr_gpio.c.
 */
U8 rr_gpio_check_channel(U8 ch_index_u8);
U8 rr_gpio_map_level(U8 level_u8, gpio_polarity_te polarity_e);

#endif /* NXP_S32K144_146 */

#endif /* CORE_LAYER_BSP_NXP_INC_RR_GPIO_NXP_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
