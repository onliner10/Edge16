/**
 * @file lv_conf.h
 * Configuration file for v9.5.0 (Edge16)
 *
 * Edge16-specific: the #if 1 guard below ensures this file is active
 * when the LVGL v9 vendor branch is used.  The old v8 lv_conf.h was
 * replaced in place; for v8 builds a separate v8-compatible config
 * should be selected via the build system.
 */

/*
 * Copy this file as `lv_conf.h`
 * 1. simply next to `lvgl` folder
 * 2. or to any other place and
 *    - define `LV_CONF_INCLUDE_SIMPLE`;
 *    - add the path as an include path.
 */

/* clang-format off */
#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#if !defined(LV_SKIP_DEFINES)
#include "definitions.h"
#include "hal.h"
#endif

/*====================
   COLOR SETTINGS
 *====================*/

/** Color depth: 1 (I1), 8 (L8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888) */
#define LV_COLOR_DEPTH 16

/* Edge16: RGB565 swap preserved at zero (not needed with LTDC) */
#define LV_COLOR_16_SWAP 0

/* Edge16: screen transparency disabled for embedded display */
#define LV_COLOR_SCREEN_TRANSP 0

/** Adjust color mix functions rounding. GPUs might calculate color mix (blending) differently.
 *  - 0:   round down,
 *  - 64:  round up from x.75,
 *  - 128: round up from half,
 *  - 192: round up from x.25,
 *  - 254: round up */
#define LV_COLOR_MIX_ROUND_OFS  0

/* Edge16: chroma key for transparency */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

/*
 * Edge16 memory strategy:
 *   BOOT       → use standard C malloc/free (LV_STDLIB_CLIB)
 *   Firmware  → use LVGL built-in allocator fed from a statically
 *                reserved SDRAM pool (get_lvgl_mem)
 */
#if defined(BOOT)
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#else
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#endif

#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    /*
     * Size depends on external SDRAM available.
     *   SDRAM_32M → 8 MiB pool
     *   SDRAM_16M → 4 MiB pool
     *   otherwise → 2 MiB pool
     * In the simulator the pool is doubled.
     */
    #if defined(SDRAM_32M)
        #define LV_MEM 8
    #elif defined(SDRAM_16M)
        #define LV_MEM 4
    #else
        #define LV_MEM 2
    #endif
    #if defined(SIMU)
        #define LV_MEM_SIZE (LV_MEM * 2 * 1024U * 1024U)  /*[bytes]*/
    #else
        #define LV_MEM_SIZE (LV_MEM * 1024U * 1024U)      /*[bytes]*/
    #endif

    /** Size of the memory expand for `lv_malloc()` in bytes */
    #define LV_MEM_POOL_EXPAND_SIZE 0

    /** Set an address for the memory pool instead of allocating it as a normal array. Can be in external SRAM too. */
    #define LV_MEM_ADR 0     /**< 0: unused*/
    /* Instead of an address give a memory allocator that will be called to get a memory pool for LVGL. E.g. my_malloc */
    #if LV_MEM_ADR == 0
        extern char* get_lvgl_mem(int);
        #define LV_MEM_POOL_ALLOC get_lvgl_mem
    #endif
#endif  /*LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN*/

/*====================
   HAL SETTINGS
 *====================*/

/** Default display refresh, input device read and animation step period. */
#define LV_DEF_REFR_PERIOD  30          /**< [ms] */

/** Default Dots Per Inch. Used to initialize default sizes such as widgets sized, style paddings.
 * (Not so important, you can adjust it to modify default sizes and spaces.) */
#define LV_DPI_DEF 130                  /**< [px/inch] */

/*=================
 * OPERATING SYSTEM
 *=================*/
/*
 * Edge16: LVGL runs in a single-threaded context on bare metal / FreeRTOS.
 * LVGL's own OS abstraction is not needed.
 */
#define LV_USE_OS   LV_OS_NONE

/*========================
 * RENDERING CONFIGURATION
 *========================*/

/** Align stride of all layers and images to this bytes */
#define LV_DRAW_BUF_STRIDE_ALIGN                1

/** Align start address of draw_buf addresses to this bytes*/
#define LV_DRAW_BUF_ALIGN                       4

