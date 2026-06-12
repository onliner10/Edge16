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
  // Single column: columns[0] holds absolute raw values (existing buildOptionsFor result).
  // Two columns: columns[0] = coarse bases (absolute), columns[1] = fine offsets (additive).
  struct WheelLayout {
    std::vector<std::vector<Option>> columns;
    bool split() const { return columns.size() == 2; }
    bool valid() const { return !columns.empty(); }
  };

  NumberWheel(NumberEdit* edit);

  static NumberWheel* open(NumberEdit* edit);

  /// Maximum acceptable option count for a single-column wheel.
  static constexpr int MAX_WHEEL_OPTIONS = 300;

  /// Returns true if the given NumberEdit can open a wheel (single or split).
  static bool canOpen(NumberEdit* edit);

  /// Builds the single-column option list for a NumberEdit.
  static std::vector<Option> buildOptionsFor(NumberEdit* edit);

  /// Builds the full layout (single or split) for a NumberEdit.
  static WheelLayout buildLayoutFor(NumberEdit* edit);

  /// Compose a raw value from coarse and fine roller indices (split layout only).
  static int composeValue(const WheelLayout& l, int coarseIdx, int fineIdx);

  /// Decompose a raw value into {coarseIdx, fineIdx} (split layout only).
  static std::pair<int, int> decomposeValue(const WheelLayout& l, int value);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "NumberWheel"; }
#endif

 protected:
  NumberEdit* edit = nullptr;
  lv_obj_t* rollerObj = nullptr;      // single: the only roller; split: coarse roller
  lv_obj_t* fineRollerObj = nullptr;  // split only: fine roller
  StaticText* titleLabel = nullptr;
  TextButton* cancelButton = nullptr;
  TextButton* defaultButton = nullptr;
  TextButton* okButton = nullptr;
  WheelLayout layout;
  std::vector<Option> options;  // single-column shorthand (alias of layout.columns[0])
  int originalValue = 0;
  std::string titleText;  // base edit title, used in title refresh

  void buildContent();
  lv_obj_t* buildRollerWidget(lv_obj_t* parent, lv_coord_t x, lv_coord_t w,
                               const std::string& optionsStr, int selectedIdx,
                               int visibleRows);
  void buildSingleRoller(lv_obj_t* parent, const std::string& optionsStr,
                         int selectedIdx, int visibleRows);
  void buildSplitRollers(lv_obj_t* parent);
  void onConfirm();
  void onCancel() override;
  int currentComposedValue() const;
  void applyCurrentSelection(bool tick);
  void previewSelection(int idx);  // single-column only; calls applyCurrentSelection
  void onLiveEvent(LiveWindow& live, event_t event) override;
  static void onRollerKey(lv_event_t* e);
  static void onRollerChanged(lv_event_t* e);
  void onLiveClicked(LiveWindow& live) override;
};
