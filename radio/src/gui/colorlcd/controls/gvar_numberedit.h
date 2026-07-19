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

#include "form.h"
#include "choice.h"
#include "numberedit.h"
#include "gvars.h"

class TextButton;

class GVarNumberEdit : public Window
{
 public:
  GVarNumberEdit(Window* parent, int32_t vmin, int32_t vmax,
                 std::function<int32_t()> getValue,
                 std::function<void(int32_t)> setValue,
                 LcdFlags textFlags = 0, int32_t voffset = 0,
                 int32_t vdefault = NO_DEFAULT);

  // Sentinel meaning "no explicit default" — keeps the wheel's Reset button
  // hidden for fields that never declared a real default value.
  static constexpr int32_t NO_DEFAULT = INT32_MIN;

  void switchGVarMode();

  void setFastStep(int value) { num_field->setFastStep(value); }
  void setAccelFactor(int value) { num_field->setAccelFactor(value); }
  void setEditTitle(std::string value)
  {
    if (num_field) num_field->setEditTitle(std::move(value));
  }
  void setDisplayHandler(std::function<std::string(int value)> function);

  static LAYOUT_VAL_SCALED(GV_BTN_W, 40)  // ds-allow: composite GVAR field; GV toggle-button width for the fixed-width NumberEdit + button pack, not a DS token

#if defined(SIMU)
  // Test-only: lets test code drive the GVAR dropdown the same way the
  // menu selection handler does, without needing a full touch/menu sim.
  Choice* getGvarFieldForTest() const { return gvar_field; }
#endif

 protected:
  Choice* gvar_field = nullptr;
  NumberEdit* num_field = nullptr;
  FormField* act_field = nullptr;
#if defined(GVARS)
  TextButton* m_gvBtn = nullptr;
#endif

  int32_t vmin;
  int32_t vmax;
  std::function<int32_t()> getValue;
  std::function<void(int32_t)> setValue;
  int32_t voffset;

  // Raw numeric value cached when switching INTO GV mode, so it can be
  // restored if the user switches back OUT of GV mode without actually
  // picking a GVAR — otherwise the shared raw/GVAR storage silently loses
  // the previously tuned raw value. Invalidated as soon as a GVAR is
  // genuinely picked (see the gvar_field setValue handler), so the normal
  // GET_GVAR/GET_GVAR_PREC1 conversion still applies for a real binding.
  int32_t rawValueBeforeGVar = 0;
  bool rawValueBeforeGVarValid = false;

  void update();

  static void value_changed(lv_event_t* e);
};
