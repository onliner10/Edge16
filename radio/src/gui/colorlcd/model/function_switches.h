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

#if defined(FUNCTION_SWITCHES)

#include "edgetx.h"
#include "page.h"
#include "messaging.h"

class Choice;
class ColorPicker;
class StaticText;
class SwitchGroup;
class ToggleSwitch;

//-----------------------------------------------------------------------------

class FunctionSwitchBase : public Window
{
 public:
  FunctionSwitchBase(Window* parent, uint8_t sw);

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  static uint8_t editSwitch() { return activeSwitch; }
#endif

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  static LAYOUT_VAL_SCALED(SW_W, 70)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t NM_X = SW_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(NM_W, 60)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t TP_X = NM_X + NM_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(TP_W, 78)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t GR_X = TP_X + TP_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(GR_W, 84)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t ST_X = GR_X + GR_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(ST_W, 60)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t ROW_HS = EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_OUTLINE * 2;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
#if NARROW_LAYOUT
  static constexpr coord_t ROW_H = EdgeTxStyles::UI_ELEMENT_HEIGHT * 3 + PAD_OUTLINE * 4;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C1_X = GR_X;
  static constexpr coord_t C1_Y = EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_OUTLINE;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(C1_W, 40)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C2_X = ST_X;
  static constexpr coord_t C2_Y = EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_OUTLINE;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(C2_W, 40)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t OVRLBL_X = NM_X;
  static constexpr coord_t OVRLBL_W = NM_W + TP_W;
  static constexpr coord_t OVROFF_X = C1_X;
  static constexpr coord_t COLLBL_YO = PAD_MEDIUM;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
#else
  static constexpr coord_t ROW_H = EdgeTxStyles::UI_ELEMENT_HEIGHT * 2 + PAD_OUTLINE * 3;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C1_X = ST_X + ST_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C1_Y = 0;
  static LAYOUT_VAL_SCALED(C1_W, 40)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C2_X = C1_X + C1_W + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t C2_Y = 0;
  static LAYOUT_VAL_SCALED(C2_W, 40)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t OVRLBL_X = GR_X;
  static constexpr coord_t OVRLBL_W = GR_W + ST_W - PAD_LARGE;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t OVROFF_X = C1_X - PAD_MEDIUM * 2;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t COLLBL_YO = PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
#endif
#else
  static constexpr coord_t ROW_H = EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_OUTLINE * 2;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t SW_W = (LCD_W - PAD_SMALL * 2 - PAD_TINY * 4) / 5;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t NM_X = SW_W + PAD_TINY;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(NM_W, 80)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t TP_X = NM_X + SW_W + PAD_TINY;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(TP_W, 86)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t GR_X = TP_X + SW_W + PAD_TINY;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(GR_W, 94)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static constexpr coord_t ST_X = GR_X + SW_W + PAD_LARGE * 2 + PAD_SMALL;  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
  static LAYOUT_VAL_SCALED(ST_W, 70)  // ds-allow: function-switch config - per-switch columns with in-cell on/off override toggles positioned absolutely; FUNCTION_SWITCHES hardware page (not built on TX16S sim); ds::Grid's text-only cells can't hold the embedded toggles.
#endif

 protected:
  uint8_t switchIndex;
  Choice* typeChoice = nullptr;
  Choice* startChoice = nullptr;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
#if defined(NARROW_LAYOUT)
  StaticText *offLabel = nullptr;
  StaticText *onLabel = nullptr;
#endif
  ColorPicker* offColor = nullptr;
  ColorPicker* onColor = nullptr;
  RGBLedColor offValue;
  RGBLedColor onValue;
  StaticText* overrideLabel = nullptr;
  ToggleSwitch* onOverride = nullptr;
  ToggleSwitch* offOverride = nullptr;
  static uint8_t activeSwitch;
#endif
  int lastType = -1;

  void setLEDState(uint8_t typ);
};

//-----------------------------------------------------------------------------

class FunctionSwitchesBase : public Page
{
 public:
  FunctionSwitchesBase(EdgeTxIcon icon, Route route, const char* title);

  void addQRCode();

 protected:
  BitmapBuffer* qrcode = nullptr;
  StaticText* startupHeader = nullptr;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  Messaging previewMsg;
#endif

  virtual void setState() = 0;
  void onLiveCheckEvents(LiveWindow& live) override;
};

//-----------------------------------------------------------------------------

class ModelFunctionSwitches : public FunctionSwitchesBase
{
 public:
  ModelFunctionSwitches(Route route);

 protected:
  SwitchGroup* groupLines[3] = {nullptr};

  void setState() override;
};

#endif

//-----------------------------------------------------------------------------
