/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ui_feedback.h"

#include "etx_lv_theme.h"
#include "lcd.h"

namespace
{
bool feedbackInProgress = false;

#if defined(SIMU)
uint32_t ackFrameCount = 0;
#endif

// Guards against a handler that itself pumps LVGL (and could otherwise
// re-enter this while the forced refresh is still on the stack).
bool forceRefreshGuarded()
{
  if (feedbackInProgress) return false;
  feedbackInProgress = true;
  bool idle = lvglRefreshNowIfIdle();
  feedbackInProgress = false;
  return idle;
}
}  // namespace

void UiFeedback::ackFrame(lv_obj_t* obj)
{
  if (!obj) return;
#if defined(SIMU)
  ackFrameCount += 1;
#endif
  lv_obj_add_state(obj, LV_STATE_PRESSED);
  forceRefreshGuarded();
}

bool UiFeedback::forceFrame() { return forceRefreshGuarded(); }

#if defined(SIMU)
uint32_t UiFeedback::ackFrameCountForTest() { return ackFrameCount; }
#endif

Window* UiFeedback::showBuildScrim(Window& parent, const rect_t& rect)
{
  auto scrim = Window::makeLive<Window>(&parent, rect);
  if (!scrim) return nullptr;

  scrim->setWindowFlag(NO_FOCUS | NO_SCROLL | NO_CLICK);
  scrim->withLive([](Window::LiveWindow& live) {
    etx_obj_add_style(live.lvobj(), styles->bg_opacity_50, LV_PART_MAIN);
    etx_bg_color(live.lvobj(), COLOR_BLACK_INDEX, LV_PART_MAIN);
  });

  forceRefreshGuarded();
  return scrim;
}