/** Using matrix for transformations.
 * Requirements:
 * - `LV_USE_MATRIX = 1`.
 * - Rendering engine needs to support 3x3 matrix transformations. */
#define LV_DRAW_TRANSFORM_USE_MATRIX            0

/* If a widget has `style_opa < 255` (not `bg_opa`, `text_opa` etc) or not NORMAL blend mode
 * it is buffered into a "simple" layer before rendering. The widget can be buffered in smaller chunks.
 * "Transformed layers" (if `transform_angle/zoom` are set) use larger buffers
 * and can't be drawn in chunks. */

/** The target buffer size for simple layer chunks. */
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (24 * 1024)    /**< [bytes]*/

/* Limit the max allocated memory for simple and transformed layers.
 * It should be at least `LV_DRAW_LAYER_SIMPLE_BUF_SIZE` sized but if transformed layers are also used
 * it should be enough to store the largest widget too (width x height x 4 area).
 * Set it to 0 to have no limit. */
#define LV_DRAW_LAYER_MAX_MEMORY 0  /**< No limit by default [bytes]*/

/** Stack size of drawing thread.
 * NOTE: If FreeType or ThorVG is enabled, it is recommended to set it to 32KB or more.
 */
#define LV_DRAW_THREAD_STACK_SIZE    (8 * 1024)         /**< [bytes]*/

/** Thread priority of the drawing task.
 *  Higher values mean higher priority.
 *  Can use values from lv_thread_prio_t enum in lv_os.h: LV_THREAD_PRIO_LOWEST,
 *  LV_THREAD_PRIO_LOW, LV_THREAD_PRIO_MID, LV_THREAD_PRIO_HIGH, LV_THREAD_PRIO_HIGHEST
 *  Make sure the priority value aligns with the OS-specific priority levels.
 *  On systems with limited priority levels (e.g., FreeRTOS), a higher value can improve
 *  rendering performance but might cause other tasks to starve. */
#define LV_DRAW_THREAD_PRIO LV_THREAD_PRIO_HIGH

/*
 * Edge16: software rendering backed by DMA2D.
 * LV_DRAW_SW_COMPLEX is enabled for full UI, disabled in BOOT to save flash.
 */
#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    /*
     * Selectively disable color format support in order to reduce code size.
     * Edge16: 16-bit RGB565 primary, ARGB8888 for opacity/internal, RGB888 for gradients.
     */
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #if defined(LV_COLOR_16_SWAP) && LV_COLOR_16_SWAP
        #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED       1
    #else
        #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED       0
    #endif
    #define LV_DRAW_SW_SUPPORT_RGB565A8     1
    #define LV_DRAW_SW_SUPPORT_RGB888       1
    #define LV_DRAW_SW_SUPPORT_XRGB8888     0
    #define LV_DRAW_SW_SUPPORT_ARGB8888     1
    #define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
    #define LV_DRAW_SW_SUPPORT_L8           0
    #define LV_DRAW_SW_SUPPORT_AL88         0
    #define LV_DRAW_SW_SUPPORT_A8           0
    #define LV_DRAW_SW_SUPPORT_I1           0

    /** Set number of draw units.
     *  - > 1 requires operating system to be enabled in `LV_USE_OS`.
     *  - > 1 means multiple threads will render the screen in parallel. */
    #define LV_DRAW_SW_DRAW_UNIT_CNT    1

    /** Use Arm-2D to accelerate software (sw) rendering. */
    #define LV_USE_DRAW_ARM2D_SYNC      0

    /** Enable native helium assembly to be compiled. */
    #define LV_USE_NATIVE_HELIUM_ASM    0

    #if defined(BOOT)
        #define LV_DRAW_SW_COMPLEX          0
    #else
        #define LV_DRAW_SW_COMPLEX          1
    #endif

    #if LV_DRAW_SW_COMPLEX == 1
        /** Allow buffering some shadow calculation.
         *  LV_DRAW_SW_SHADOW_CACHE_SIZE is the maximum shadow size to buffer, where shadow size is
         *  `shadow_width + radius`.  Caching has LV_DRAW_SW_SHADOW_CACHE_SIZE^2 RAM cost. */
        #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0

        /** Set number of maximally-cached circle data.
         *  The circumference of 1/4 circle are saved for anti-aliasing.
         *  `radius * 4` bytes are used per circle (the most often used radiuses are saved).
         *  - 0: disables caching */
        #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
    #endif

    #define  LV_USE_DRAW_SW_ASM     LV_DRAW_SW_ASM_NONE

    /** Enable drawing complex gradients in software: linear at an angle, radial or conical */
    #define LV_USE_DRAW_SW_COMPLEX_GRADIENTS    0
