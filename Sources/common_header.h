/**
 * @file common_headers.h
 * @author Prabhu
 * @brief Common base header for every River firmware module: controller platform
 * 		  selection, fixed-width type aliases and the shared COM_HDR_* constant set.
 * @date May 26, 2025
 *
 * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2025
 *
 */

#ifndef COMMON_HEADERS_H_
#define COMMON_HEADERS_H_

/* TODO: To be Removed *******/
#ifndef NXP_S32K144_146

#define NXP_S32K144_146

#endif /* NXP_S32K144_146 */
/****************************/

/* Platform selection: exactly one controller is active at a time. Define STM32 or RENESAS
   from the build system to override; NXP_S32K144_146 is the default when neither is given. */
#if (defined(STM32) && defined(NXP_S32K144_146)) || \
    (defined(STM32) && defined(RENESAS)) || \
    (defined(NXP_S32K144_146) && defined(RENESAS))
#error "Multiple controller platforms selected: define exactly one of STM32 / NXP_S32K144_146 / RENESAS"
#endif

#if defined(STM32)

#include "main.h"

#elif defined(NXP_S32K144_146)

/* @note Reserved for NXP build support */

#elif defined(RENESAS)

/* @note Reserved for Renesas RA build support */

#endif /* STM32 / NXP_S32K144_146 / RENESAS */

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"

/** @{
 * Type definitions: fixed-width aliases used by every River module instead of raw C types;
 * sizes assume the 32-bit ARM targets (NXP S32K / STM32 / Renesas RA) */

typedef signed char         S8;         /* char, signed char 8 bits */
typedef unsigned char       U8;       	/* unsigned char 8 bits */
typedef signed short        S16;      	/* short, signed short 16 bits */
typedef unsigned short      U16;     	/* unsigned short 16 bits */
typedef signed long         S32;       	/* int, signed int, long, signed long 32 bits */
//typedef unsigned long       U32;        /* unsigned int, unsigned long 32 bits */
typedef uint32_t            U32;        /* unsigned int, unsigned long 32 bits */
typedef signed long long    S64;        /* signed long, signed long long 64 bits */
typedef unsigned long long  U64;        /* unsigned long, unsigned long long 64 bits */

typedef float   SFP;             /* 32 bits  IEEE-754 format*/
typedef double  DFP;             /* 64 bits  IEEE-754 format*/

/* MISRA-C Plain char for character storage */
typedef char                CHAR;

/* BIT data type */
typedef unsigned char   BIT;

/* MISRA 6.4 requires bitfields as unsigned */
typedef unsigned int    BITFIELD;

/*legacy type definitions*/
typedef unsigned long   ULONG;
typedef unsigned short  UWORD;
typedef unsigned char   UBYTE;
typedef signed long     SLONG;
typedef signed short    SWORD;
typedef signed char     SBYTE;
typedef unsigned char   BOOL;


/* NOTE Storage Endianness is Little Endian
    so a 32 bit value oc 0x12345678 stored at address 0x100 will be stored as
    Address     0x100   0x101   0x102   0x103
    Data        0x78    0x56    0x34    0x12    */

/** Definitions **/
/** Maximum and minimum representable values for the type aliases above */
#define COM_HDR_MIN_U8  ((U8)(0U))
#define COM_HDR_MAX_U8  ((U8)(0xFFU))                   /* 2^8 */
#define COM_HDR_MIN_U16 ((U16)(0U))
#define COM_HDR_MAX_U16 ((U16)(0xFFFFU))                /* 2^16 */
#define COM_HDR_MIN_U32 ((U32)(0UL))
#define COM_HDR_MAX_U32 ((U32)(0xFFFFFFFFUL))           /* 2^32 */
#define COM_HDR_MAX_U64 ((U64)(0xFFFFFFFFFFFFFFFFULL))  /* 2^64 */
#define COM_HDR_MIN_S8  ((S8)(-128))                    /* -2^7 */
#define COM_HDR_MAX_S8  ((S8)(127))                     /* 2^7 -1 */
#define COM_HDR_MIN_S16 ((S16)(-32768))                 /* -2^15 */
#define COM_HDR_MAX_S16 ((S16)(32767))                  /* 2^15 -1 */
#define COM_HDR_MIN_S32 ((S32)(-2147483648UL))          /* -2^31 */
#define COM_HDR_MAX_S32 ((S32)2147483647L)              /* 2^31 -1 */
#define COM_HDR_MIN_S64 ((S64)(-9223372036854775808LL)) /* -2^63 */
#define COM_HDR_MAX_S64 ((S64)9223372036854775807LL)    /* 2^63 -1 */

