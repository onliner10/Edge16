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

#include "window.h"
#include "theme_manager.h"

class HeaderDateTime;

// display controls using the appropriate theme.
class PreviewWindow : public Window
{
 public:
  PreviewWindow(Window *window, rect_t rect, std::vector<ColorEntry> colorList);

  void setColorList(std::vector<ColorEntry> colorList);

  static LAYOUT_VAL_SCALED(DATE_XO, 46)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CBT_X, 5)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CBT_Y, 54)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CBT_W, 100)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CB1_X, 100)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CB2_X, 154)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CB_Y, 50)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(CB_W, 50)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(BTN_X, 210)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(BTN1_Y, 48)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(BTN2_Y, 82)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(BTN_W, 100)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(TRIM_Y, 89)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(SLIDER_Y, 111)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(TXT1_Y, 132)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(TXT2_Y, 154)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(TXT_W, 100)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(EDT2_X, 94)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(EDT_Y, 180)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(EDY_W, 80)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(QM_X, 180)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(QM_Y, 117)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(QM_W, 140)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
  static LAYOUT_VAL_SCALED(QM_H, 104)  // ds-allow: main-view preview - miniature absolute-positioned replica of the home screen (date/quick-menu/trims/sliders/widgets) for the theme editor; canvas mockup, not a DS surface.
};