#endif

/*Use TSi's aka (Think Silicon) NemaGFX */
#define LV_USE_NEMA_GFX 0

#if LV_USE_NEMA_GFX
    #define LV_USE_NEMA_LIB LV_NEMA_LIB_NONE
    #define LV_USE_NEMA_HAL LV_NEMA_HAL_CUSTOM
    #if LV_USE_NEMA_HAL == LV_NEMA_HAL_STM32
        #define LV_NEMA_STM32_HAL_INCLUDE <stm32u5xx_hal.h>
        #define LV_NEMA_STM32_HAL_ATTRIBUTE_POOL_MEM
    #endif
    #define LV_USE_NEMA_VG 0
    #if LV_USE_NEMA_VG
        #define LV_NEMA_GFX_MAX_RESX 800
        #define LV_NEMA_GFX_MAX_RESY 600
    #endif
#endif

/** Use NXP's PXP on iMX RTxxx platforms. */
#define LV_USE_PXP 0
#if LV_USE_PXP
    #define LV_USE_DRAW_PXP 1
    #define LV_USE_ROTATE_PXP 0
    #if LV_USE_DRAW_PXP && LV_USE_OS
        #define LV_USE_PXP_DRAW_THREAD 1
    #endif
    #define LV_USE_PXP_ASSERT 0
#endif

/** Use NXP's G2D on MPU platforms. */
#define LV_USE_G2D 0
#if LV_USE_G2D
    #define LV_USE_DRAW_G2D 1
    #define LV_USE_ROTATE_G2D 0
    #define LV_G2D_HASH_TABLE_SIZE 50
    #if LV_USE_DRAW_G2D && LV_USE_OS
        #define LV_USE_G2D_DRAW_THREAD 1
    #endif
    #define LV_USE_G2D_ASSERT 0
#endif

/** Use Renesas Dave2D on RA  platforms. */
#define LV_USE_DRAW_DAVE2D 0

/** Draw using cached SDL textures*/
#define LV_USE_DRAW_SDL 0

/** Use VG-Lite GPU. */
#define LV_USE_DRAW_VG_LITE 0
#if LV_USE_DRAW_VG_LITE
    #define LV_VG_LITE_USE_GPU_INIT 0
    #define LV_VG_LITE_USE_ASSERT 0
    #define LV_VG_LITE_FLUSH_MAX_COUNT 8
    #define LV_VG_LITE_USE_BOX_SHADOW 1
    #define LV_VG_LITE_GRAD_CACHE_CNT 32
    #define LV_VG_LITE_STROKE_CACHE_CNT 32
    #define LV_VG_LITE_BITMAP_FONT_CACHE_CNT 256
    #define LV_VG_LITE_DISABLE_VLC_OP_CLOSE 0
    #define LV_VG_LITE_DISABLE_BLIT_RECT_OFFSET 0
    #define LV_VG_LITE_DISABLE_LINEAR_GRADIENT_EXT 0
    #define LV_VG_LITE_PATH_DUMP_MAX_LEN 1000
    #define LV_USE_VG_LITE_DRIVER  0
    #if LV_USE_VG_LITE_DRIVER
        #define LV_VG_LITE_HAL_GPU_SERIES gc255
        #define LV_VG_LITE_HAL_GPU_REVISION 0x40
        #define LV_VG_LITE_HAL_GPU_BASE_ADDRESS 0x40240000
    #endif
    #define LV_USE_VG_LITE_THORVG   0
    #if LV_USE_VG_LITE_THORVG
        #define LV_VG_LITE_THORVG_LVGL_BLEND_SUPPORT 0
        #define LV_VG_LITE_THORVG_YUV_SUPPORT 0
        #define LV_VG_LITE_THORVG_LINEAR_GRADIENT_EXT_SUPPORT 0
        #define LV_VG_LITE_THORVG_16PIXELS_ALIGN 1
        #define LV_VG_LITE_THORVG_BUF_ADDR_ALIGN 64
        #define LV_VG_LITE_THORVG_THREAD_RENDER 0
    #endif
