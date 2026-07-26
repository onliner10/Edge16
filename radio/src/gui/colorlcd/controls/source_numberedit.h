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

#include <cstdint>

#include "choice.h"
#include "form.h"
#include "numberedit.h"

class TextButton;

class SourceNumberEdit : public Window
{
 public:
  SourceNumberEdit(Window* parent, int32_t vmin, int32_t vmax,
                   std::function<int32_t()> getValue,
                   std::function<void(int32_t)> setValue,
                   int16_t sourceMin,
                   LcdFlags textFlags = 0, int32_t voffset = 0,
                   int32_t vdefault = NO_DEFAULT,
                   const char* editTitle = nullptr);

  // Sentinel meaning "no explicit default" — keeps the wheel's Reset button
  // hidden for fields that never declared a real default value.
  static constexpr int32_t NO_DEFAULT = INT32_MIN;

  void switchSourceMode();
  void setSuffix(const std::string& value);

  void setFastStep(int value) { num_field->setFastStep(value); }
  void setAccelFactor(int value) { num_field->setAccelFactor(value); }
  void setDefault(int value) { num_field->setDefault(value); }
  void setEditTitle(std::string value) { num_field->setEditTitle(std::move(value)); }

  void update();

  // 40, not 38: this is a tap target, and 38 put it under the 8 mm touch floor
  // for no reason -- its twin GV_BTN_W (gvar_numberedit.h) is already 40 and the
  // two render side by side on the same forms.
  static LAYOUT_VAL_SCALED(SRC_BTN_W, 40)  // ds-allow: composite source field; SRC toggle-button width for the fixed-width NumberEdit + button pack, not a DS token

 protected:
  Choice* source_field = nullptr;
  NumberEdit* num_field = nullptr;
  FormField* act_field = nullptr;
  TextButton* m_srcBtn = nullptr;

  int32_t vmin;
  int32_t vmax;
  int16_t sourceMin;
  std::function<int32_t()> getValue;
  std::function<void(int32_t)> setValue;
  int32_t voffset;

  bool isSource();

  static void value_changed(lv_event_t* e);
};
