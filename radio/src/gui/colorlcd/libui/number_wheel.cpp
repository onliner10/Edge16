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
#include <climits>
#include <cstdio>
#include <string>

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "hal/rotary_encoder.h"
#include "keys.h"
#include "mainwindow.h"
#include "numberedit.h"

// Padded card dimensions — fat-finger friendly
constexpr lv_coord_t CARD_W = 340;
constexpr lv_coord_t CARD_H = 200;
constexpr lv_coord_t TITLE_H = 24;
constexpr lv_coord_t ROLLER_H = 130;
constexpr lv_coord_t BTN_H = 32;

// Split-wheel roller geometry (x, width of each column)
constexpr lv_coord_t SPLIT_COARSE_X = 14;
constexpr lv_coord_t SPLIT_FINE_X = 176;
constexpr lv_coord_t SPLIT_ROLLER_W = 150;

// ---- Static layout logic ------------------------------------------------

std::vector<NumberWheel::Option> NumberWheel::buildOptionsFor(NumberEdit* edit)
{
  if (!edit) return {};

  int min = edit->getMin();
  int max = edit->getMax();
  int step = edit->getStep();
  if (step < 1) step = 1;

  // First pass: every valid raw value with its display label.
  std::vector<Option> all;
  for (int raw = min; raw <= max; raw += step) {
    if (!edit->isValueAvailableCheck(raw)) continue;
    all.push_back({raw, edit->getDisplayValFor(raw)});
  }

  // Second pass: group consecutive entries with the same label and keep the
  // ceiling-median of each group.  For PREC2 fields this ensures "0.1s" stores
  // raw 10 (0.10 s) rather than raw 5 (0.05 s, the first value whose truncated
  // display rounds to "0.1").
  std::vector<Option> opts;
  for (size_t i = 0; i < all.size(); ) {
    size_t j = i + 1;
    while (j < all.size() && all[j].label == all[i].label) j++;
    opts.push_back(all[(i + j) / 2]);
    i = j;
  }
  return opts;
}

NumberWheel::WheelLayout NumberWheel::buildLayoutFor(NumberEdit* edit)
{
  if (!edit) return {};

  // Row 2: try single column first (cheap, existing path)
  {
    int step = edit->getStep();
    if (step < 1) step = 1;
    int count = 0;
    bool feasible = true;
    for (int raw = edit->getMin(); raw <= edit->getMax(); raw += step) {
      if (!edit->isValueAvailableCheck(raw)) continue;
      if (++count > MAX_WHEEL_OPTIONS) { feasible = false; break; }
    }
    if (feasible && count > 0) {
      auto opts = buildOptionsFor(edit);
      if (!opts.empty()) return WheelLayout{{opts}};
    }
  }

  // Row 3/5: custom display or availability handler → no split
  if (edit->hasDisplayFunction() || edit->hasAvailableHandler()) return {};

  // Row 4: try additive split (K=10 first, then K=100)
  int vmin = edit->getMin();
  int vmax = edit->getMax();
  int fineUnit;
  if (edit->hasDecimalPrecision()) {
    fineUnit = edit->getPrecisionScale() / 10;
    if (fineUnit < 1) fineUnit = 1;
  } else {
    fineUnit = std::max(1, edit->getStep());
  }

  for (int K : {10, 100}) {
    // Build coarse bases: b_j = vmin + j*K*fineUnit while b_j+(K-1)*fineUnit <= vmax
    std::vector<Option> coarse;
    for (int j = 0; ; j++) {
      int base = vmin + j * K * fineUnit;
      if (base + (K - 1) * fineUnit > vmax) break;
      coarse.push_back({base, edit->getDisplayValFor(base)});
    }
    // Append overlapping last base so vmax is reachable
    int lastCoveredMax =
        coarse.empty() ? vmin - 1 : coarse.back().rawValue + (K - 1) * fineUnit;
    if (lastCoveredMax < vmax) {
      int lastBase = vmax - (K - 1) * fineUnit;
      coarse.push_back({lastBase, edit->getDisplayValFor(lastBase)});
    }

    if (coarse.empty() || (int)coarse.size() > 350) continue;

    // Build fine options: additive offsets 0 .. (K-1)*fineUnit
    bool isDecimal = edit->hasDecimalPrecision();
    std::vector<Option> fine;
    for (int i = 0; i < K; i++) {
      int offset = i * fineUnit;
      std::string label;
      if (isDecimal) {
        // "+.0" .. "+.9"  (one decimal digit regardless of K; K≤10 for decimals)
        label = "+." + std::to_string(i);
      } else if (K == 100) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "+%02d", offset);
        label = buf;
      } else {
        label = "+" + std::to_string(offset);
      }
      fine.push_back({offset, label});
    }

    return WheelLayout{{coarse, fine}};
  }

  return {};  // no split feasible
}