#endif

/*
 * Edge16: STM32 DMA2D acceleration.
 * Enabled on hardware (non-SIMU), disabled in simulator.
 * CMSIS include path depends on STM32 variant.
 */
#ifdef SIMU
#define LV_USE_DRAW_DMA2D 0
#else
#define LV_USE_DRAW_DMA2D 1
#endif
#if LV_USE_DRAW_DMA2D
    #if defined(STM32H7)
        #define LV_DRAW_DMA2D_HAL_INCLUDE "stm32h7xx.h"
    #elif defined(STM32H7RS)
        #define LV_DRAW_DMA2D_HAL_INCLUDE "stm32h7rsxx.h"
    #else
        #define LV_DRAW_DMA2D_HAL_INCLUDE "stm32f4xx.h"
    #endif
    /* Edge16: DMA2D is handled synchronously (polling), not via interrupt. */
    #define LV_USE_DRAW_DMA2D_INTERRUPT 0
#endif

/** Draw using cached OpenGLES textures. Requires LV_USE_OPENGLES */
#define LV_USE_DRAW_OPENGLES 0
#if LV_USE_DRAW_OPENGLES
    #define LV_DRAW_OPENGLES_TEXTURE_CACHE_COUNT 64
#endif

/** Draw using espressif PPA accelerator */
#define LV_USE_PPA  0
#if LV_USE_PPA
    #define LV_USE_PPA_IMG      0
    #define LV_PPA_BURST_LENGTH    128
#endif

/* Use EVE FT81X GPU. */
#define LV_USE_DRAW_EVE 0
#if LV_USE_DRAW_EVE
    #define LV_DRAW_EVE_EVE_GENERATION 4
    #define LV_DRAW_EVE_WRITE_BUFFER_SIZE 2048
#endif

/** Use NanoVG Renderer
 * - Requires LV_USE_NANOVG, LV_USE_MATRIX.
 */
#define LV_USE_DRAW_NANOVG 0
#if LV_USE_DRAW_NANOVG
    #define LV_NANOVG_BACKEND   LV_NANOVG_BACKEND_GLES2
    #define LV_NANOVG_IMAGE_CACHE_CNT 128
    #define LV_NANOVG_LETTER_CACHE_CNT 512
#endif

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Logging
 *-----------*/

/** Enable log module */
#define LV_USE_LOG 0
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF 0
    #define LV_LOG_USE_TIMESTAMP 1
    #define LV_LOG_USE_FILE_LINE 1

    #define LV_LOG_TRACE_MEM        1
    #define LV_LOG_TRACE_TIMER      1
    #define LV_LOG_TRACE_INDEV      1
    #define LV_LOG_TRACE_DISP_REFR  1
    #define LV_LOG_TRACE_EVENT      1
    #define LV_LOG_TRACE_OBJ_CREATE 1
    #define LV_LOG_TRACE_LAYOUT     1
    #define LV_LOG_TRACE_ANIM       1
    #define LV_LOG_TRACE_CACHE      1
#endif  /*LV_USE_LOG*/

/*-------------
 * Asserts
 *-----------*/

/* Edge16: enable null and malloc assertions (fast, low cost). */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/** Add a custom handler when assert happens e.g. to restart MCU. */
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);     /**< Halt by default */

/*-------------
 * Debug
 *-----------*/

/** 1: Draw random colored rectangles over the redrawn areas. */
#define LV_USE_REFR_DEBUG 0

/** 1: Draw a red overlay for ARGB layers and a green overlay for RGB layers*/
#define LV_USE_LAYER_DEBUG 0

/** 1: Adds the following behaviors for debugging:
 *  - Draw overlays with different colors for each draw_unit's tasks.
 *  - Draw index number of draw unit on white background.
 *  - For layers, draws index number of draw unit on black background. */
