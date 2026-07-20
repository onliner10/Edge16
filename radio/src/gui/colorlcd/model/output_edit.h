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

#include "page.h"
#include "gvar_numberedit.h"

// deadband in % for switching direction of Min/Max text and value field highlighting
// 0 = no deadband
// 1..100 = [-DEADBAND; DEADBAND]
#define DEADBAND 0

class OutputEditStatusBar;

namespace ds {
class FieldRow;
}

class OutputEditWindow : public Page
{
 public:
  explicit OutputEditWindow(uint8_t channel, Route route);

  static LAYOUT_SIZE_SCALED(OUTPUT_EDIT_STATUS_BAR_WIDTH, 250, 180)  // ds-allow: output edit - channel status-bar width constant; multi-region page, not a plain DS FormRow list.

 protected:
  uint8_t channel;
  int value = 0;
  ds::FieldRow* minMaxRow = nullptr;
  GVarNumberEdit* minEdit = nullptr;
  GVarNumberEdit* maxEdit = nullptr;
  OutputEditStatusBar *statusBar = nullptr;

  void onLiveCheckEvents(LiveWindow& live) override;
  void buildHeader(Window *window);
  void buildBody(Window *window);
};
