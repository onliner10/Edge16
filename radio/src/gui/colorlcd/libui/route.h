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

#include "bitmaps.h"        // EdgeTxIcon
#include "dataconstants.h"  // LEN_MODEL_FILENAME

#include <cstdint>

// Editor/leaf route page identifiers.
//
// Tab pages reuse QMPage values directly (PageGroupItem::pageId()).
// Editor/leaf pages use values starting at 128 to avoid collision
// with the QMPage enum (which currently stays below 60).
enum RouteEditorPage : uint8_t {
  RP_NONE = 0,
  // Model section editors
  RP_MIX_EDIT = 128,
  RP_INPUT_EDIT = 129,
  RP_CURVE_EDIT = 130,
  RP_OUTPUT_EDIT = 131,
  RP_LOGICAL_SWITCH_EDIT = 132,
  RP_SPECIAL_FUNCTION_EDIT = 133,
  RP_GLOBAL_FUNCTION_EDIT = 134,
  RP_FLIGHT_MODE_EDIT = 135,
  RP_GVAR_EDIT = 136,
  RP_SENSOR_EDIT = 137,
  RP_SCRIPT_EDIT = 138,
  RP_TIMER_EDIT = 139,
  RP_USB_CHANNEL_EDIT = 140,
  // Nested sub-editors
  RP_MIX_ADVANCED = 141,
  RP_INPUT_ADVANCED = 142,
  // Radio section editors
  RP_BATTERY_PACK_EDIT = 143,
};

// A compact, heap-free navigation path — like a URL but for firmware.
//
// rootIcon identifies the section (ICON_MODEL, ICON_RADIO, etc.).
// pages[0..depth-1] are segment identifiers:
//   pages[0] = tab (QMPage value, e.g. QM_MODEL_MIXES)
//   pages[1] = editor (RouteEditorPage value, e.g. RP_MIX_EDIT)
//   pages[2] = nested sub-editor (future use)
// params[i] is the per-segment parameter (e.g. mix index).
//
// A page builds its route by appending its segment to its parent's route,
// producing a path like:  ICON_MODEL / QM_MODEL_MIXES / RP_MIX_EDIT(3)
struct Route {
  static constexpr uint8_t MAX_SEGMENTS = 5;
  EdgeTxIcon rootIcon = EDGETX_ICONS_COUNT;
  uint8_t depth = 0;
  uint8_t pages[MAX_SEGMENTS] = {};
  int16_t params[MAX_SEGMENTS] = {};

  bool valid() const { return rootIcon != EDGETX_ICONS_COUNT && depth > 0; }

  // Returns a child route with (pageId, param) appended. If the path is
  // already at MAX_SEGMENTS the route is returned unchanged.
  Route appended(uint8_t pageId, int16_t param) const
  {
    Route r = *this;
    if (r.depth < MAX_SEGMENTS) {
      r.pages[r.depth] = pageId;
      r.params[r.depth] = param;
      r.depth += 1;
    }
    return r;
  }
};

// The single remembered route slot — set on long-press RTN, consumed on
// section reopen.  modelScoped + modelFilename invalidate stale routes
// after a model switch.
struct RememberedRoute {
  Route route;
  bool valid = false;
  bool modelScoped = false;
  char modelFilename[LEN_MODEL_FILENAME + 1] = {};
};