#define LV_USE_PARALLEL_DRAW_DEBUG 0

/*-------------
 * Others
 *-----------*/

#define LV_ENABLE_GLOBAL_CUSTOM 0
#if LV_ENABLE_GLOBAL_CUSTOM
    #define LV_GLOBAL_CUSTOM_INCLUDE <stdint.h>
#endif

/** Default cache size in bytes.
 *  Used by image decoders such as `lv_lodepng` to keep the decoded image in memory.
 *  If size is not set to 0, the decoder will fail to decode when the cache is full.
 *  If size is 0, the cache function is not enabled and the decoded memory will be
 *  released immediately after use. */
#define LV_CACHE_DEF_SIZE       0

/** Default number of image header cache entries. The cache is used to store the headers of images
 *  The main logic is like `LV_CACHE_DEF_SIZE` but for image headers. */
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 0

/** Number of stops allowed per gradient. Increase this to allow more stops.
 *  This adds (sizeof(lv_color_t) + 1) bytes per additional stop. */
#define LV_GRADIENT_MAX_STOPS   2

/** Add 2 x 32-bit variables to each `lv_obj_t` to speed up getting style properties */
#define LV_OBJ_STYLE_CACHE      0

/** Add `id` field to `lv_obj_t` */
#define LV_USE_OBJ_ID           0

/**  Enable support widget names*/
#define LV_USE_OBJ_NAME         0

/** Automatically assign an ID when obj is created */
#define LV_OBJ_ID_AUTO_ASSIGN   LV_USE_OBJ_ID

/** Use builtin obj ID handler functions */
#define LV_USE_OBJ_ID_BUILTIN   1

/** Use obj property set/get API. */
#define LV_USE_OBJ_PROPERTY 0

/** Enable property name support. */
#define LV_USE_OBJ_PROPERTY_NAME 0

/* Enable the multi-touch gesture recognition feature */
/* Gesture recognition requires the use of floats */
#define LV_USE_GESTURE_RECOGNITION 0

/*=====================
 *  COMPILER SETTINGS
 *====================*/

/** For big endian systems set to 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/** Define a custom attribute for `lv_tick_inc` function */
#define LV_ATTRIBUTE_TICK_INC

/** Define a custom attribute for `lv_timer_handler` function */
#define LV_ATTRIBUTE_TIMER_HANDLER

/** Define a custom attribute for `lv_display_flush_ready` function */
#define LV_ATTRIBUTE_FLUSH_READY

/** Align VG_LITE buffers on this number of bytes. */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/** Will be added where memory needs to be aligned (with -Os data might not be aligned to boundary by default).
 *  E.g. __attribute__((aligned(4)))*/
#define LV_ATTRIBUTE_MEM_ALIGN

/** Attribute to mark large constant arrays, for example for font bitmaps */
#define LV_ATTRIBUTE_LARGE_CONST

/** Compiler prefix for a large array declaration in RAM */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/** Place performance critical functions into a faster memory (e.g RAM) */
#define LV_ATTRIBUTE_FAST_MEM

/** Export integer constant to binding. This macro is used with constants in the form of LV_<CONST> that
 *  should also appear on LVGL binding API such as MicroPython. */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/** Prefix all global extern data with this */
#define LV_ATTRIBUTE_EXTERN_DATA

/** Use `float` as `lv_value_precise_t` */
#define LV_USE_FLOAT            0

/** Enable matrix support
 *  - Requires `LV_USE_FLOAT = 1` */
#define LV_USE_MATRIX           0

/** Include `lvgl_private.h` in `lvgl.h` to access internal data and functions by default */
#ifndef LV_USE_PRIVATE_API
    #define LV_USE_PRIVATE_API  1
#endif

/* Edge16: coordinate range stays at int16_t (-32k..32k) */
#define LV_USE_LARGE_COORD 0

/*==================
 *   FONT USAGE
 *===================*/

/* Montserrat fonts with ASCII range and some symbols using bpp = 4
 * https://fonts.google.com/specimen/Montserrat */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_28_COMPRESSED    0  /**< bpp = 3 */
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW    TRANSLATION_IS_RTL  /**< Hebrew, Arabic, Persian letters */
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK   0  /**< 1338 most common CJK radicals */
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK   0  /**< 1338 most common CJK radicals */

