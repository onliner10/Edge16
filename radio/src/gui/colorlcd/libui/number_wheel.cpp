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

// Base card dimensions for the 480x272 TX16S MK2 display.  Larger screens use
// the same proportions with larger touch targets, while MK2 keeps a compact fit.
constexpr lv_coord_t BASE_CARD_W = 340;
constexpr lv_coord_t BASE_CARD_H = 216;
constexpr lv_coord_t BASE_TITLE_H = 24;
constexpr lv_coord_t BASE_BTN_H = 32;
constexpr lv_coord_t BASE_MULTI_MARGIN_X = 14;

struct WheelMetrics {
  bool largeScreen = false;
  lv_coord_t cardW = BASE_CARD_W;
  lv_coord_t cardH = BASE_CARD_H;
  lv_coord_t titleH = BASE_TITLE_H;
  lv_coord_t btnH = BASE_BTN_H;
  lv_coord_t contentPad = 10;
  lv_coord_t rowGap = 8;
  lv_coord_t columnGap = BASE_MULTI_MARGIN_X;
  lv_coord_t singleRollerW = 260;
  int visibleRows = 5;
};

static WheelMetrics wheelMetrics()
{
  WheelMetrics m;
  m.largeScreen = LV_HOR_RES >= 640 || LV_VER_RES >= 400;
  if (m.largeScreen) {
    m.cardW = std::min<lv_coord_t>(560, LV_HOR_RES - 120);
    m.cardW = std::max<lv_coord_t>(BASE_CARD_W, m.cardW);
    m.cardH = std::min<lv_coord_t>(360, LV_VER_RES - 96);
    m.cardH = std::max<lv_coord_t>(BASE_CARD_H, m.cardH);
    m.titleH = 34;
    m.btnH = 44;
    m.contentPad = 24;
    m.rowGap = 14;
    m.columnGap = 18;
    m.singleRollerW = std::min<lv_coord_t>(360, m.cardW - 2 * m.contentPad);
    m.visibleRows = 7;
  }
  return m;
}

