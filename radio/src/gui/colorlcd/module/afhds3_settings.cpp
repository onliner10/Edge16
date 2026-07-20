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

#include "afhds3_settings.h"

#include "afhds3_options.h"
#include "button.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"

static const char* const _afhds3_region[] = {"CE", "FCC"};

static const char* const _afhds3_phy_mode[] = {
    // V0
    "Classic 18ch",
    "C-Fast 10ch",
    // V1
    "Routine 18ch",
    "Fast 8ch",
    "Lora 12ch",
};

#include "pulses/afhds3.h"
#include "pulses/afhds3_config.h"
#include "pulses/flysky.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

AFHDS3Settings::AFHDS3Settings(Window* parent, const FlexGridLayout& g,
                               uint8_t moduleIdx) :
    Window(parent, rect_t{}),
    moduleIdx(moduleIdx),
    md(&g_model.moduleData[moduleIdx]),
    grid(g)
{
  setFlexLayout();

  // Status
  afhds3StatusLine = new ds::FormRow(this, STR_MODULE_STATUS, [=](Window* slot) {
    new DynamicText(slot, rect_t{}, [=] {
      char msg[64] = "";
      getModuleStatusString(moduleIdx, msg);
      return std::string(msg);
    });
  });

  // TYPE: phy-mode | region | Options button flowed after the TYPE label.
  afhds3TypeGroup = new ds::FieldGroup(this, STR_TYPE, [=](Window* content) {
    afhds3PhyMode =
        new Choice(content, rect_t{}, _afhds3_phy_mode, 0,
                   afhds3::PHYMODE_MAX, GET_SET_DEFAULT(md->afhds3.phyMode));

    afhds3Emi =
        new Choice(content, rect_t{}, _afhds3_region, afhds3::LNK_ES_CE,
                   afhds3::LNK_ES_FCC, GET_SET_DEFAULT(md->afhds3.emi));

    new TextButton(content, rect_t{}, STR_MODULE_OPTIONS, [=]() {
      afhds3::applyModelConfig(moduleIdx);
      new AFHDS3_Options(moduleIdx, Route{});
      return 0;
    });
  });

  bool hasPowerOption = false;
  int maxPower;
  if (moduleIdx == INTERNAL_MODULE) {
  #if defined(RADIO_PL18U) || defined(PCBPA01)
    hasPowerOption = true;
    maxPower = AFHDS3_POWER_500;
  #if  defined(PCBPA01)
    md->afhds3.rfPower = afhds3::get_current_rfpower_level(moduleIdx);
  #endif
  #endif
  } else if (moduleIdx == EXTERNAL_MODULE) {
    hasPowerOption = true;
    maxPower = AFHDS3_FRM303_POWER_MAX;
    md->afhds3.rfPower = afhds3::get_current_rfpower_level(moduleIdx);
  }

  if (hasPowerOption) {
    auto cfg = afhds3::getConfig(moduleIdx);
    new ds::FormRow(this, STR_MULTI_RFPOWER, [=](Window* slot) {
      afhds3RfPower = new Choice(
          slot, rect_t{}, STR_AFHDS3_POWERS, 0, maxPower,
          GET_DEFAULT(md->afhds3.rfPower), [=](int32_t newValue) {
            md->afhds3.rfPower = newValue;
            cfg->others.dirtyFlag |= (uint32_t)1
                                     << afhds3::DirtyConfig::DC_RX_CMD_TX_PWR;
            SET_DIRTY();
          });
    });
  }

  hideAFHDS3Options();
}

void AFHDS3Settings::hideAFHDS3Options()
{
  afhds3StatusLine->hide();
  afhds3TypeGroup->hide();
}

void AFHDS3Settings::showAFHDS3Options()
{
  afhds3StatusLine->show();
  afhds3TypeGroup->show();
  afhds3PhyMode->update();
  afhds3Emi->update();
  if (moduleIdx == EXTERNAL_MODULE) {
    afhds3RfPower->update();
  }
  if (afhds3::getConfig(moduleIdx)->others.isConnected) {
    afhds3PhyMode->disable();
    afhds3Emi->disable();
  } else {
    afhds3PhyMode->enable();
    afhds3Emi->enable();
  }
}

void AFHDS3Settings::onLiveCheckEvents(Window::LiveWindow& live)
{
  if (afhds3::getConfig(moduleIdx)->others.lastUpdated > lastRefresh) {
    update();
  }
  Window::onLiveCheckEvents(live);
}

void AFHDS3Settings::update()
{
  lastRefresh = get_tmr10ms();

  if (isModuleAFHDS3(moduleIdx)) {
    showAFHDS3Options();
  } else {
    hideAFHDS3Options();
  }
}