/** Pixel perfect monospaced fonts */
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/*
 * Edge16 custom fonts.
 * BOOT → bootloader bitmap font (lv_font_bl).
 * Firmware → per-language font (lv_font_xx_STD).
 * SIMU → English font.
 */
#if !defined(BOOT)

#if !defined(SIMU)

#if defined(TRANSLATIONS_CN)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_cn_STD)
  #define LV_FONT_DEFAULT &lv_font_cn_STD
#elif defined(TRANSLATIONS_TW)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_tw_STD)
  #define LV_FONT_DEFAULT &lv_font_tw_STD
#elif defined(TRANSLATIONS_RU)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_ru_STD)
  #define LV_FONT_DEFAULT &lv_font_ru_STD
#elif defined(TRANSLATIONS_JP)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_jp_STD)
  #define LV_FONT_DEFAULT &lv_font_jp_STD
#elif defined(TRANSLATIONS_KO)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_ko_STD)
  #define LV_FONT_DEFAULT &lv_font_ko_STD
#elif defined(TRANSLATIONS_HE)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_he_STD)
  #define LV_FONT_DEFAULT &lv_font_he_STD
#elif defined(TRANSLATIONS_UA)
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_ua_STD)
  #define LV_FONT_DEFAULT &lv_font_ua_STD
#else
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_en_STD)
  #define LV_FONT_DEFAULT &lv_font_en_STD
#endif

#else
/* Simulator font */
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_en_STD)
#define LV_FONT_DEFAULT &lv_font_en_STD
#endif

#else
/* Bootloader font */
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_bl)
#define LV_FONT_DEFAULT &lv_font_bl
#endif

/** Enable handling large font and/or fonts with a lot of characters.
 *  The limit depends on the font size, font face and bpp.
 *  A compiler error will be triggered if a font needs it. */
#define LV_FONT_FMT_TXT_LARGE 0

/** Enables/disables support for compressed fonts. */
#define LV_USE_FONT_COMPRESSED 0

/** Enable drawing placeholders when glyph dsc is not found. */
#define LV_USE_FONT_PLACEHOLDER 0

/*=================
 *  TEXT SETTINGS
 *=================*/

/**
 * Select a character encoding for strings.
 * Your IDE or editor should have the same character encoding.
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/** While rendering text strings, break (wrap) text on these chars. */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/** If a word is at least this long, will break wherever "prettiest".
 *  To disable, set to a value <= 0. */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/** Minimum number of characters in a long word to put on a line before a break.
 *  Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/** Minimum number of characters in a long word to put on a line after a break.
 *  Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/** Support bidirectional text. Allows mixing Left-to-Right and Right-to-Left text.
 *  The direction will be processed according to the Unicode Bidirectional Algorithm:
 *  https://www.w3.org/International/articles/inline-bidi-markup/uba-basics */
#if defined(SIMU)
#define LV_USE_BIDI 1
#else
#define LV_USE_BIDI TRANSLATION_IS_RTL
#endif
#if LV_USE_BIDI
    #define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#endif

/** Enable Arabic/Persian processing
 *  In these languages characters should be replaced with another form based on their position in the text */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*The control character to use for signaling text recoloring*/
#define LV_TXT_COLOR_CMD "#"

/*==================
 * WIDGETS
 *==================*/

/*
 * Edge16 widget set.
 * In BOOT everything is disabled; in firmware the specific
 * widgets used by the UI are enabled.
 */
#if !defined(BOOT)

#define LV_WIDGETS_HAS_DEFAULT_VALUE  0

#define LV_USE_ANIMIMG    0

#define LV_USE_ARC        1

#define LV_USE_ARCLABEL  0

#define LV_USE_BAR        1

#define LV_USE_BUTTON        1

#define LV_USE_BUTTONMATRIX  1

#define LV_USE_CALENDAR   0

#define LV_USE_CANVAS     1

#define LV_USE_CHART      0

#define LV_USE_CHECKBOX   1

#define LV_USE_DROPDOWN   0

#define LV_USE_IMAGE      1

#define LV_USE_IMAGEBUTTON     0