bool NumberWheel::canOpen(NumberEdit* edit)
{
  return buildLayoutFor(edit).valid();
}

int NumberWheel::composeValue(const WheelLayout& l, int coarseIdx, int fineIdx)
{
  return l.columns[0][coarseIdx].rawValue + l.columns[1][fineIdx].rawValue;
}

std::pair<int, int> NumberWheel::decomposeValue(const WheelLayout& l, int value)
{
  const auto& coarse = l.columns[0];
  const auto& fine = l.columns[1];
  int K = (int)fine.size();

  // Largest coarse index whose base <= value
  int ci = 0;
  for (int j = 1; j < (int)coarse.size(); j++) {
    if (coarse[j].rawValue <= value) ci = j;
    else break;
  }

  int fineUnit = (K > 1) ? fine[1].rawValue : 1;
  int offset = value - coarse[ci].rawValue;
  int fi = (fineUnit > 0) ? LV_CLAMP(0, offset / fineUnit, K - 1) : 0;

  return {ci, fi};
}

// ---- Construction -------------------------------------------------------

NumberWheel::NumberWheel(NumberEdit* numEdit) :
    ModalWindow(true), edit(numEdit)
{
  originalValue = edit ? edit->getValue() : 0;
  layout = buildLayoutFor(edit);

  if (layout.valid() && !layout.split()) {
    options = layout.columns[0];
  }

  // Guard: should never be called if canOpen() returned false
  if (!layout.valid()) {
    int curVal = edit ? edit->getValue() : 0;
    std::string lbl = edit ? edit->getDisplayValFor(curVal) : "0";
    options.push_back({curVal, lbl});
    layout = WheelLayout{{options}};
  }

  buildContent();
}

// ---- Roller widget helper -----------------------------------------------

