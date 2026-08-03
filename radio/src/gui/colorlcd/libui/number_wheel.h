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

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "button.h"
#include "modal_window.h"
#include "static.h"

class NumberEdit;

class NumberWheel : public ModalWindow
{
 public:
  struct Option {
    int rawValue;
    std::string label;
  };

  // Layout description for a wheel.
  // Single column: columns[0] holds absolute raw values (buildOptionsFor result).
  // Multi column (additive): the committed value is the SUM of the selected
  // rawValue in every column.  Covers both the coarse+fine split (bases +
  // offsets) and iOS-countdown-style time pickers (h*3600 + m*60 + s).
  // captions, when non-empty, holds one small header per column ("h", "min"...).
  struct WheelLayout {
    std::vector<std::vector<Option>> columns;
    std::vector<std::string> captions;
    bool split() const { return columns.size() >= 2; }
    bool valid() const { return !columns.empty(); }
  };

  NumberWheel(NumberEdit* edit);

  static NumberWheel* open(NumberEdit* edit);

  /// Close any open wheel bound to this NumberEdit without invoking its
  /// NumberArea closeHandler (field is being destroyed).
  static void detachEdit(NumberEdit* edit);

  /// Maximum acceptable option count for a single-column wheel.
  static constexpr int MAX_WHEEL_OPTIONS = 300;

  /// Returns true if the given NumberEdit can open a wheel (single or split).
  static bool canOpen(NumberEdit* edit);

  /// Builds the single-column option list for a NumberEdit.
  static std::vector<Option> buildOptionsFor(NumberEdit* edit);

  /// Builds the full layout (single or split) for a NumberEdit.
  static WheelLayout buildLayoutFor(NumberEdit* edit);

  /// Compose a raw value by summing the selected option of every column
  /// (multi-column layouts only).
  static int composeValue(const WheelLayout& l, const std::vector<int>& idxs);

  /// Decompose a raw value into one index per column (greedy largest-base,
  /// columns must hold ascending rawValues; multi-column layouts only).
  static std::vector<int> decomposeValue(const WheelLayout& l, int value);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "NumberWheel"; }
#endif

#if defined(SIMU)
  // Test-only accessors: let a gtest simulate a touch-driven roller change
  // exactly the way LVGL's roller release_handler() does it -- update the
  // selected index, then emit LV_EVENT_VALUE_CHANGED -- so the assertions
  // exercise the real onRollerChanged() path a finger drag takes, not a
  // hand-rolled shortcut around it.
  void touchRollerForTest(size_t col, int idx)
  {
    if (col >= rollers.size()) return;
    lv_roller_set_selected(rollers[col], (uint32_t)idx, LV_ANIM_OFF);
    lv_obj_send_event(rollers[col], LV_EVENT_VALUE_CHANGED, nullptr);
  }
  int getRollerSelectedForTest(size_t col) const
  {
    return (col < rollers.size()) ? (int)lv_roller_get_selected(rollers[col])
                                   : -1;
  }
  // Test-only accessor: simulate one hardware rotary-encoder detent exactly
  // the way it is delivered to a focused roller -- an LV_EVENT_KEY event
  // carrying the key code as its param -- so the assertions exercise the real
  // onWheelEncoder() preprocess handler a physical encoder detent takes (it
  // stops event processing itself, so the roller's own class handler and
  // onRollerKey() never see the event, matching production behaviour).
  void rotateEncoderForTest(size_t col, uint32_t key)
  {
    if (col >= rollers.size()) return;
    lv_obj_send_event(rollers[col], LV_EVENT_KEY, &key);
  }
#endif

 protected:
  static NumberWheel* activeWheel;

  NumberEdit* edit = nullptr;
  std::vector<lv_obj_t*> rollers;  // all rollers, in column order
  lv_obj_t* rollerObj = nullptr;   // alias of rollers[0]
  StaticText* titleLabel = nullptr;
  TextButton* cancelButton = nullptr;
  TextButton* defaultButton = nullptr;
  TextButton* okButton = nullptr;
  WheelLayout layout;
  std::vector<Option> options;  // single-column shorthand (alias of layout.columns[0])
  int originalValue = 0;
  // Raw units per one display-precision unit (e.g. 1 for a PREC1 field whose
  // fine column steps by 0.1, or getStep() for integer split fields).  For a
  // split wheel a single encoder detent moves the *composed* value by this,
  // carrying across columns, so rotary resolution matches the finest column.
  int fineStepRaw = 1;
  std::string titleText;  // base edit title, used in title refresh
  // Re-entrancy guard for the touch-path normalization pass in
  // onRollerChanged(): lv_roller_set_selected() never itself emits
  // LV_EVENT_VALUE_CHANGED (only a real press/release cycle does, via
  // LVGL's release_handler()) so this should never actually re-enter, but
  // guard explicitly so a future LVGL change -- or an indirect event bubble
  // -- can never turn normalization into a feedback loop or fight an
  // in-progress user drag.
  bool normalizingSelection = false;

  void buildContent();
  lv_obj_t* buildRollerWidget(lv_obj_t* parent, lv_coord_t w,
                               const std::string& optionsStr, int selectedIdx,
                               int visibleRows);
  void buildSingleRoller(lv_obj_t* parent, const std::string& optionsStr,
                         int selectedIdx, int visibleRows);
  void buildMultiRollers(lv_obj_t* parent);
  void onConfirm();
  void onCancel() override;
  void onDelete() override;
  int currentComposedValue() const;
  void applyCurrentSelection(bool tick);
  void previewSelection(int idx);  // single-column only; calls applyCurrentSelection
  void onLiveEvent(LiveWindow& live, event_t event) override;
  static void onRollerKey(lv_event_t* e);
  // Preprocess KEY handler for split wheels: adjusts the whole composed value by
  // one fineStepRaw per detent (with carry) so every user-paced detent is a
  // single display-precision step regardless of which column is focused.
  static void onWheelEncoder(lv_event_t* e);
  static void onRollerChanged(lv_event_t* e);
  static void onButtonKey(lv_event_t* e);
  void onLiveClicked(LiveWindow& live) override;
};