#define LV_USE_KEYBOARD   1

#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 0
    #define LV_LABEL_LONG_TXT_HINT 1
#endif

#define LV_USE_LED        0

#define LV_USE_LINE       1

#define LV_USE_LIST       0

#define LV_USE_LOTTIE     0

#define LV_USE_MENU       0

#define LV_USE_MSGBOX     0

#define LV_USE_ROLLER     0

#define LV_USE_SCALE      0

#define LV_USE_SLIDER     1

#define LV_USE_SPAN       0

#define LV_USE_SPINBOX    0

#define LV_USE_SPINNER    0

#define LV_USE_SWITCH     1

#define LV_USE_TABLE      1

#define LV_USE_TABVIEW    0

#define LV_USE_TEXTAREA   1
#if LV_USE_TEXTAREA != 0
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500    /**< [ms] */
#endif

#define LV_USE_TILEVIEW   1

#define LV_USE_WIN        0

#define LV_USE_3DTEXTURE  0

#else  /* BOOT */

#define LV_WIDGETS_HAS_DEFAULT_VALUE  0

#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        0
#define LV_USE_ARCLABEL  0
#define LV_USE_BAR        0
#define LV_USE_BUTTON        0
#define LV_USE_BUTTONMATRIX  0
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGE      0
#define LV_USE_IMAGEBUTTON     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      0
#define LV_USE_LED        0
#define LV_USE_LINE       0
#define LV_USE_LIST       0
#define LV_USE_LOTTIE     0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TEXTAREA   0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_3DTEXTURE  0

#endif /* !defined(BOOT) */

/*==================
 * THEMES
 *==================*/

/* Edge16 uses a custom theme; built-in themes are disabled. */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

/*==================
 * LAYOUTS
 *==================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
 * 3RD PARTIES LIBRARIES
 *====================*/

/* File system interfaces for common APIs */

#define LV_FS_DEFAULT_DRIVER_LETTER '\0'

/* Edge16: STDIO, POSIX, WIN32, LITTLEFS not used */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0

/*
 * Edge16: FATFS for SD card access.
 * Disabled in BOOT, enabled in firmware with letter 'A'.
 */
#if defined(BOOT)
#define LV_USE_FS_FATFS 0
#else
#define LV_USE_FS_FATFS  1
#endif
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER 'A'
    #define LV_FS_FATFS_PATH ""
    #define LV_FS_FATFS_CACHE_SIZE 0
#endif

#define LV_USE_FS_MEMFS 0
#define LV_USE_FS_LITTLEFS 0
#define LV_USE_FS_ARDUINO_ESP_LITTLEFS 0
#define LV_USE_FS_ARDUINO_SD 0
#define LV_USE_FS_UEFI 0
#define LV_USE_FS_FROGFS 0

/* Edge16: image decoders disabled (Edge16 uses raw bin images) */
#define LV_USE_LODEPNG 0
#define LV_USE_LIBPNG 0
#define LV_USE_BMP 0
#define LV_USE_TJPGD 0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_LIBWEBP 0
#define LV_USE_GIF 0
#define LV_USE_GSTREAMER 0
#define LV_BIN_DECODER_RAM_LOAD 0
#define LV_USE_RLE 0

/* Edge16: QR code library enabled in firmware, disabled in BOOT */
#if defined(BOOT)
#define LV_USE_QRCODE 0
#else
#define LV_USE_QRCODE 1
#endif
#define LV_USE_BARCODE 0

/* Edge16: no FreeType, TinyTTF, RLottie, FFmpeg */
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_RLOTTIE 0
#define LV_USE_GLTF  0
#define LV_USE_VECTOR_GRAPHIC  0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_NANOVG 0
#define LV_USE_LZ4_INTERNAL  0
#define LV_USE_LZ4_EXTERNAL  0
#define LV_USE_SVG 0
#define LV_USE_FFMPEG 0

/*==================
 * OTHERS
 *==================*/

/* Edge16: snapshot API enabled for screenshots */
#define LV_USE_SNAPSHOT 1

/*
 * Edge16: system monitor and perf monitor.
 * Only enabled in non-BOOT debug builds (UI_PERF_MONITOR defined).
 */
