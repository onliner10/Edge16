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
#include "numberedit.h"
#include "ds_core.h"

// The PPM frame line's control side: PPM frame length + delay edits and a
// polarity Choice, flowed side-by-side. It is embedded as the control (60%)
// column of a labelled form line by BOTH callers — module_setup (inside a
// ds::FormRow whose label is STR_PPMFRAME) and trainer_setup (inside a
// FlexGridLayout line whose col-0 StaticText is STR_PPMFRAME) — so it carries
// NO label of its own: a label-less ds::FieldGroup whose wrapping content area
// owns the layout, gaps and touch floor.
template <typename T>
struct PpmFrameSettings : public ds::FieldGroup {
  private:
    NumberEdit* ppmFrameLenEditObject = nullptr;

  public:
    PpmFrameSettings(Window* parent, T* ppm);

    NumberEdit* getPpmFrameLenEditObject() {
      return ppmFrameLenEditObject;
    };
};