static void clearContainerStyle(lv_obj_t* obj)
{
  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

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

// iOS-countdown-style layout for time fields (raw value = seconds):
// independent hours / minutes / seconds columns, composed additively.
static NumberWheel::WheelLayout buildTimeLayout(NumberEdit* edit)
{
  int vmin = edit->getMin();
  int vmax = edit->getMax();
  // Only sensible for 0-based ranges of at least a minute; smaller ranges
  // read better as a plain single-column wheel.
  if (vmin != 0 || vmax < 60) return {};

  NumberWheel::WheelLayout l;
  char buf[8];

  if (vmax >= 3600) {
    // Hours roller capped at 99 ("99:59:59"); larger maxima (the timer field
    // allows ~2330 h) stay reachable through the keypad only — nobody dials
    // in a four-digit-hour timer on a wheel.
    int maxH = std::min(vmax / 3600, 99);
    std::vector<NumberWheel::Option> hours;
    for (int h = 0; h <= maxH; h++) hours.push_back({h * 3600, std::to_string(h)});
    l.columns.push_back(std::move(hours));
    l.captions.push_back("h");
  }

  int maxM = (vmax >= 3600) ? 59 : std::min(vmax / 60, 59);
  std::vector<NumberWheel::Option> minutes;
  for (int m = 0; m <= maxM; m++) {
    std::snprintf(buf, sizeof(buf), "%02d", m);
    minutes.push_back({m * 60, buf});
  }
  l.columns.push_back(std::move(minutes));
  l.captions.push_back("min");

  std::vector<NumberWheel::Option> seconds;
  for (int s = 0; s <= 59; s++) {
    std::snprintf(buf, sizeof(buf), "%02d", s);
    seconds.push_back({s, buf});
  }
  l.columns.push_back(std::move(seconds));
  l.captions.push_back("s");

  return l;
}

NumberWheel::WheelLayout NumberWheel::buildLayoutFor(NumberEdit* edit)
{
  if (!edit) return {};

  // Row 1: explicit time-style fields get the h/m/s picker
  if (edit->isWheelTimeStyle()) {
    auto l = buildTimeLayout(edit);
    if (l.valid()) return l;
  }

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
      // Display handlers that ignore their value argument (live readouts,
      // "current param" formatters) collapse every option into one label;
      // a one-option wheel for a multi-value range is useless, so treat the
      // field as ineligible and let it keep its keypad/inline editor.
      bool degenerate = (int)opts.size() <= 1 && count > 1;
      if (!opts.empty() && !degenerate) return WheelLayout{{opts}};
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
    // The regular loop stops at the last base whose *full* fine range fits below
    // vmax, so it drops the top base whenever vmax sits on the coarse grid
    // (e.g. range 0..1000, K*fineUnit=10: base 1000 is excluded because 1000+9 >
    // 1000).  Append a top base so vmax stays reachable.  Prefer the grid-aligned
    // base (largest multiple of K*fineUnit <= vmax): that keeps every coarse
    // label on the fine grid (all end in ".0"), so a single detent is a uniform
    // step and the highlighted row after Reset reads cleanly (100.0, not 99.1).
    // Fall back to the old vmax-(K-1)*fineUnit base only when the aligned base
    // cannot represent vmax with a fine offset (odd remainder) — that guarantees
    // vmax stays reachable for unusual ranges without changing their behaviour.
    int lastCoveredMax =
        coarse.empty() ? vmin - 1 : coarse.back().rawValue + (K - 1) * fineUnit;
    if (lastCoveredMax < vmax) {
      int span = K * fineUnit;
      int alignedTop = vmin + ((vmax - vmin) / span) * span;
      int lastBase;
      if (alignedTop > (coarse.empty() ? vmin - 1 : coarse.back().rawValue) &&
          (vmax - alignedTop) % fineUnit == 0) {
        lastBase = alignedTop;
      } else {
        lastBase = vmax - (K - 1) * fineUnit;
      }
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

int NumberWheel::composeValue(const WheelLayout& l, const std::vector<int>& idxs)
{
  int v = 0;
  for (size_t c = 0; c < l.columns.size() && c < idxs.size(); c++) {
    int i = LV_CLAMP(0, idxs[c], (int)l.columns[c].size() - 1);
    v += l.columns[c][i].rawValue;
  }
  return v;
}

std::vector<int> NumberWheel::decomposeValue(const WheelLayout& l, int value)
{
  // Greedy: per column take the largest base <= remainder.  Columns hold
  // ascending rawValues whose units divide evenly (coarse+fine, h/m/s), so
  // the greedy pick is exact whenever the value is representable.
  std::vector<int> idxs;
  int rem = value;
  for (const auto& col : l.columns) {
    int pick = 0;
    for (int j = 1; j < (int)col.size(); j++) {
      if (col[j].rawValue <= rem) pick = j;
      else break;
    }
    idxs.push_back(pick);
    rem -= col[pick].rawValue;
  }
  return idxs;
}

// ---- Construction -------------------------------------------------------

NumberWheel::NumberWheel(NumberEdit* numEdit) :
    ModalWindow(true), edit(numEdit)
{
  originalValue = edit ? edit->getValue() : 0;
  layout = buildLayoutFor(edit);

  // Raw units per one display-precision step — mirrors the fineUnit used to lay
  // out the fine column in buildLayoutFor().  A split-wheel detent moves the
  // composed value by this amount.
  if (edit) {
    if (edit->hasDecimalPrecision()) {
      fineStepRaw = edit->getPrecisionScale() / 10;
      if (fineStepRaw < 1) fineStepRaw = 1;
    } else {
      fineStepRaw = std::max(1, edit->getStep());
    }
  }

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

lv_obj_t* NumberWheel::buildRollerWidget(lv_obj_t* parent, lv_coord_t w,
                                          const std::string& optionsStr,
                                          int selectedIdx, int visibleRows)
{
  lv_obj_t* obj = lv_roller_create(parent);
  lv_obj_set_width(obj, w);
  lv_obj_set_height(obj, LV_PCT(100));
  lv_obj_set_flex_grow(obj, 1);
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
  const auto m = wheelMetrics();
  rollerObj = buildRollerWidget(parent, m.singleRollerW, optionsStr,
                                selectedIdx, visibleRows);
  if (rollerObj) rollers.push_back(rollerObj);
}

void NumberWheel::buildMultiRollers(lv_obj_t* parent)
{
  if (!layout.split()) return;
  int n = (int)layout.columns.size();
  const auto m = wheelMetrics();

  auto idxs = decomposeValue(layout, originalValue);

  auto makeOptsStr = [](const std::vector<Option>& col) {
    std::string s;
    for (size_t i = 0; i < col.size(); i++) {
      if (i > 0) s += '\n';
      s += col[i].label;
    }
    return s;
  };

  const int visibleRows = m.visibleRows;
  for (int c = 0; c < n; c++) {
    lv_obj_t* col = lv_obj_create(parent);
    if (!col) continue;
    clearContainerStyle(col);
    lv_obj_set_width(col, 0);
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, m.largeScreen ? 8 : 4, LV_PART_MAIN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    if (c < (int)layout.captions.size() && !layout.captions[c].empty()) {
      // Small unit caption above each roller, iOS-picker style.
      lv_obj_t* cap = lv_label_create(col);
      if (cap) {
        lv_label_set_text(cap, layout.captions[c].c_str());
        lv_obj_set_width(cap, LV_PCT(100));
        lv_obj_set_height(cap, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(cap, lv_color_hex(0x888888), 0);
        etx_font(cap, FONT_XS_INDEX, 0);
      }
    }

    lv_obj_t* r = buildRollerWidget(col, LV_PCT(100),
                                    makeOptsStr(layout.columns[c]),
                                    c < (int)idxs.size() ? idxs[c] : 0,
                                    visibleRows);
    if (r) {
      // Intercept encoder LEFT/RIGHT before the roller class handler so one
      // detent steps the whole composed value by fineStepRaw (with carry across
      // columns) instead of moving only the focused column by a whole index.
      lv_obj_add_event_cb(
          r, &NumberWheel::onWheelEncoder,
          (lv_event_code_t)(LV_EVENT_KEY | LV_EVENT_PREPROCESS), this);
      rollers.push_back(r);
    }
  }
  rollerObj = rollers.empty() ? nullptr : rollers[0];
}

// ---- UI construction ----------------------------------------------------

void NumberWheel::buildContent()
{
  // Dark opaque overlay
  withLive([](LiveWindow& l) {
    lv_obj_set_style_bg_color(l.lvobj(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(l.lvobj(), LV_OPA_90, 0);
  });

  const auto m = wheelMetrics();

  // Bright card, centered
  lv_obj_t* cardObj = nullptr;
  auto* card = new (std::nothrow) Window(this, rect_t{
    (LV_HOR_RES - m.cardW) / 2, (LV_VER_RES - m.cardH) / 2,
    m.cardW, m.cardH
  });
  if (!card) return;
  // Mark the card opaque so a tap anywhere inside the card's visual bounds is
  // absorbed here instead of bubbling up to the modal scrim.  Without this a
  // tap on the card background (e.g. the sliver between the roller's clipped
  // bottom edge and the button row) fell through to onLiveClicked and silently
  // committed the live-previewed roller value.
  card->setWindowFlag(OPAQUE);
  card->withLive([&](LiveWindow& l) {
    cardObj = l.lvobj();
    lv_obj_set_style_bg_color(cardObj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cardObj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cardObj, 14, 0);
    lv_obj_clear_flag(cardObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cardObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cardObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cardObj, m.contentPad, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cardObj, m.rowGap, LV_PART_MAIN);
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

  titleLabel = new (std::nothrow) StaticText(
      card, rect_t{0, 0, LV_PCT(100), m.titleH}, titleStr,
      COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD));
  titleLabel->withLive([](LiveWindow& l) {
    lv_obj_set_style_text_align(l.lvobj(), LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l.lvobj(), lv_color_black(), 0);
  });

  if (!cardObj) return;

  lv_obj_t* rollerArea = lv_obj_create(cardObj);
  if (!rollerArea) return;
  clearContainerStyle(rollerArea);
  lv_obj_set_width(rollerArea, LV_PCT(100));
  lv_obj_set_height(rollerArea, 0);
  lv_obj_set_flex_grow(rollerArea, 1);
  lv_obj_set_flex_flow(rollerArea, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rollerArea, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(rollerArea, m.columnGap, LV_PART_MAIN);

  // Build roller(s)
  if (layout.split()) {
    buildMultiRollers(rollerArea);
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

    const int visibleRows = std::min(optionCount, m.visibleRows);
    buildSingleRoller(rollerArea, optsStr, selectedIdx, visibleRows);
  }

  auto* buttonRow = new (std::nothrow) Window(card, rect_t{});
  if (!buttonRow) return;
  buttonRow->withLive([&](LiveWindow& l) {
    lv_obj_t* row = l.lvobj();
    clearContainerStyle(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, m.btnH);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, m.columnGap, LV_PART_MAIN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
  });

  auto stretchButton = [](TextButton* btn) {
    if (!btn) return;
    btn->withLive([](LiveWindow& l) {
      lv_obj_set_width(l.lvobj(), 0);
      lv_obj_set_height(l.lvobj(), LV_PCT(100));
      lv_obj_set_flex_grow(l.lvobj(), 1);
    });
  };

  // Bottom button row
  int defVal = edit->getDefault();
  bool showDefault = (edit->hasDefaultValue()
                      && defVal >= edit->getMin() && defVal <= edit->getMax()
                      && edit->isValueAvailableCheck(defVal));
  if (showDefault) {
    cancelButton = new (std::nothrow) TextButton(
        buttonRow, rect_t{}, STR_CANCEL, [this]() { onCancel(); return 0; });
    stretchButton(cancelButton);
    defaultButton = new (std::nothrow) TextButton(
        buttonRow, rect_t{}, STR_RESET, [this]() {
          if (!rollerObj) return 0;
          int dv = edit->getDefault();
          if (layout.split()) {
            auto idxs = decomposeValue(layout, dv);
            for (size_t c = 0; c < rollers.size() && c < idxs.size(); c++)
              lv_roller_set_selected(rollers[c], idxs[c], LV_ANIM_ON);
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
    stretchButton(defaultButton);
    okButton = new (std::nothrow) TextButton(
        buttonRow, rect_t{}, STR_OK, [this]() { onConfirm(); return 0; });
    stretchButton(okButton);
  } else {
    cancelButton = new (std::nothrow) TextButton(
        buttonRow, rect_t{}, STR_CANCEL, [this]() { onCancel(); return 0; });
    stretchButton(cancelButton);
    okButton = new (std::nothrow) TextButton(
        buttonRow, rect_t{}, STR_OK, [this]() { onConfirm(); return 0; });
    stretchButton(okButton);
  }
}

// ---- Value helpers -------------------------------------------------------

int NumberWheel::currentComposedValue() const
{
  if (!rollerObj) return originalValue;
  if (layout.split()) {
    if (rollers.size() != layout.columns.size()) return originalValue;
    std::vector<int> idxs;
    idxs.reserve(rollers.size());
    for (auto* r : rollers) idxs.push_back((int)lv_roller_get_selected(r));
    return composeValue(layout, idxs);
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
    // Refresh the title to show the composed value.  A grid-aligned top coarse
    // base can compose slightly past vmax if the fine column is touch-scrolled
    // at the very top; setValue() already clamps the stored value, so clamp the
    // displayed value too instead of briefly showing an out-of-range number.
    int shown = LV_CLAMP(edit->getMin(), val, edit->getMax());
    // Use += to avoid operator+(string&&,string&&) chains that confuse GCC -fanalyzer.
    std::string text = titleText;
    text += " \xe2\x80\x94 ";
    text += edit->getDisplayValFor(shown);
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
  // Reached only for a tap on the dark scrim OUTSIDE the card (taps inside the
  // card are absorbed by the opaque card window).  A tap outside a modal is a
  // CANCEL: restore the exact pre-open value, identical to EXIT / the Cancel
  // button.  Committing here (the previous behaviour) meant an accidental
  // near-miss saved whatever value the live preview happened to be showing —
  // on an RC transmitter a mis-tap must never write an unintended value.
  onCancel();
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
  // Re-entrancy guard: the normalization pass below (re-)calls
  // lv_roller_set_selected() on one or more columns.  That call never itself
  // emits LV_EVENT_VALUE_CHANGED -- same property onWheelEncoder already
  // relies on to silently re-select rollers -- so this should never actually
  // re-enter, but bail out defensively anyway so it can never loop or fight
  // an in-progress user drag.
  if (nw->normalizingSelection) return;

  // LVGL's roller widget calls lv_group_set_editing(g, false) in its own
  // VALUE_CHANGED handler (runs before ours) whenever an encoder drives a
  // value change.  Re-assert editing=true so the next encoder rotation batch
  // still adjusts the roller instead of cycling group focus.  This is also
  // harmless for touch-scroll VALUE_CHANGED (editing is already true, the
  // lv_group_set_editing no-op check makes it free).
  lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
  lv_group_t* g = lv_obj_get_group(roller);
  if (g) lv_group_set_editing(g, true);

  // A touch drag on ONE column can compose past vmin/vmax without that
  // column knowing it -- e.g. a grid-aligned top coarse base already equals
  // vmax, so scrolling the fine roller to any nonzero offset composes past
  // vmax, or dragging the coarse roller up onto that same top base while the
  // fine roller sits on a nonzero offset overshoots the same way.  The naive
  // fix -- always clamp the stored value then re-derive EVERY column's
  // selection from the canonical decomposition of that clamped number, the
  // way onWheelEncoder always does -- has a bad side effect when applied to a
  // touch drag: the canonical decomposition of vmax always picks fine offset
  // 0, so a coarse-only drag that overshoots silently WIPES a fine offset the
  // user had dialled in (e.g. dragging coarse up through the ceiling with
  // fine at "+.6" reset it to "+.0", and dragging coarse back down one step
  // then landed on X9.0 instead of X9.6 -- the .6 was gone for good).
  //
  // Fix: identify the column that JUST moved (the event's own target) and,
  // if the drag overshot, slide ONLY that column's own selection back to the
  // furthest index that keeps the composed value in range -- leaving every
  // OTHER column's selection, in particular the fine offset, untouched. This
  // is the same "clamp the coarse roller's reachable range instead of
  // resetting fine" behaviour applied symmetrically to whichever column the
  // user is actually touching. Columns hold ascending rawValues that start at
  // 0 (fine/minute/second offsets) or vmin (the coarse base column), so the
  // changed column's own zero/vmin-ward end is always in range and sliding
  // toward it can always find a fix by construction; the full
  // re-decomposition fallback below exists only to stay provably safe even if
  // that invariant is ever violated by an unusual layout.
  bool handled = true;
  if (nw->layout.split() && nw->edit &&
      nw->rollers.size() == nw->layout.columns.size()) {
    size_t changedCol = nw->rollers.size();
    for (size_t c = 0; c < nw->rollers.size(); c++) {
      if (nw->rollers[c] == roller) { changedCol = c; break; }
    }

    if (changedCol < nw->rollers.size()) {
      int vmin = nw->edit->getMin();
      int vmax = nw->edit->getMax();
      int composed = nw->currentComposedValue();
      if (composed < vmin || composed > vmax) {
        const auto& col = nw->layout.columns[changedCol];
        int curIdx = LV_CLAMP(
            0, (int)lv_roller_get_selected(nw->rollers[changedCol]),
            (int)col.size() - 1);
        // Contribution of every OTHER column, held fixed while we search.
        int otherSum = composed - col[curIdx].rawValue;
        int best = -1;
        if (composed > vmax) {
          // Slide down: rawValue is ascending, so the first index below
          // curIdx that composes in range is the furthest (largest) base
          // reachable without exceeding vmax.
          for (int i = curIdx - 1; i >= 0; i--) {
            int v = otherSum + col[i].rawValue;
            if (v >= vmin && v <= vmax) { best = i; break; }
          }
        } else {
          // composed < vmin: slide up, symmetrically.
          for (int i = curIdx + 1; i < (int)col.size(); i++) {
            int v = otherSum + col[i].rawValue;
            if (v >= vmin && v <= vmax) { best = i; break; }
          }
        }
        if (best >= 0) {
          nw->normalizingSelection = true;
          lv_roller_set_selected(nw->rollers[changedCol], (uint32_t)best,
                                  LV_ANIM_OFF);
          nw->normalizingSelection = false;
        } else {
          handled = false;
        }
      }
    }
  }

  // Commit the (now in-range, unless the fallback below still needs to run)
  // composed value from the current roller selections.
  nw->applyCurrentSelection(true);

  if (!handled) {
    // The changed column's own range could not bring the composed value back
    // in range by itself -- should not happen given how columns are built,
    // kept only so this stays provably safe regardless.
    // applyCurrentSelection() above already clamped the STORED value via
    // NumberEdit::setValue(); fall back to re-deriving every column's
    // selection from that clamped value, exactly as before this change.
    int stored = nw->edit->getValue();
    auto idxs = decomposeValue(nw->layout, stored);
    nw->normalizingSelection = true;
    for (size_t c = 0; c < nw->rollers.size() && c < idxs.size(); c++) {
      if ((int)lv_roller_get_selected(nw->rollers[c]) != idxs[c])
        lv_roller_set_selected(nw->rollers[c], idxs[c], LV_ANIM_OFF);
    }
    nw->normalizingSelection = false;
  }
}

void NumberWheel::onWheelEncoder(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  uint32_t key = lv_event_get_key(e);
  // Only handle rotation keys here; ENTER / ESC fall through to the roller class
  // handler and onRollerKey (two-step confirm, cancel) unchanged.
  if (key != LV_KEY_LEFT && key != LV_KEY_RIGHT && key != LV_KEY_UP &&
      key != LV_KEY_DOWN)
    return;

  auto* nw = static_cast<NumberWheel*>(lv_event_get_user_data(e));
  if (!nw || !nw->edit || !nw->layout.split()) return;

  int dir = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) ? 1 : -1;
  int step = nw->fineStepRaw > 0 ? nw->fineStepRaw : 1;

  // Match the inline-editor acceleration feel: extra steps for a fast spin, but
  // always a whole number of fineStepRaw units so we can never land off the
  // display-precision grid (no sticky ".9" residue).
  int accel = (rotaryEncoderGetAccel() * nw->edit->getAccelFactor()) / 8;
  if (accel < 0) accel = 0;
  int units = 1 + accel;

  int cur = nw->currentComposedValue();
  int target =
      LV_CLAMP(nw->edit->getMin(), cur + dir * units * step, nw->edit->getMax());
  if (target != cur) {
    auto idxs = decomposeValue(nw->layout, target);
    // set_selected does not emit VALUE_CHANGED, so update every column silently
    // then apply once (single tick, one title/model refresh).
    for (size_t c = 0; c < nw->rollers.size() && c < idxs.size(); c++)
      lv_roller_set_selected(nw->rollers[c], idxs[c], LV_ANIM_OFF);
  }
  nw->applyCurrentSelection(true);

  // We fully handled this detent: stop before the roller class handler so it
  // does not also move the focused column by one whole index.
  lv_event_stop_processing(e);
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
      // Tab order: each roller hands focus to the next column, the last
      // column hands it to the OK button.
      for (size_t i = 0; i + 1 < nw->rollers.size(); i++) {
        if (roller == nw->rollers[i]) {
          next = nw->rollers[i + 1];
          break;
        }
      }
      if (!next && nw->okButton) {
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
    for (auto* r : wheel->rollers)
      if (r) lv_group_add_obj(g, r);
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