/** Typed null-pointer constant used for pointer and callback initialisation/checks */
#define COM_HDR_NULL_P  ((void *)(0))

/** Boolean true/false values (U8-typed). NOTE: rr_* APIs return COM_HDR_RET_OK/ERR, not these. */

#define COM_HDR_FALSE ((U8)(0U))

#ifndef COM_HDR_TRUE
#define COM_HDR_TRUE  ((U8)(1U))
#endif

/** Status returned by every rr_* API: OK is zero, error is non-zero (U8-typed) */
#ifndef COM_HDR_RET_OK
#define COM_HDR_RET_OK  ((U8)(0U))
#endif
#ifndef COM_HDR_RET_ERR
#define COM_HDR_RET_ERR ((U8)(1U))
#endif

/** Physical pin levels for active-high and active-low signal interpretation */
#ifndef COM_HDR_ACTIVE_HIGH_FALSE
#define COM_HDR_ACTIVE_HIGH_FALSE   (0U)
#endif
#ifndef COM_HDR_ACTIVE_HIGH_TRUE
#define COM_HDR_ACTIVE_HIGH_TRUE    (1U)
#endif

#ifndef COM_HDR_ACTIVE_LOW_FALSE
#define COM_HDR_ACTIVE_LOW_FALSE    (1U)
#endif
#ifndef COM_HDR_ACTIVE_LOW_TRUE
#define COM_HDR_ACTIVE_LOW_TRUE     (0U)
#endif

/** Generic ON/OFF states */
#ifndef COM_HDR_OFF
#define COM_HDR_OFF  (0U)
#endif
#ifndef COM_HDR_ON
#define COM_HDR_ON  (1U)
#endif

#define COM_HDR_ZERO_U32                 (0UL)
#define COM_HDR_ZERO_U16 				 (0U)

/** Bit masks: contiguous low-bit masks, byte-lane masks and single-bit masks */
#define COM_HDR_MASK_LO1_BIT             (0x0001U)
#define COM_HDR_MASK_LO2_BITS            (0x0003U)
#define COM_HDR_MASK_LO3_BITS            (0x0007U)
#define COM_HDR_MASK_LO4_BITS            (0x000FU)
#define COM_HDR_MASK_LO5_BITS            (0x001FU)
#define COM_HDR_MASK_LO6_BITS            (0x003FU)
#define COM_HDR_MASK_LO7_BITS            (0x007FU)
#define COM_HDR_MASK_LO8_BITS            (0x00FFU)
#define COM_HDR_MASK_LO9_BITS            (0x01FFU)
#define COM_HDR_MASK_LO10_BITS           (0x03FFU)
#define COM_HDR_MASK_LO11_BITS           (0x07FFU)
#define COM_HDR_MASK_LO12_BITS           (0x0FFFU)
#define COM_HDR_MASK_LO13_BITS           (0x1FFFU)
#define COM_HDR_MASK_LO14_BITS           (0x3FFFU)
#define COM_HDR_MASK_LO15_BITS           (0x7FFFU)
#define COM_HDR_MASK_LO16_BITS           (0xFFFFU)

#define COM_HDR_MASK_LO8_BITS_U32        (0x000000FFUL)
#define COM_HDR_MASK_LO15_8_BITS_U32     (0x0000FF00UL)
#define COM_HDR_MASK_LO16_BITS_U32       (0x0000FFFFUL)
#define COM_HDR_MASK_HI23_16_BITS_U32    (0x00FF0000UL)
#define COM_HDR_MASK_HI31_24_BITS_U32    (0xFF000000UL)
#define COM_HDR_MASK_HI16_BITS_U32       (0xFFFF0000UL)
#define COM_HDR_MASK_BITS_15_8_U32       (0x0000FF00UL)
#define COM_HDR_MASK_BITS_24_16_U32      (0x00FF0000UL)

