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
  //
  // This is the primitive behind the design-system rule that *every* tap is
  // acknowledged instantly: the libui choke-points (ButtonBase::onLiveClicked,
  // TableField row dispatch, MenuBody rows) call it before running the user
  // handler, so a tap that starts heavy synchronous work (page build, model
  // load, directory scan) never looks like a dropped tap. Callers that leave
  // the pressed state set for the duration of a blocking handler get a
  // sustained highlight for free (the cue stays on the LCD until the handler
  // returns and the state is cleared).
  static void ackFrame(lv_obj_t* obj);

  // Forces any pending invalidated regions to the LCD immediately, when the
  // display is idle. Reusable, cheap building block shared by ackFrame() and
  // showBuildScrim(): a heavy, blocking call site that refreshes an on-screen
  // progress cue mid-operation can call this to push an extra frame without
  // pulling in a modal spinner or changing navigation flow. No-op while a
  // flush or ack frame is already in flight. Returns true if a frame was
  // flushed.
  static bool forceFrame();

  // Paints a one-shot static dim layer over `rect` (in `parent`'s
  // coordinate space) and forces it to the LCD before returning. The
  // caller owns the returned window and must deleteLater() it once the
  // content it was covering has been rebuilt.
  static Window* showBuildScrim(Window& parent, const rect_t& rect);

#if defined(SIMU)
  // Number of ackFrame() calls so far this process. Test hook used to assert
  // that a tap-dispatch path acknowledges before running its handler.
  static uint32_t ackFrameCountForTest();
#endif
};
