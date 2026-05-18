/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <lvgl/lvgl.h>

// ------------------------------------------------------------------
// LVGL v9 compatibility adapter for Edge16 firmware.
//
// All firmware UI code should call through this layer instead of
// accessing LVGL driver structs or private API functions directly.
//
// v9 API wrappers are marked inline; nontrivial functions live in
// edgetx_lvgl_adapter.cpp.
//
// The adapter encapsulates the LVGL v8 to v9 API migration for:
//   - Display creation, flush signalling, and refresh control
//   - Input device creation and configuration
//   - Theme management
//   - Timer / animation queries
//
// Internal static storage for the display pointer lives in the
// adapter translation unit (one display, matching Edge16 hardware).
// ------------------------------------------------------------------

namespace etx {
namespace lvgl {

// ==================================================================
// Display functions
// ==================================================================

// Combined display buffer + driver init and registration (v9 path).
// Internally: lv_display_create + lv_display_set_buffers +
// lv_display_set_flush_cb + lv_display_set_flush_wait_cb.
// The adapter stores the created lv_display_t* internally so that
// etx_lvgl_flush_ready() and etx_lvgl_flush_is_last() can be called
// without passing a display handle.
lv_disp_t* etx_lvgl_disp_create(lv_display_flush_cb_t flush_cb,
                                 lv_display_flush_wait_cb_t wait_cb,
                                 void* buf1, void* buf2, lv_coord_t w,
                                 lv_coord_t h, bool full_refresh,
                                 bool direct_mode);

// Signal that the current display flush chunk is complete.
// Uses the display pointer stored during etx_lvgl_disp_create().
inline void etx_lvgl_flush_ready()
{
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_display_flush_ready(disp);
  }
}

// Query whether the current flush chunk is the last one.
// Uses the display pointer stored during etx_lvgl_disp_create().
inline bool etx_lvgl_flush_is_last()
{
  lv_display_t* disp = lv_display_get_default();
  return !disp || lv_display_flush_is_last(disp);
}

// Force an immediate refresh of the default display.
inline void etx_lvgl_refr_now()
{
  lv_refr_now(nullptr);
}

// Get the refresh timer of the default display.
// v8: _lv_disp_get_refr_timer(nullptr)  (private API)
// v9: lv_display_get_refr_timer()     (public API)
lv_timer_t* etx_lvgl_get_disp_refr_timer();

// Get the default display handle.
// Note: lv_disp_t is typedef'd to lv_display_t in v9 via api_map.
inline lv_disp_t* etx_lvgl_disp_get_default()
{
  return lv_disp_get_default();
}

// Get the active screen of the default display.
// v8: lv_scr_act() maps to v9 lv_screen_active() via api_map.
inline lv_obj_t* etx_lvgl_get_scr_act() { return lv_scr_act(); }

// ==================================================================
// Input device functions
// ==================================================================

// Create a POINTER-type input device.
// v9: lv_indev_create() + lv_indev_set_type() + lv_indev_set_read_cb()
//      + lv_indev_set_scroll_limit() + lv_indev_set_scroll_throw()
//      + lv_display_add_indev()
lv_indev_t* etx_lvgl_indev_create_pointer(
    lv_indev_read_cb_t read_cb,
    uint8_t scroll_limit, uint8_t scroll_throw);

// Create a KEYPAD-type input device.
lv_indev_t* etx_lvgl_indev_create_keypad(
    lv_indev_read_cb_t read_cb);

// Create an ENCODER-type input device.
lv_indev_t* etx_lvgl_indev_create_encoder(
    lv_indev_read_cb_t read_cb);

// Get the group attached to an input device.
// v8: indev->group (direct field access)
// v9: lv_indev_get_group(indev) (public API)
inline lv_group_t* etx_lvgl_indev_get_group(lv_indev_t* indev)
{
  return lv_indev_get_group(indev);
}

// Read the input device via its driver's read_timer.
// v8: lv_indev_read_timer_cb(indev->driver->read_timer)
// v9: lv_indev_read_timer_cb(lv_indev_get_read_timer(indev))
void etx_lvgl_indev_read_timer_cb(lv_indev_t* indev);

// Get the read timer from an input device.
// v8: indev->driver->read_timer (direct field access)
// v9: lv_indev_get_read_timer(indev) (public API)
inline lv_timer_t* etx_lvgl_indev_get_read_timer(lv_indev_t* indev)
{
  if (!indev) return nullptr;
  return lv_indev_get_read_timer(indev);
}

// ==================================================================
// Timer / handler
// ==================================================================

// Run the LVGL timer handler.
inline uint32_t etx_lvgl_timer_handler() { return lv_timer_handler(); }

// Set the period of an LVGL timer.
inline void etx_lvgl_timer_set_period(lv_timer_t* timer, uint32_t period)
{
  lv_timer_set_period(timer, period);
}

// Count the number of running LVGL animations.
inline uint16_t etx_lvgl_anim_count_running()
{
  return lv_anim_count_running();
}

// ==================================================================
// Display state (for adaptive UI pump)
//
// v8 private fields removed in v9:
//   - disp->rendering_in_progress: no public equivalent
//   - disp->inv_p: no public equivalent
//
// Adaptive-pump work detection now relies on:
//   - Pending input events
//   - Active input state timers
//   - Running animations (lv_anim_count_running() > 0)
//   - Input device scroll state
// ==================================================================

// ==================================================================
// Screen functions
// ==================================================================

// Get the active screen of the default display (alias).
inline lv_obj_t* etx_lvgl_screen_active() { return lv_scr_act(); }

// Remove all styles from an object.
inline void etx_lvgl_remove_style_all(lv_obj_t* obj)
{
  if (obj) {
    lv_obj_remove_style_all(obj);
  }
}

// ==================================================================
// Theme
// ==================================================================

// Edge16 local theme type — mirrors the v9 _lv_theme_t struct to
// avoid depending on the private header layout. The adapter manages
// a static instance.
//
// v9 lv_theme_t is opaque in the public API (only forward-declared).
// We define the same fields here for direct initialisation.
struct etx_lvgl_theme_t {
  void (*apply_cb)(struct _lv_theme_t*, lv_obj_t*);
  struct _lv_theme_t* parent;
  void* user_data;
  lv_display_t* disp;
  lv_color_t color_primary;
  lv_color_t color_secondary;
  const lv_font_t* font_small;
  const lv_font_t* font_normal;
  const lv_font_t* font_large;
  uint32_t flags;
};

// Assign a theme to the default display.
inline void etx_lvgl_disp_set_theme(lv_theme_t* theme)
{
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_display_set_theme(disp, theme);
  }
}