lv_obj_t* NumberWheel::buildRollerWidget(lv_obj_t* parent, lv_coord_t x,
                                          lv_coord_t w,
                                          const std::string& optionsStr,
                                          int selectedIdx, int visibleRows)
{
  lv_obj_t* obj = lv_roller_create(parent);
  lv_obj_set_pos(obj, x, 40);
  lv_obj_set_size(obj, w, ROLLER_H);
  etx_font(obj, FONT_STD_INDEX, 0);
  lv_roller_set_options(obj, optionsStr.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(obj, visibleRows);
  lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(obj, lv_color_hex(0x999999), LV_PART_MAIN);
  lv_obj_set_style_text_color(obj, lv_color_black(), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_BLUE, 3),
                             LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(obj, LV_OPA_30, LV_PART_SELECTED);
  lv_obj_set_style_radius(obj, 8, LV_PART_SELECTED);
  lv_obj_add_event_cb(obj, &NumberWheel::onRollerKey, LV_EVENT_KEY, this);
  // Set initial selection BEFORE registering VALUE_CHANGED to avoid a spurious
  // live-preview beep on open.
  int cnt = (int)lv_roller_get_option_count(obj);
  if (selectedIdx >= 0 && selectedIdx < cnt)
    lv_roller_set_selected(obj, selectedIdx, LV_ANIM_OFF);
  lv_obj_add_event_cb(obj, &NumberWheel::onRollerChanged, LV_EVENT_VALUE_CHANGED,
                      this);
  return obj;
}

void NumberWheel::buildSingleRoller(lv_obj_t* parent, const std::string& optionsStr,
                                    int selectedIdx, int visibleRows)
{
  int optionCount = static_cast<int>(options.size());
  if (optionCount == 0) return;
  rollerObj = buildRollerWidget(parent, (CARD_W - 260) / 2, 260, optionsStr,
                                selectedIdx, visibleRows);
}

void NumberWheel::buildSplitRollers(lv_obj_t* parent)
{
  if (!layout.split()) return;
  const auto& coarse = layout.columns[0];
  const auto& fine = layout.columns[1];

  auto [ci, fi] = decomposeValue(layout, originalValue);

  auto makeOptsStr = [](const std::vector<Option>& col) {
    std::string s;
    for (size_t i = 0; i < col.size(); i++) {
      if (i > 0) s += '\n';
      s += col[i].label;
    }
    return s;
  };

  const int visibleRows = 5;
  rollerObj = buildRollerWidget(parent, SPLIT_COARSE_X, SPLIT_ROLLER_W,
                                makeOptsStr(coarse), ci, visibleRows);
  fineRollerObj = buildRollerWidget(parent, SPLIT_FINE_X, SPLIT_ROLLER_W,
                                    makeOptsStr(fine), fi, visibleRows);
}

// ---- UI construction ----------------------------------------------------

void NumberWheel::buildContent()
{
  // Dark opaque overlay
  withLive([](LiveWindow& l) {
    lv_obj_set_style_bg_color(l.lvobj(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(l.lvobj(), LV_OPA_90, 0);
  });

  // Bright card, centered
  lv_obj_t* cardObj = nullptr;
  auto* card = new (std::nothrow) Window(this, rect_t{
    (LV_HOR_RES - CARD_W) / 2, (LV_VER_RES - CARD_H) / 2,
    CARD_W, CARD_H
  });
  if (!card) return;
  card->withLive([&](LiveWindow& l) {
    cardObj = l.lvobj();
    lv_obj_set_style_bg_color(cardObj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cardObj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cardObj, 14, 0);
    lv_obj_clear_flag(cardObj, LV_OBJ_FLAG_SCROLLABLE);
  });

  // Title — base text stored for live refresh
  titleText = edit->getEditTitle();
  if (titleText.empty()) titleText = STR_VALUE;

  std::string titleStr = titleText;
  if (layout.split()) {
    // Show composed value immediately in title
    titleStr += " \xe2\x80\x94 ";
    titleStr += edit->getDisplayValFor(originalValue);
  }

  titleLabel = new (std::nothrow) StaticText(card, rect_t{0, 6, CARD_W, TITLE_H},
      titleStr, COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD));
  titleLabel->withLive([](LiveWindow& l) {
    lv_obj_set_style_text_align(l.lvobj(), LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l.lvobj(), lv_color_black(), 0);
  });

  if (!cardObj) return;

  // Build roller(s)
  if (layout.split()) {
    buildSplitRollers(cardObj);
  } else {
    std::string optsStr;
    int curVal = edit->getValue();
    int selectedIdx = 0;
    int optionCount = static_cast<int>(options.size());
    int nearestDist = INT_MAX;

    for (int i = 0; i < optionCount; i++) {
      if (i > 0) optsStr += '\n';
      optsStr += options[i].label;
      int dist = std::abs(options[i].rawValue - curVal);
      if (dist < nearestDist) {
        nearestDist = dist;
        selectedIdx = i;
      }
    }

    const int visibleRows = std::min(optionCount, 5);
    buildSingleRoller(cardObj, optsStr, selectedIdx, visibleRows);
  }

  // Bottom button row
  int defVal = edit->getDefault();
  bool showDefault = (edit->hasDefaultValue()
                      && defVal >= edit->getMin() && defVal <= edit->getMax()
                      && edit->isValueAvailableCheck(defVal));
  const lv_coord_t btnY = CARD_H - BTN_H - 6;
  if (showDefault) {
    constexpr lv_coord_t BW = 96, GAP = 16, MARGIN = 10;
    cancelButton = new (std::nothrow) TextButton(card,
        rect_t{MARGIN, btnY, BW, BTN_H},
        STR_CANCEL, [this]() { onCancel(); return 0; });
    defaultButton = new (std::nothrow) TextButton(card,
        rect_t{MARGIN + BW + GAP, btnY, BW, BTN_H},
        STR_RESET, [this]() {
          if (!rollerObj) return 0;
          int dv = edit->getDefault();
          if (layout.split()) {
            auto [ci, fi] = decomposeValue(layout, dv);
            lv_roller_set_selected(rollerObj, ci, LV_ANIM_ON);
            if (fineRollerObj)
              lv_roller_set_selected(fineRollerObj, fi, LV_ANIM_ON);
          } else {
            int nearestIdx = 0, nearestDist = INT_MAX;
            for (int i = 0; i < (int)options.size(); i++) {
              int dist = std::abs(options[i].rawValue - dv);
              if (dist < nearestDist) { nearestDist = dist; nearestIdx = i; }
            }
            // set_selected sends no VALUE_CHANGED — preview explicitly
            lv_roller_set_selected(rollerObj, nearestIdx, LV_ANIM_ON);
          }
          applyCurrentSelection(true);
          return 0;
        });
    okButton = new (std::nothrow) TextButton(card,
        rect_t{MARGIN + 2 * (BW + GAP), btnY, BW, BTN_H},
        STR_OK, [this]() { onConfirm(); return 0; });
  } else {
    cancelButton = new (std::nothrow) TextButton(card,
        rect_t{16, btnY, 140, BTN_H},
        STR_CANCEL, [this]() { onCancel(); return 0; });
    okButton = new (std::nothrow) TextButton(card,
        rect_t{CARD_W - 156, btnY, 140, BTN_H},
        STR_OK, [this]() { onConfirm(); return 0; });
  }
}

// ---- Value helpers -------------------------------------------------------

int NumberWheel::currentComposedValue() const
{
  if (!rollerObj) return originalValue;
  if (layout.split()) {
    if (!fineRollerObj) return originalValue;
    return composeValue(layout,
                        (int)lv_roller_get_selected(rollerObj),
                        (int)lv_roller_get_selected(fineRollerObj));
  }
  int idx = (int)lv_roller_get_selected(rollerObj);
  if (idx < 0 || idx >= (int)options.size()) return originalValue;
  return options[idx].rawValue;
}

void NumberWheel::applyCurrentSelection(bool tick)
{
  if (!edit) return;
  int val = currentComposedValue();
  edit->setValue(val);
  if (titleLabel && layout.split()) {
    // Refresh the title to show the composed value
    // Use += to avoid operator+(string&&,string&&) chains that confuse GCC -fanalyzer.
    std::string text = titleText;
    text += " \xe2\x80\x94 ";
    text += edit->getDisplayValFor(val);
    titleLabel->withLive([&](LiveWindow& l) {
      lv_label_set_text(l.lvobj(), text.c_str());
    });
  }
  if (tick) audioKeyPress();
}

void NumberWheel::previewSelection(int idx)
{
  if (!edit) return;
  if (idx < 0 || idx >= (int)options.size()) return;
  edit->setValue(options[idx].rawValue);
  audioKeyPress();
}

// ---- Confirm / cancel ---------------------------------------------------

void NumberWheel::onConfirm()
{
  // If the roller never got built, close via cancel so the modal can't stick.
  if (!edit || !rollerObj) {
    onCancel();
    return;
  }

  edit->setValue(currentComposedValue());

  // Move the handler out before calling so deleteLater() does not call it again.
  auto handler = std::move(closeHandler);
  // Reset indev state so the ENTER key release that follows doesn't trigger
  // a click on the re-focused parent button and re-open the wheel.
  lv_indev_reset(nullptr, nullptr);
  if (handler) handler();
  deleteLater();
}

void NumberWheel::onCancel()
{
  // Restore the model value if live preview changed it while scrolling.
  if (edit && edit->getValue() != originalValue) edit->setValue(originalValue);
  auto handler = std::move(closeHandler);
  lv_indev_reset(nullptr, nullptr);
  if (handler) handler();
  deleteLater();
}

void NumberWheel::onLiveClicked(LiveWindow&)
{
  // Tapping outside the card commits.  Live preview has already applied the
  // scrolled value to the model, so silently reverting here would surprise a
  // pilot who just watched the value take effect.  EXIT / Cancel remain the
  // explicit revert paths.
  onConfirm();
}

void NumberWheel::onLiveEvent(LiveWindow& live, event_t event)
{
#if defined(HARDWARE_KEYS)
  // Reached only when the LVGL group has no focused object (keyboardDriverRead
  // routes keys here only in that case).  Normal ENTER handling happens in
  // onRollerKey / via the keypad indev clicking the focused button.
  if (event == EVT_KEY_BREAK(KEY_ENTER)) { onConfirm(); return; }
  if (event == EVT_KEY_BREAK(KEY_EXIT)) { onCancel(); return; }
#endif
  Window::onLiveEvent(live, event);
}

// ---- LVGL event callbacks -----------------------------------------------

void NumberWheel::onRollerChanged(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  auto* nw = static_cast<NumberWheel*>(lv_event_get_user_data(e));
  if (!nw) return;
  // LVGL's roller widget calls lv_group_set_editing(g, false) in its own
  // VALUE_CHANGED handler (runs before ours) whenever an encoder drives a
  // value change.  Re-assert editing=true so the next encoder rotation batch
  // still adjusts the roller instead of cycling group focus.  This is also
  // harmless for touch-scroll VALUE_CHANGED (editing is already true, the
  // lv_group_set_editing no-op check makes it free).
  lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
  lv_group_t* g = lv_obj_get_group(roller);
  if (g) lv_group_set_editing(g, true);
  nw->applyCurrentSelection(true);
}

void NumberWheel::onRollerKey(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  uint32_t key = lv_event_get_key(e);
  auto* nw = static_cast<NumberWheel*>(lv_event_get_user_data(e));
  if (!nw) return;

  if (key == LV_KEY_ESC) {
    nw->onCancel();
  } else if (key == LV_KEY_ENTER) {
    // Encoder/keypad click while a roller is focused (the keypad indev sends
    // LV_KEY_ENTER to the focused object on press).  Two-step confirm: move
    // focus to the fine roller (split coarse) or to the OK button, so the
    // next click activates a button instead of committing immediately.
    lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_group_t* g = roller ? lv_obj_get_group(roller) : nullptr;

    lv_obj_t* next = nullptr;
    if (g) {
      if (nw->layout.split() && roller == nw->rollerObj && nw->fineRollerObj) {
        next = nw->fineRollerObj;
      } else if (nw->okButton) {
        nw->okButton->withLive([&](LiveWindow& l) { next = l.lvobj(); });
      }
    }
    if (!next) {
      nw->onConfirm();  // no group/buttons — fall back to immediate commit
      return;
    }

    lv_group_focus_obj(next);
    // lv_group_focus_obj clears editing; restore it so encoder rotation keeps
    // sending LEFT/RIGHT to the focused object instead of cycling group focus.
    lv_group_set_editing(g, true);

    // We are inside the keypad indev's ENTER *press* processing.  Without
    // these, the press would leave the roller in pressed state and the
    // release would CLICK the newly focused object.
    lv_indev_t* indev = lv_indev_active();
    if (indev) {
      lv_indev_wait_release(indev);
      lv_indev_reset(indev, nullptr);
    }
  } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT ||
             key == LV_KEY_UP || key == LV_KEY_DOWN) {
    // The roller class handler already moved the selection by 1 (guardrail #3).
    // Apply additional accelerated steps to match inline-edit feel.
    int dir = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) ? 1 : -1;
    lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (roller && nw->edit) {
      int extra = (rotaryEncoderGetAccel() * nw->edit->getAccelFactor()) / 8;
      if (extra > 0) {
        int cur = (int)lv_roller_get_selected(roller);
        int cnt = (int)lv_roller_get_option_count(roller);
        int next = LV_CLAMP(0, cur + dir * extra, cnt - 1);
        if (next != cur)
          lv_roller_set_selected(roller, next, LV_ANIM_OFF);
      }
    }
    nw->applyCurrentSelection(true);
  }
}

