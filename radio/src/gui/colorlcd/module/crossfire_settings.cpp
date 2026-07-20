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

#include "crossfire_settings.h"

#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "mixer_scheduler.h"
#include "telemetry/crossfire.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

CrossfireSettings::CrossfireSettings(Window* parent, const FlexGridLayout& g,
                                     uint8_t moduleIdx) :
    Window(parent, rect_t{}), md(&g_model.moduleData[moduleIdx]), moduleIdx(moduleIdx)
{
  setFlexLayout();

  // Baudrate (external module only): label -> Choice line.
  if (moduleIdx == EXTERNAL_MODULE) {
    new ds::FormRow(this, STR_BAUDRATE, [&](Window* slot) {
      new Choice(
          slot, rect_t{}, STR_CRSF_BAUDRATE, 0, CROSSFIRE_MAX_EXTERNAL_BAUDRATE,
          [=]() -> int {
            return CROSSFIRE_STORE_TO_INDEX(md->crsf.telemetryBaudrate);
          },
          [=](int newValue) {
            md->crsf.telemetryBaudrate = CROSSFIRE_INDEX_TO_STORE(newValue);
            SET_DIRTY();
            restartModule(moduleIdx);
          });
    });
  } else {
    //   char buf[6];
    //   new StaticText(this, grid.getFieldSlot(2, 1),
    //                  getStringAtIndex(buf, STR_CRSF_BAUDRATE,
    //                                   CROSSFIRE_STORE_TO_INDEX(
    //                                       g_eeGeneral.internalModuleBaudrate)),
    //                  COLOR_THEME_PRIMARY1);
  }

  // Status: label -> live rate text.
  new ds::FormRow(this, STR_STATUS, [&](Window* slot) {
    new DynamicText(slot, rect_t{}, [=] {
      char msg[64] = "";
      // sprintf(msg, "%d Hz %" PRIu32 " Err", 1000000 / getMixerSchedulerPeriod(),
      //         telemetryErrors);
      sprintf(msg, "%d Hz", 1000000 / getMixerSchedulerPeriod());
      return std::string(msg);
    });
  });

  // Arming mode: a variable option box (arming-mode Choice, plus the trigger
  // SwitchChoice when in switch mode) flowed after the label. The whole group
  // is shown/hidden by version in update().
  armingGroup = new ds::FieldGroup(this, STR_CRSF_ARMING_MODE, [&](Window* box) {
    choArmMode = new Choice(box, rect_t{}, STR_CRSF_ARMING_MODES, 0, 1, GET_SET_DEFAULT(md->crsf.crsfArmingMode));
    choArmSwitch = new SwitchChoice(box, rect_t{}, SWSRC_FIRST, SWSRC_LAST, GET_SET_DEFAULT(md->crsf.crsfArmingTrigger));
    choArmSwitch->setAvailableHandler([=](int sw) { return isSwitchAvailableForArming(sw); });
  });

  update();
}

void CrossfireSettings::update() {
    if(CRSF_ELRS_MIN_VER(moduleIdx, 4, 0)) {
      armingGroup->show();

      if(md->crsf.crsfArmingMode == ARMING_MODE_SWITCH)
        choArmSwitch->show();
      else
        choArmSwitch->hide();
    } else {
      armingGroup->hide();
    }
}

void CrossfireSettings::onLiveCheckEvents(Window::LiveWindow& live) {
  update();

  Window::onLiveCheckEvents(live);
}