#define COM_HDR_TEST_BIT_3               (0x04U)
#define COM_HDR_TEST_BIT_7               (0x80U)
#define COM_HDR_MASK_BITS_2_3            (0x000CU)
#define COM_HDR_MASK_BITS_4_5            (0x0030U)
#define COM_HDR_MASK_BITS_4_5_6          (0x0070U)
#define COM_HDR_MASK_BITS_6_7            (0x00C0U)
#define COM_HDR_MASK_BITS_0_3            (0x000FU)
#define COM_HDR_MASK_BITS_0_2            (0x0007U)
#define COM_HDR_MASK_BITS_4_7            (0x00F0U)
#define COM_HDR_MASK_BITS_0_7            (0x00FFU)
#define COM_HDR_SET_BITS_0_7             (0x00FFU)
#define COM_HDR_MASK_BITS_15_8           (0xFF00U)
#define COM_HDR_SET_BITS_15_8            (0xFF00U)
#define COM_HDR_MASK_BITS_15_0           (0xFFFFU)
#define COM_HDR_MASK_BITS_31_0           (0xFFFFFFFFU)
#define COM_HDR_SET_BITS_15_0            (0xFFFFFFFFU)

#define COM_HDR_MASK_BIT_23              (0x00800000U)
#define COM_HDR_MASK_BIT_13				 (0x2000)
#define COM_HDR_MASK_BIT_12				 (0x1000)
#define COM_HDR_MASK_BIT_11				 (0x800U)
#define COM_HDR_MASK_BIT_10				 (0x400U)
#define COM_HDR_MASK_BIT_9				 (0x200U)
#define COM_HDR_MASK_BIT_8               (0x100U)
#define COM_HDR_MASK_BIT_7               (0x80U)
#define COM_HDR_MASK_BIT_6               (0x40U)
#define COM_HDR_MASK_BIT_5               (0x20U)
#define COM_HDR_MASK_BIT_4               (0x10U)
#define COM_HDR_MASK_BIT_3               (0x08U)
#define COM_HDR_MASK_BIT_2               (0x04U)
#define COM_HDR_MASK_BIT_1               (0x02U)
#define COM_HDR_MASK_BIT_0               (0x01U)


/** Bit-shift counts; use these instead of magic shift literals at call sites */
#define COM_HDR_SHIFT_0_BITS             (0U)
#define COM_HDR_SHIFT_1_BITS             (1U)
#define COM_HDR_SHIFT_2_BITS             (2U)
#define COM_HDR_SHIFT_3_BITS             (3U)
#define COM_HDR_SHIFT_4_BITS             (4U)
#define COM_HDR_SHIFT_5_BITS             (5U)
#define COM_HDR_SHIFT_6_BITS             (6U)
#define COM_HDR_SHIFT_7_BITS             (7U)
#define COM_HDR_SHIFT_8_BITS             (8U)
#define COM_HDR_SHIFT_9_BITS			 (9U)
#define COM_HDR_SHIFT_10_BITS            (10U)
#define COM_HDR_SHIFT_11_BITS            (11U)
#define COM_HDR_SHIFT_12_BITS            (12U)
#define COM_HDR_SHIFT_13_BITS            (13U)
#define COM_HDR_SHIFT_14_BITS            (14U)
#define COM_HDR_SHIFT_15_BITS            (15U)
#define COM_HDR_SHIFT_16_BITS            (16U)
#define COM_HDR_SHIFT_21_BITS            (21U)
#define COM_HDR_SHIFT_24_BITS            (24U)
#define COM_HDR_SHIFT_25_BITS            (25U)
#define COM_HDR_SHIFT_26_BITS            (26U)

#define COM_HDR_BITS_PER_U8              (8U)

#define COM_HDR_MAX_100_PCNT_X10         (1000U)

/** Generic decimal scaling factors for fixed-point unit conversions */
#define COM_HDR_SCALE_2                  (2U)
#define COM_HDR_SCALE_10                 (10U)
#define COM_HDR_SCALE_20 				 (20U)
#define COM_HDR_SCALE_100                (100U)
#define COM_HDR_SCALE_1000               (1000U)
#define COM_HDR_SCALE_10000              (10000U)
#define COM_HDR_SCALE_100000             (100000U)
#define COM_HDR_SCALE_1000000            (1000000U)
#define COM_HDR_1S_TO_10MS               (100U)


/** Marks a deliberately unused parameter (e.g. reserved platform arms) without a warning */
#define COM_HDR_UNUSED(x) (void)(x)

/** Compile-time feature switches and build-mode (boot/application) selection */
#define COM_HDR_ENABLED							1U
#define COM_HDR_DISABLED						0U

#define COM_HDR_NULL							0

#define DEBUG_ENABLED							0U
#define APP_OK									0U

#define BOOT_COMPILE							(0U)
#define APP_COMPILE								(!BOOT_COMPILE)

/** @} */
/** @{
 *  Public Variable Definitions */

/** @} */

/* Public Function Prototypes **/

#endif /* COMMON_HEADERS_H_ */