#if !defined(BOOT) && defined(UI_PERF_MONITOR)
#define LV_USE_SYSMON   1
#else
#define LV_USE_SYSMON   0
#endif
#if LV_USE_SYSMON
    #define LV_SYSMON_GET_IDLE lv_os_get_idle_percent
    #define LV_SYSMON_PROC_IDLE_AVAILABLE 0
    #define LV_USE_PERF_MONITOR 1
    #if LV_USE_PERF_MONITOR
        #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
        #define LV_USE_PERF_MONITOR_LOG_MODE 0
    #endif
    #define LV_USE_MEM_MONITOR 0
#endif /*LV_USE_SYSMON*/

/** 1: Enable runtime performance profiler */
#define LV_USE_PROFILER 0

/** 1: Enable Monkey test */
#define LV_USE_MONKEY 0

/** 1: Enable grid navigation */
#define LV_USE_GRIDNAV 0

/** 1: Enable `lv_obj` fragment logic */
#define LV_USE_FRAGMENT 0

/** 1: Support using images as font in label or span widgets */
#define LV_USE_IMGFONT 0

/** 1: Enable an observer pattern implementation */
#define LV_USE_OBSERVER 1

/** 1: Enable Pinyin input method */
#define LV_USE_IME_PINYIN 0

/** 1: Enable file explorer */
#define LV_USE_FILE_EXPLORER 0

/** 1: Enable Font manager */
#define LV_USE_FONT_MANAGER 0

/** Enable emulated input devices, time emulation, and screenshot compares. */
#define LV_USE_TEST 0

/** 1: Enable text translation support */
#define LV_USE_TRANSLATION 0

/*1: Enable color filter style*/
#define LV_USE_COLOR_FILTER     0

/*==================
 * DEVICES
 *==================*/

/* Edge16: all desktop/embedded display backends are unused. */
#define LV_USE_SDL              0
#define LV_USE_X11              0
#define LV_USE_WAYLAND          0
#define LV_USE_LINUX_FBDEV      0
#define LV_USE_NUTTX    0
#define LV_USE_LINUX_DRM        0
#define LV_USE_TFT_ESPI         0
#define LV_USE_LOVYAN_GFX       0
#define LV_USE_EVDEV    0
#define LV_USE_LIBINPUT    0
#define LV_USE_ST7735        0
#define LV_USE_ST7789        0
#define LV_USE_ST7796        0
#define LV_USE_ILI9341       0
#define LV_USE_FT81X         0
#define LV_USE_NV3007        0
#define LV_USE_GENERIC_MIPI  0
#define LV_USE_RENESAS_GLCDC    0
#define LV_USE_ST_LTDC    0
#define LV_USE_NXP_ELCDIF   0
#define LV_USE_WINDOWS    0
#define LV_USE_UEFI 0
#define LV_USE_OPENGLES   0
#define LV_USE_GLFW   0
#define LV_USE_QNX              0

/** Enable or disable for external data and destructor function */
#define LV_USE_EXT_DATA   0

/*=====================
* BUILD OPTIONS
*======================*/

/** Enable examples to be built with the library. */
#define LV_BUILD_EXAMPLES 0

/** Build the demos */
#define LV_BUILD_DEMOS 0

/*===================
 * DEMO USAGE
 ====================*/

/* All demos disabled. */
#if LV_BUILD_DEMOS
    #define LV_USE_DEMO_WIDGETS 0
    #define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
    #define LV_USE_DEMO_BENCHMARK 0
    #define LV_USE_DEMO_RENDER 0
    #define LV_USE_DEMO_STRESS 0
    #define LV_USE_DEMO_MUSIC 0
    #define LV_USE_DEMO_VECTOR_GRAPHIC  0
    #define LV_USE_DEMO_GLTF            0
    #define LV_USE_DEMO_FLEX_LAYOUT     0
    #define LV_USE_DEMO_MULTILANG       0
    #define LV_USE_DEMO_EBIKE           0
    #define LV_USE_DEMO_HIGH_RES        0
    #define LV_USE_DEMO_SMARTWATCH      0
#endif /* LV_BUILD_DEMOS */

/*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/

#endif /*End of "Content enable"*/
