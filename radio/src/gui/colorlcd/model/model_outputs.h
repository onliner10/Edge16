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

#include <vector>

#include "pagegroup.h"
#include "route.h"

class OutputLineButton;

class ModelOutputsPage : public PageGroupItem
{
 public:
  ModelOutputsPage(const PageDef& pageDef);

  bool openRoute(const Route& r, uint8_t depth) override;

  void build(Window* window) override;

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): all row/header geometry is owned by
  // the ds:: layer (ds::List, ds::FormRow, ds::rowHeight). No per-screen
  // coordinate constants live here anymore.

 protected:
  std::vector<OutputLineButton*> outputButtons;

  void editOutput(uint8_t channel, OutputLineButton* btn);
};