// ---- Button key handler: encoder click activates, rotation cycles buttons --

void NumberWheel::onButtonKey(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  uint32_t key = lv_event_get_key(e);
  auto* nw = static_cast<NumberWheel*>(lv_event_get_user_data(e));
  if (!nw) return;

  lv_obj_t* cur = static_cast<lv_obj_t*>(lv_event_get_target(e));

  if (key == LV_KEY_ESC) {
    nw->onCancel();
  } else if (key == LV_KEY_LEFT || key == LV_KEY_UP ||
             key == LV_KEY_RIGHT || key == LV_KEY_DOWN) {
    // Cycle focus among the three buttons (skip nullptr entries).
    lv_obj_t* btns[3] = {};
    int nbtns = 0;
    auto collect = [&](TextButton* b) {
      if (b) b->withLive([&](LiveWindow& l) { btns[nbtns++] = l.lvobj(); });
    };
    collect(nw->cancelButton);
    collect(nw->defaultButton);
    collect(nw->okButton);
    int idx = -1;
    for (int i = 0; i < nbtns; i++) {
      if (btns[i] == cur) { idx = i; break; }
    }
    if (idx < 0 || nbtns < 2) return;
    int dir = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) ? 1 : -1;
    lv_obj_t* nextBtn = btns[(idx + dir + nbtns) % nbtns];
    lv_group_focus_obj(nextBtn);
    // lv_group_focus_obj clears editing; restore it so the next encoder detent
    // keeps cycling buttons instead of moving group focus back to the roller.
    lv_group_t* g = lv_obj_get_group(nextBtn);
    if (g) lv_group_set_editing(g, true);
  }
}

