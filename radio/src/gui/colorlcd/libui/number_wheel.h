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

#include "button.h"
#include "modal_window.h"
#include "static.h"

class NumberEdit;

class NumberWheel : public ModalWindow
{
 public:
  NumberWheel(NumberEdit* edit);

  static NumberWheel* open(NumberEdit* edit);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "NumberWheel"; }
#endif

 protected:
  NumberEdit* edit = nullptr;
  lv_obj_t* wholeRoller = nullptr;
  lv_obj_t* decimalRoller = nullptr;
  StaticText* titleLabel = nullptr;
  TextButton* cancelButton = nullptr;
  TextButton* okButton = nullptr;
  bool hasDecimal = false;
  int precisionScale = 1;

  int wholeCount() const;
  int decimalCount() const;
  int wholeMin() const;
  int decimalMin() const;

  void buildContent();
  void buildRoller(lv_obj_t* roller, int count);
  void buildRollerOptions(lv_obj_t* roller, int count,
                          std::function<std::string(int)> fmt);
  void selectValue(lv_obj_t* roller, int count, int cur);
  void onConfirm();
  void onCancel() override;
  void onLiveEvent(LiveWindow& live, event_t event) override;
  static void onRollerKey(lv_event_t* e);
  void onLiveClicked(LiveWindow& live) override;
};
