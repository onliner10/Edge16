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

class TextButton;

class ModelSetupPage: public PageGroupItem {
 public:
  ModelSetupPage(const PageDef& pageDef);

  void build(Window * window) override;

  static LAYOUT_SIZE(BTN_COLS, 3, 3) // ds-allow: model-setup sub-page button-group column count; non-form navigation grid metric, not a DS list
  static LAYOUT_VAL_SCALED(BTN_H, 62) // ds-allow: model-setup sub-page button-group cell height; non-form navigation grid metric, not a DS list
  static LAYOUT_VAL_SCALED(OPTS_W, 100) // ds-allow: model-setup ADC-filter option Choice width used for absolute label placement; multi-control line metric, not a DS FormRow
  static LAYOUT_SIZE_SCALED(NAM_W, 200, 140) // ds-allow: model-setup name-field width constant for absolute field sizing; not a DS FormRow metric
};
