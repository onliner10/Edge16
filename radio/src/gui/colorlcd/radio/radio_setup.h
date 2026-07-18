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

#include "pagegroup.h"

class RadioSetupPage: public PageGroupItem
{
 public:
  RadioSetupPage(const PageDef& pageDef);

  void build(Window * window) override;

  static LAYOUT_ORIENTATION(BTN_COLS, 3, 2)  // ds-allow: radio setup — column count for multi-column radio-setup button group; not a single DS FormRow control.
  static LAYOUT_VAL_SCALED(BTN_H, 62)  // ds-allow: radio setup — fixed button height for multi-column button group; not a single DS FormRow control.
  static LAYOUT_VAL_SCALED(HATS_MODE_W, 120)  // ds-allow: radio setup — fixed width to place '?' help button beside hats-mode field; not a single DS FormRow control.
};
