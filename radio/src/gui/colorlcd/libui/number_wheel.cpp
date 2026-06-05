/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
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

#include "number_wheel.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "keys.h"
#include "mainwindow.h"
#include "numberedit.h"

// Padded card dimensions — fat-finger friendly
constexpr lv_coord_t CARD_W = 340;
constexpr lv_coord_t CARD_H = 200;
constexpr lv_coord_t CARD_PAD = 12;
constexpr lv_coord_t TITLE_H = 24;
constexpr lv_coord_t DIV_H = 1;
constexpr lv_coord_t ROLLER_H = 130;
constexpr lv_coord_t ROLLER_VISIBLE = 5;
constexpr lv_coord_t BTN_H = 32;

NumberWheel::NumberWheel(NumberEdit* numEdit) :
    ModalWindow(true), edit(numEdit)
{
  hasDecimal = edit->hasDecimalPrecision();
  precisionScale = edit->getPrecisionScale();
  buildContent();
}

void NumberWheel::buildContent()
{
  // Dark opaque overlay
  withLive([](LiveWindow& l) {
    lv_obj_set_style_bg_color(l.lvobj(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(l.lvobj(), LV_OPA_90, 0);
  });

  // Bright card, centered
  lv_obj_t* cardObj = nullptr;
  auto* card = new Window(this, rect_t{
    (LV_HOR_RES - CARD_W) / 2, (LV_VER_RES - CARD_H) / 2,
    CARD_W, CARD_H
  });
  card->withLive([&](LiveWindow& l) {
    cardObj = l.lvobj();
    lv_obj_set_style_bg_color(cardObj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cardObj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cardObj, 14, 0);
    lv_obj_clear_flag(cardObj, LV_OBJ_FLAG_SCROLLABLE);
  });

  // Title - show what's being edited
  std::string title = edit->getEditTitle();
  if (title.empty()) title = "Value";
  titleLabel = new StaticText(card, rect_t{0, 6, CARD_W, TITLE_H},
      title, COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD));
  titleLabel->withLive([](LiveWindow& l) {
    lv_obj_set_style_text_align(l.lvobj(), LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l.lvobj(), lv_color_black(), 0);
  });

  // --- Rollers area ---
  const int rollerW = hasDecimal ? 120 : 260;
  const int rollerX = hasDecimal ? 30 : (CARD_W - rollerW) / 2;
  const int wMin = wholeMin();
  const int wCnt = wholeCount();

  // Build whole roller options
  std::string wholeOpts;
  for (int i = 0; i < wCnt; i++) {
    if (i > 0) wholeOpts += '\n';
    wholeOpts += std::to_string(wMin + i);
  }

  wholeRoller = lv_roller_create(cardObj);
  lv_obj_set_pos(wholeRoller, rollerX, 40);
  lv_obj_set_size(wholeRoller, rollerW, ROLLER_H);
  etx_font(wholeRoller, FONT_STD_INDEX, 0);
  lv_roller_set_options(wholeRoller, wholeOpts.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(wholeRoller, 5);
  lv_obj_set_style_text_align(wholeRoller, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(wholeRoller, lv_color_hex(0x999999), LV_PART_MAIN);
  lv_obj_set_style_text_color(wholeRoller, lv_color_black(), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(wholeRoller, lv_palette_lighten(LV_PALETTE_BLUE, 3), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(wholeRoller, LV_OPA_30, LV_PART_SELECTED);
  lv_obj_set_style_radius(wholeRoller, 8, LV_PART_SELECTED);
  // Catch ENTER key to confirm
  lv_obj_add_event_cb(wholeRoller, &NumberWheel::onRollerKey, LV_EVENT_KEY, this);
  // Set current value
  int curVal = edit->getValue();
  int wholePart = curVal / precisionScale;
  int wholeIdx = wholePart - wMin;
  if (wholeIdx >= 0 && wholeIdx < wCnt) lv_roller_set_selected(wholeRoller, wholeIdx, LV_ANIM_OFF);

  if (hasDecimal) {
    auto* dot = new StaticText(card, rect_t{rollerX + rollerW + 4, 40, 16, ROLLER_H},
                                ".", COLOR_THEME_PRIMARY1_INDEX, CENTERED);
    dot->withLive([](LiveWindow& l) { lv_obj_set_style_text_color(l.lvobj(), lv_color_black(), 0); });

    decimalRoller = lv_roller_create(cardObj);
    lv_obj_set_pos(decimalRoller, rollerX + rollerW + 22, 40);
    lv_obj_set_size(decimalRoller, 120, ROLLER_H);
    etx_font(decimalRoller, FONT_STD_INDEX, 0);
    lv_roller_set_options(decimalRoller, ".0\n.1\n.2\n.3\n.4\n.5\n.6\n.7\n.8\n.9", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(decimalRoller, 5);
    lv_obj_set_style_text_align(decimalRoller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(decimalRoller, lv_color_hex(0x999999), LV_PART_MAIN);
    lv_obj_set_style_text_color(decimalRoller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(decimalRoller, lv_palette_lighten(LV_PALETTE_BLUE, 3), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(decimalRoller, LV_OPA_30, LV_PART_SELECTED);
    lv_obj_set_style_radius(decimalRoller, 8, LV_PART_SELECTED);
    lv_obj_add_event_cb(decimalRoller, &NumberWheel::onRollerKey, LV_EVENT_KEY, this);
    int decStep = precisionScale / 10;
    int decPart = decStep > 0 ? (std::abs(curVal) % precisionScale) / decStep : 0;
    lv_roller_set_selected(decimalRoller, decPart, LV_ANIM_OFF);
  }

  // Cancel / OK at bottom
  cancelButton = new TextButton(card, rect_t{16, CARD_H - BTN_H - 6, 140, BTN_H},
                                 "Cancel", [this]() { onCancel(); return 0; });
  okButton = new TextButton(card, rect_t{CARD_W - 156, CARD_H - BTN_H - 6, 140, BTN_H},
                             "OK", [this]() { onConfirm(); return 0; });
}

// --- Roller helpers ---

void NumberWheel::buildRoller(lv_obj_t* roller, int count)
{
  lv_obj_set_width(roller, LV_PCT(100));
  lv_obj_set_height(roller, LV_PCT(100));

  // Unselected rows
  lv_obj_set_style_text_color(roller, lv_color_hex(0x999999), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);

  // Selected row — highlighted
  lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(roller, lv_palette_lighten(LV_PALETTE_BLUE, 3), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(roller, LV_OPA_30, LV_PART_SELECTED);
  lv_obj_set_style_radius(roller, 8, LV_PART_SELECTED);

  // Center-align options
  lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_SELECTED);

  lv_roller_set_visible_row_count(roller, ROLLER_VISIBLE);
}

void NumberWheel::buildRollerOptions(lv_obj_t* roller, int count,
                                      std::function<std::string(int)> fmt)
{
  if (!roller || count <= 0) {
    count = 5;  // Always show at least 5 options
  }
  
  // Limit to reasonable size
  if (count > 1000) {
    count = 1000;
  }
  
  std::string options;
  options.reserve(count * 12);  // More space for formatting
  
  for (int i = 0; i < count; ++i) {
    if (i > 0) options += '\n';
    std::string val = fmt(i);
    if (val.empty()) {
      val = std::to_string(i);
    }
    options += val;
  }
  
  // Ensure we have at least some content
  if (options.empty()) {
    options = "0\n1\n2\n3\n4";
  }
  
  lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
  
  // Force a refresh to ensure the roller displays the options
  lv_obj_invalidate(roller);
}

void NumberWheel::selectValue(lv_obj_t* roller, int count, int cur)
{
  if (!roller || count <= 0) return;
  int idx = std::clamp(cur, 0, count - 1);
  lv_roller_set_selected(roller, idx, LV_ANIM_OFF);
}

// --- Range helpers ---

int NumberWheel::wholeMin() const
{
  int min = edit->getMin();
  return std::floor(static_cast<float>(min) / static_cast<float>(precisionScale));
}

int NumberWheel::wholeCount() const
{
  int max = edit->getMax();
  int wmin = wholeMin();
  int wmax = static_cast<int>(std::ceil(static_cast<float>(max) / static_cast<float>(precisionScale)));
  int count = wmax - wmin + 1;
  
  // Ensure we always have at least a reasonable number of options
  if (count < 5) {
    count = 5;
  }
  
  return count;
}

int NumberWheel::decimalMin() const { return 0; }

int NumberWheel::decimalCount() const {
  return 10; // 0..9 tenths
}

// --- Confirmation ---

void NumberWheel::onConfirm()
{
  if (!edit || !wholeRoller) return;

  int whole = lv_roller_get_selected(wholeRoller) + wholeMin();
  int newVal = whole * precisionScale;

  if (hasDecimal && decimalRoller) {
    int dec = lv_roller_get_selected(decimalRoller) * (precisionScale / 10);
    if (edit->getValue() < 0 && whole == 0) {
      newVal = -dec;  // for values between -0.9 and 0
    } else {
      newVal += dec;
    }
  }

  newVal = limit<int>(newVal, edit->getMin(), edit->getMax());
  edit->setValue(newVal);

  if (closeHandler) closeHandler();
  deleteLater();
}

void NumberWheel::onCancel()
{
  if (closeHandler) closeHandler();
  deleteLater();
}

void NumberWheel::onLiveClicked(LiveWindow& live)
{
  ModalWindow::onLiveClicked(live);
}

void NumberWheel::onLiveEvent(LiveWindow& live, event_t event)
{
#if defined(HARDWARE_KEYS)
  if (event == EVT_KEY_BREAK(KEY_ENTER)) { onConfirm(); return; }
  if (event == EVT_KEY_BREAK(KEY_EXIT)) { onCancel(); return; }
#endif
  Window::onLiveEvent(live, event);
}

void NumberWheel::onRollerKey(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  uint32_t key = lv_event_get_key(e);
  if (key != LV_KEY_ENTER) return;
  auto* nw = static_cast<NumberWheel*>(lv_event_get_user_data(e));
  if (nw) nw->onConfirm();
}

NumberWheel* NumberWheel::open(NumberEdit* edit)
{
  if (!edit) return nullptr;
  auto* wheel = new NumberWheel(edit);
  if (!wheel) return nullptr;

  // Create a group so hardware rotary and keys work
  lv_group_t* g = lv_group_create();
  if (g) {
    lv_group_set_editing(g, true);
    // Assign the whole roller to the group so rotary scrolls it
    if (wheel->wholeRoller) lv_group_add_obj(g, wheel->wholeRoller);
    if (wheel->decimalRoller) lv_group_add_obj(g, wheel->decimalRoller);
    wheel->assignLvGroup(g, true);
  }
  return wheel;
}
