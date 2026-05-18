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

#include "edgetx_lvgl_adapter.h"
#include <cstddef>

// ------------------------------------------------------------------
// Internal static storage
//
// Edge16 uses exactly one display and up to three input devices
// (pointer, keypad, encoder).  The display handle lives for the
// lifetime of the program, stored as a file-scope static within the
// adapter translation unit.
//
// v8 static driver structs (lv_disp_draw_buf_t, lv_disp_drv_t,
// lv_indev_drv_t) are not used in v9 — lv_display_create() and
// lv_indev_create() manage their own internal state.
// ------------------------------------------------------------------

namespace etx {
namespace lvgl {

namespace {

static lv_display_t* s_disp = nullptr;

}  // anonymous namespace

// ==================================================================
// Display functions
// ==================================================================

lv_disp_t* etx_lvgl_disp_create(lv_display_flush_cb_t flush_cb,
                                 lv_display_flush_wait_cb_t wait_cb,
                                 void* buf1, void* buf2,
                                 lv_coord_t w, lv_coord_t h,
                                 bool full_refresh, bool direct_mode)
{
  // v9 device creation
  s_disp = lv_display_create(w, h);
  if (!s_disp) {
    return nullptr;
  }

  // v9 defaults to an ARGB/XRGB display format on some configurations. Edge16
  // frame buffers are RGB565, so set the display format before calculating the
  // draw-buffer size and registering the buffers.
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

  // Determine render mode from the v8-originated flags
  lv_display_render_mode_t render_mode;
  if (full_refresh) {
    render_mode = LV_DISPLAY_RENDER_MODE_FULL;
  } else if (direct_mode) {
    render_mode = LV_DISPLAY_RENDER_MODE_DIRECT;
  } else {
    render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
  }

  // Set buffers (byte size — v9 API)
  uint32_t buf_size = static_cast<uint32_t>(w * h * sizeof(uint16_t));
  lv_display_set_buffers(s_disp, buf1, buf2, buf_size, render_mode);

  // Set flush callback
  lv_display_set_flush_cb(s_disp, flush_cb);

  // Set wait callback (still supported in v9 as flush_wait_cb)
  lv_display_set_flush_wait_cb(s_disp, wait_cb);

  return reinterpret_cast<lv_disp_t*>(s_disp);
}

lv_timer_t* etx_lvgl_get_disp_refr_timer()
{
  if (!s_disp) return nullptr;
  return lv_display_get_refr_timer(s_disp);
}

// ==================================================================
// Input device functions
// ==================================================================

lv_indev_t* etx_lvgl_indev_create_pointer(
    lv_indev_read_cb_t read_cb,
    uint8_t scroll_limit, uint8_t scroll_throw)
{
  if (!s_disp) return nullptr;

  lv_indev_t* indev = lv_indev_create();
  if (!indev) return nullptr;

  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, read_cb);
  lv_indev_set_scroll_limit(indev, scroll_limit);
  lv_indev_set_scroll_throw(indev, scroll_throw);
  lv_indev_set_display(indev, s_disp);
  return indev;
}

lv_indev_t* etx_lvgl_indev_create_keypad(
    lv_indev_read_cb_t read_cb)
{
  if (!s_disp) return nullptr;

  lv_indev_t* indev = lv_indev_create();
  if (!indev) return nullptr;

  lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev, read_cb);
  lv_indev_set_display(indev, s_disp);
  return indev;
}

lv_indev_t* etx_lvgl_indev_create_encoder(
    lv_indev_read_cb_t read_cb)
{
  if (!s_disp) return nullptr;

  lv_indev_t* indev = lv_indev_create();
  if (!indev) return nullptr;

  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, read_cb);
  lv_indev_set_display(indev, s_disp);
  return indev;
}

void etx_lvgl_indev_read_timer_cb(lv_indev_t* indev)
{
  if (!indev) return;

  lv_timer_t* timer = lv_indev_get_read_timer(indev);
  if (timer) {
    lv_indev_read_timer_cb(timer);
  }
}

}  // namespace lvgl
}  // namespace etx
