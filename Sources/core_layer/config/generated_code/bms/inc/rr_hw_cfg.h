/**
 * @file rr_hw_cfg.h
 * @author vishalagarwal_rideri
 * @brief Consolidated build-time peripheral configuration for the BMS product.
 * @date 06-Aug-2026
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026
 *
 * @note GENERATED FILE - DO NOT EDIT BY HAND. Produced by core_layer/config/tools/gen_config.py
 *       from core_layer/config/variants/bms.xml. Edit the XML and re-run the generator instead.
 * @note Generation hash (bms.xml): b304fbd6cefcb14d. A mismatch on re-run signals a stale tree.
 */

#ifndef CORE_LAYER_CONFIG_RR_HW_CFG_H_
#define CORE_LAYER_CONFIG_RR_HW_CFG_H_

/* Public Macros -----------------------------------------------------------------------------------------------*/

/* --- Product --- */

#define RR_PRODUCT_BMS	(1U)	/*!< Selected product build */
#define RR_PRODUCT_NAME	"bms"	/*!< Product name string */

/* --- Clock --- */

#define RR_CLOCK_SRC_HZ		(8000000U)	/*!< System oscillator source clock, Hz */
#define RR_CLOCK_FIRC_HZ	(48000000U)	/*!< Fast IRC clock, Hz */
#define RR_CLOCK_DIVCORE	(1U)		/*!< Core clock divider */
#define RR_CLOCK_DIVBUS		(2U)		/*!< Bus clock divider */
#define RR_CLOCK_DIVSLOW	(2U)		/*!< Slow (flash) clock divider */

/* --- SPI --- */

#define RR_SPI_PRESENT		(0U)		/*!< 1 if the SPI peripheral is used on this product */
#define RR_SPI_SRC_CLK_HZ	(8000000U)	/*!< LPSPI functional source clock, Hz */
#define RR_SPI_TIMEOUT_MS	(100U)		/*!< Blocking transfer timeout, ms */
#define RR_SPI_INSTANCE_COUNT	(0U)		/*!< Number of enabled LPSPI instances */


/* --- I2C --- */

#define RR_I2C_PRESENT		(0U)		/*!< 1 if the I2C peripheral is used on this product */
#define RR_I2C_TIMEOUT_MS	(50U)		/*!< Blocking transfer timeout, ms */
#define RR_I2C_BAUD_HZ		(100000U)	/*!< LPI2C bus baud rate, bits/s */

/* --- ADC --- */

#define RR_ADC_PRESENT			(0U)	/*!< 1 if the ADC peripheral is used on this product */
#define RR_ADC_RESOLUTION_BITS		(12U)	/*!< Converter resolution, bits (bsp maps to SDK enum) */
#define RR_ADC_CLOCK_DIVIDE		(1U)	/*!< ADC clock divider (bsp maps to SDK enum) */
#define RR_ADC_MAX_CHANNELS		(16U)	/*!< Hardware channel ceiling */
#define RR_ADC_AVG_DEPTH		(10U)	/*!< Software averaging depth per channel */
#define RR_ADC_GROUP_COUNT		(0U)	/*!< Number of conversion groups */
#define RR_ADC_GROUP_MAX_CHANNELS	(0U)	/*!< Largest group's channel count */

/* @note Voltage reference token; bsp maps to the SDK adc_voltage_reference_t value. */
#define RR_ADC_VOLTAGE_REF_VREF		(1U)	/*!< Selected ADC voltage reference */


/* --- Timer --- */

#define RR_TIMER_PRESENT		(1U)			/*!< 1 if the FTM timer is used on this product */
#define RR_TIMER_CHANNEL_MAX		(8U)			/*!< Output-compare channels per timer instance */
#define RR_TIMER_INSTANCE_COUNT		(1U)			/*!< Number of enabled FTM instances */

#define RR_TIMER_INST0_HW		(0U)			/*!< Instance 0 FTM hardware index */
#define RR_TIMER_INST0_CHANNELS		(1U)			/*!< Instance 0 active channel count */
#define RR_TIMER_INST0_ONESHOT		(0U)			/*!< Instance 0 1=oneshot 0=continuous */
#define RR_TIMER_INST0_CLK_SRC		(TIMER_CLK_SRC_SYSTEM)	/*!< Instance 0 counter clock source token */
#define RR_TIMER_INST0_PRESCALER	(TIMER_PRESCALE_1)	/*!< Instance 0 counter clock prescaler token */

/* --- CAN --- */

#define RR_CAN_PRESENT		(1U)	/*!< 1 if FlexCAN is used on this product */
#define RR_CAN_INSTANCE_COUNT	(1U)	/*!< Number of enabled CAN instances */

#define RR_CAN0_PROP_SEG	(7U)	/*!< CAN0 propagation segment */
#define RR_CAN0_PSEG1		(4U)	/*!< CAN0 phase segment 1 */
#define RR_CAN0_PSEG2		(1U)	/*!< CAN0 phase segment 2 */
#define RR_CAN0_PRE_DIVIDER	(5U)	/*!< CAN0 clock pre-divider */
#define RR_CAN0_RJW		(1U)	/*!< CAN0 resync jump width */

/* --- Watchdog --- */

#define RR_WDOG_PRESENT			(1U)	/*!< 1 if a watchdog is used on this product */
#define RR_WDOG_INTERNAL_TIMEOUT	(5000U)	/*!< Internal WDOG timeout, ticks */
#define RR_WDOG_WINDOW_PERCENT		(50U)	/*!< Windowed-refresh opening, % of timeout */
#define RR_WDOG_EWM_PRESENT		(0U)	/*!< 1 if the external EWM watchdog is used */
#define RR_WDOG_EWM_TIMEOUT		(0U)	/*!< External EWM timeout window */

/* --- CRC --- */

#define RR_CRC_PRESENT		(1U)		/*!< 1 if any CRC engine is used on this product */
#define RR_CRC_HW_PRESENT	(1U)		/*!< 1 if the hardware CRC engine is used */
#define RR_CRC_HW_WIDTH		(16U)		/*!< HW CRC width, bits */
#define RR_CRC_HW_POLY		(0x1021U)	/*!< HW CRC polynomial */
#define RR_CRC_HW_SEED		(0xFFFFU)	/*!< HW CRC seed */
#define RR_CRC_SW_PRESENT	(0U)		/*!< 1 if the software CRC engine is used */

/* --- PWM --- */

#define RR_PWM_PRESENT	(0U)	/*!< 1 if the PWM PAL is used on this product */
#define RR_PWM_PERIOD	(0U)	/*!< PWM period, timer ticks */

/* --- RTC --- */

#define RR_RTC_PRESENT	(1U)	/*!< 1 if an RTC is used on this product */
#define RR_RTC_EXTERNAL	(1U)	/*!< 1 if the RTC is an external (SPI) device */

#endif /* CORE_LAYER_CONFIG_RR_HW_CFG_H_ */
