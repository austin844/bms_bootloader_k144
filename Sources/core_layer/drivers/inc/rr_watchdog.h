/**
 * @file rr_watchdog.h
 * @author vishalagarwal_rideri
 * @brief ECU Abstraction internal watchdog driver interface.
 * @date 01-Jul-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note One platform-abstracted API serves every internal on-chip watchdog instance: initialise,
 * 		 refresh, reconfigure the timeout and gate debug/wait/stop behaviour. The configuration layer
 * 		 injects the full timeout/window/mode/interrupt setup at rr_watchdog_initialize(), the driver owns
 * 		 no board data beyond that struct. There is no banking: the enumerator table exists only to keep the
 * 		 index-not-address convention shared with every other core-layer driver, with one entry per active
 * 		 watchdog peripheral.
 *
 * 		 The early-warning interrupt (fires a fixed number of clocks before the hardware reset) is optional:
 * 		 when enabled, the middleware supplies a callback that rr_watchdog_irqHandler() dispatches. This
 * 		 driver never touches the NVIC; enabling/masking the vector is rr_nvic's job.
 *
 */

#ifndef CORE_LAYER_INC_RR_WATCHDOG_H_
#define CORE_LAYER_INC_RR_WATCHDOG_H_

/* Common Includes -----------------------------------------------------------------------------------------------*/
#include "common_header.h"	/* Base fixed-width types (U8/U32) and COM_HDR_* constants */

/* Core Layer Includes -----------------------------------------------------------------------------------------------*/
#if defined(STM32)

/* @note Reserved for STM32 build support */

#elif defined(NXP_S32K144_146)

/* @note NXP watchdog vendor headers (the WDG PAL) are pulled in only by the bsp/nxp port (rr_watchdog_nxp.c) */

#elif defined(RENESAS)

/* @note Reserved for Renesas build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

/* Configuration Layer Includes -----------------------------------------------------------------------------------------------*/

/* Application Layer Includes -----------------------------------------------------------------------------------------------*/

/* Public Macros -----------------------------------------------------------------------------------------------*/
/** Watchdog instance-enable reservation macros (one per active on-chip watchdog peripheral). */
#ifndef WATCHDOG_INSTANCE_0
#define WATCHDOG_INSTANCE_0	COM_HDR_ENABLED		/*!< Macro to Enable NXP WDOG Instance Code */
#endif

/* Public TypeDefs -----------------------------------------------------------------------------------------------*/
/**
 * @brief Watchdog instance enum alias to refer to a specific watchdog instance from the application.
 *
 * @note NXP index mapping: WATCHDOG1_INST -> WDOG instance 0 (the internal on-chip watchdog).
 */
typedef enum wdog_inst_te_tag
{
#if WATCHDOG_INSTANCE_0
	WATCHDOG1_INST,
#endif
	WDOG_INST_MAX
} wdog_inst_te;

/**
 * @brief Behaviour-gating mode passed to @ref rr_watchdog_setMode.
 *
 * @note @c WDOG_MODE_NONE is a no-op guard: @ref rr_watchdog_setMode returns without touching the
 * 		 hardware when passed this value.
 */
typedef enum wdog_mode_te_tag
{
	WDOG_MODE_DEBUG = 0,	/**< Watchdog behaviour while the core is halted in debug */
	WDOG_MODE_WAIT,			/**< Watchdog behaviour while the core is in wait (idle) */
	WDOG_MODE_STOP,			/**< Watchdog behaviour while the core is in stop (deep sleep) */
	WDOG_MODE_NONE,			/**< No-op: @ref rr_watchdog_setMode leaves the current mode untouched */
	WDOG_MODE_MAX
} wdog_mode_te;

/**
 * @brief Decoded last-reset cause. Retained for reset-source consumers; the reset cause is owned and
 * 		  reported by rr_system (@c rr_system_get_reset_info), not by this driver.
 */