// ---- open() -------------------------------------------------------------

NumberWheel* NumberWheel::open(NumberEdit* edit)
{
  if (!edit) return nullptr;
  if (!canOpen(edit)) return nullptr;

  auto* wheel = new (std::nothrow) NumberWheel(edit);
  if (!wheel) return nullptr;

  // Create a group so hardware rotary and keys work
  lv_group_t* g = lv_group_create();
  if (g) {
    lv_group_set_editing(g, true);
    if (wheel->rollerObj) lv_group_add_obj(g, wheel->rollerObj);
    if (wheel->fineRollerObj) lv_group_add_obj(g, wheel->fineRollerObj);
    // Add buttons so encoder click (from roller) can focus and activate them.
    auto attachBtn = [g, wheel](TextButton* btn) {
      if (!btn) return;
      btn->withLive([g, wheel](LiveWindow& l) {
        lv_group_add_obj(g, l.lvobj());
        lv_obj_add_event_cb(l.lvobj(), &NumberWheel::onButtonKey,
                            LV_EVENT_KEY, wheel);
      });
    };
    attachBtn(wheel->cancelButton);
    attachBtn(wheel->defaultButton);
    attachBtn(wheel->okButton);
    wheel->assignLvGroup(g, true);
  }
  return wheel;
}
