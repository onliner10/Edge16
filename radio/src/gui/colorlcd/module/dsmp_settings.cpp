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

#include "dsmp_settings.h"

#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

// Enable-AETR: a single label -> ToggleSwitch line (always shown for DSMP).
struct DSMPEnableAETR {
  DSMPEnableAETR(Window* form, uint8_t moduleIdx)
  {
    auto md = &g_model.moduleData[moduleIdx];

    new ds::FormRow(form, STR_DSMP_ENABLE_AETR, [&](Window* slot) {
      cb = new ToggleSwitch(slot, rect_t{}, GET_SET_DEFAULT(md->dsmp.enableAETR));
    });
  }

  void update() { cb->update(); }

 private:
  ToggleSwitch* cb = nullptr;
};

DSMPSettings::DSMPSettings(Window* parent,
                           const FlexGridLayout& g,
                           uint8_t moduleIdx) :
    Window(parent, rect_t{}),
    md(&g_model.moduleData[moduleIdx]),
    moduleIdx(moduleIdx)
{
  setFlexLayout();

  // DSMP status: label -> live status text.
  new ds::FormRow(this, STR_MODULE_STATUS, [&](Window* slot) {
    new DynamicText(
        slot, rect_t{},
        [=] {
          char msg[64] = "";
          getModuleStatusString(moduleIdx, msg);
          return std::string(msg);
        });
  });

  enableAETR_line = new DSMPEnableAETR(this, moduleIdx);

  // Ensure elements properly initalised
  update();
}

void DSMPSettings::update() { 
    enableAETR_line->update(); 
}
