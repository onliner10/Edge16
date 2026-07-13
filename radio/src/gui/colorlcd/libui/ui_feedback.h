/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include "window.h"

// Global "instant tap feedback" helpers. A click handler that
// synchronously builds a page can block the UI thread for a second or
// more; these helpers force a synchronous partial repaint so the tap (or
// the outgoing content being replaced) is acknowledged on the LCD before
// that happens.
class UiFeedback
{
 public:
  // Re-asserts the pressed visual state on `obj` (LVGL already cleared it
  // before the CLICKED event is dispatched) and forces the frame to the
  // LCD. Cheap: only invalidated areas are repainted, and the call is a
  // no-op while a flush is already in flight or another ack frame is in
  // progress.
  static void ackFrame(lv_obj_t* obj);

  // Paints a one-shot static dim layer over `rect` (in `parent`'s
  // coordinate space) and forces it to the LCD before returning. The
  // caller owns the returned window and must deleteLater() it once the
  // content it was covering has been rebuilt.
  static Window* showBuildScrim(Window& parent, const rect_t& rect);
};