// Initialise an etx_lvgl_theme_t struct with the given parameters.
// Fields not managed by Edge16 (apply_cb, parent, user_data, disp)
// are zero-initialised.
inline void etx_lvgl_theme_init(etx_lvgl_theme_t* theme,
                                 lv_color_t color_primary,
                                 lv_color_t color_secondary,
                                 const lv_font_t* font_small,
                                 const lv_font_t* font_normal,
                                 const lv_font_t* font_large,
                                 uint32_t flags)
{
  theme->apply_cb = nullptr;
  theme->parent = nullptr;
  theme->user_data = nullptr;
  theme->disp = nullptr;
  theme->color_primary = color_primary;
  theme->color_secondary = color_secondary;
  theme->font_small = font_small;
  theme->font_normal = font_normal;
  theme->font_large = font_large;
  theme->flags = flags;
}

// Get a palette main color.
inline lv_color_t etx_lvgl_palette_main(lv_palette_t p)
{
  return lv_palette_main(p);
}

// ==================================================================
// Input device iteration / scroll
// ==================================================================

// Iterate through registered input devices.
inline lv_indev_t* etx_lvgl_indev_get_next(lv_indev_t* indev)
{
  return lv_indev_get_next(indev);
}

// Get the currently scrolled object for an input device.
inline lv_obj_t* etx_lvgl_indev_get_scroll_obj(lv_indev_t* indev)
{
  return lv_indev_get_scroll_obj(indev);
}

// Get the current scroll direction for an input device.
inline lv_dir_t etx_lvgl_indev_get_scroll_dir(lv_indev_t* indev)
{
  return lv_indev_get_scroll_dir(indev);
}

// Reset one or all input devices.
inline void etx_lvgl_indev_reset(lv_indev_t* indev, lv_obj_t* obj)
{
  lv_indev_reset(indev, obj);
}

// Wait until the next input device release.
inline void etx_lvgl_indev_wait_release(lv_indev_t* indev)
{
  lv_indev_wait_release(indev);
}

// ==================================================================
// Type aliases and constants
// ==================================================================

// lv_indev_data_t re-exposed under the adapter namespace so that
// callers do not need to include LVGL headers directly.
using etx_lvgl_indev_data_t = lv_indev_data_t;

// Input device state constants
constexpr lv_indev_state_t ETX_LVGL_INDEV_STATE_PRESSED =
    LV_INDEV_STATE_PRESSED;
constexpr lv_indev_state_t ETX_LVGL_INDEV_STATE_RELEASED =
    LV_INDEV_STATE_RELEASED;

// Key codes
constexpr uint32_t ETX_LVGL_KEY_ENTER = LV_KEY_ENTER;
constexpr uint32_t ETX_LVGL_KEY_ESC = LV_KEY_ESC;

}  // namespace lvgl
}  // namespace etx