typedef enum board_reset_te_tag
{
	BOARD_RESET_UNKNOWN = 0,	/**< Reset source did not match any decoded cause */
	BOARD_RESET_WATCHDOG,		/**< Reset caused by watchdog timeout */
	BOARD_RESET_POR_PLUS_LVD,	/**< Reset caused by power-on reset plus low-voltage detect */
	BOARD_RESET_SOFTWARE,		/**< Reset caused by a software-requested reset */
	BOARD_RESET_EXTERNAL_PIN,	/**< Reset caused by the external reset pin or debugger */
	BOARD_RESET_MAX
} board_reset_te;

/**
 * @brief Middleware early-warning callback prototype. Registered via @ref wdog_cfg_tst at
 * 		  @ref rr_watchdog_initialize and invoked from @ref rr_watchdog_irqHandler.
 *
 * @param wdog_inst_e Watchdog instance the early-warning interrupt fired on.
 */
typedef void (*rr_watchdog_cb_t)(wdog_inst_te wdog_inst_e);

/**
 * @brief Board configuration injected at @ref rr_watchdog_initialize. The configuration layer owns
 * 		  every timing/mode datum; the driver stores only a const pointer to this struct.
 */
typedef struct wdog_cfg_tst_tag
{
	U32               timeout_ms_u32;		/**< Requested reset timeout in milliseconds */
	U8                window_enable_u8;	/**< COM_HDR_TRUE => enable window mode (refresh only inside the window) */
	U32               window_ms_u32;		/**< Window open time in milliseconds; ignored unless window_enable_u8
											 	 is COM_HDR_TRUE */
	U8                debug_mode_enable_u8;/**< COM_HDR_TRUE => watchdog stays active while the core is halted in debug */
	U8                wait_mode_enable_u8;	/**< COM_HDR_TRUE => watchdog stays active in wait (idle) */
	U8                stop_mode_enable_u8;	/**< COM_HDR_TRUE => watchdog stays active in stop (deep sleep) */
	U8                int_enable_u8;		/**< COM_HDR_TRUE => enable the early-warning interrupt before reset */
	rr_watchdog_cb_t  cb_pf;				/**< Early-warning callback invoked from @ref rr_watchdog_irqHandler;
											 	 COM_HDR_NULL_P if unused */
} wdog_cfg_tst;

/* Public Variable Declaration -----------------------------------------------------------------------------------------------*/
/** @brief Stored configuration pointer per instance (defined in rr_watchdog.c); @c COM_HDR_NULL_P until
 *  	   @ref rr_watchdog_initialize succeeds. Used by the wrapper to gate refresh/reconfigure calls on
 *  	   an initialised instance. */
extern const wdog_cfg_tst* wdog_cfg_pst[WDOG_INST_MAX];

/* Public Function Declarations -----------------------------------------------------------------------------------------------*/
/**
 * @note Every status API below returns @c COM_HDR_RET_OK on success or @c COM_HDR_RET_ERR if any
 * 		 argument/operation check failed (uninitialized, bad instance/argument, or a vendor error).
 * 		 @ref rr_watchdog_refresh has no return (best-effort, time-critical). The early-warning interrupt,
 * 		 when enabled by the injected configuration, is dispatched to the registered callback via
 * 		 @ref rr_watchdog_irqHandler; this driver never calls INT_SYS_/HAL_NVIC_ directly.
 */
U8 rr_watchdog_initialize(wdog_inst_te wdog_inst_e, const wdog_cfg_tst* const cfg_pst);
U8 rr_watchdog_deinit(wdog_inst_te wdog_inst_e);

void rr_watchdog_refresh(wdog_inst_te wdog_inst_e);
U8 rr_watchdog_setTimeout(wdog_inst_te wdog_inst_e, U32 timeout_ms_u32);
U8 rr_watchdog_setMode(wdog_inst_te wdog_inst_e, wdog_mode_te mode_e, U8 enable_u8);

void rr_watchdog_irqHandler(wdog_inst_te wdog_inst_e);

#endif /* CORE_LAYER_INC_RR_WATCHDOG_H_ */

/* EOF -----------------------------------------------------------------------------------------------*/
