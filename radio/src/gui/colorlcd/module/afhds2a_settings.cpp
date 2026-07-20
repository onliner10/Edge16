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

#include "afhds2a_settings.h"

#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "pulses/flysky.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

// The two RX-option Choices (PPM/PWM pulse proto + SBUS/iBUS serial proto)
// flowed side-by-side into a ds::FieldGroup's content area (which owns the
// layout, gaps and touch floor), so this helper just builds them into the
// group's content rather than being a Window itself.
struct FSProtoOpts {
  FSProtoOpts(Window* content, std::function<uint8_t()> getMode,
              std::function<void(uint8_t)> setMode)
  {
    // PPM / PWM
    new Choice(
        content, rect_t{}, STR_FLYSKY_PULSE_PROTO, 0, 1,
        [=]() -> int { return getMode() >> 1; },
        [=](int v) {
          setMode((getMode() & 1) | ((v & 1) << 1));
          SET_DIRTY();
        });

    // SBUS / iBUS
    new Choice(
        content, rect_t{}, STR_FLYSKY_SERIAL_PROTO, 0, 1,
        [=]() -> int { return getMode() & 1; },
        [=](int v) {
          setMode((getMode() & 2) | (v & 1));
          SET_DIRTY();
        });
  }
};

AFHDS2ASettings::AFHDS2ASettings(Window* parent, const FlexGridLayout& g,
                               uint8_t moduleIdx) :
    Window(parent, rect_t{}),
    moduleIdx(moduleIdx),
    md(&g_model.moduleData[moduleIdx]),
    grid(g)
{
  setFlexLayout();

  // RX options: the PPM/PWM + SBUS/iBUS Choices flowed after the OPTIONS label.
  afhds2OptionsGroup = new ds::FieldGroup(this, STR_OPTIONS, [=](Window* content) {
    FSProtoOpts(content, [=]() { return md->flysky.mode; },
                [=](uint8_t v) { md->flysky.mode = v; });
  });

#if defined(RADIO_NV14_FAMILY)
  static const char* _rf_power[] = {"Default", "High"};
  afhds2RFPowerLine = new ds::FormRow(this, STR_MULTI_RFPOWER, [=](Window* slot) {
    afhds2RFPowerChoice = new Choice(slot, rect_t{}, _rf_power, 0, 1,
                                     GET_DEFAULT(md->flysky.rfPower),
                                     [=](int32_t newValue) -> void {
                                       md->flysky.rfPower = newValue;
                                       resetPulsesAFHDS2();
                                     });
  });
#endif

  hideAFHDS2Options();
}

void AFHDS2ASettings::hideAFHDS2Options()
{
  afhds2OptionsGroup->hide();
#if defined(RADIO_NV14_FAMILY)
  afhds2RFPowerLine->hide();
#endif
}

void AFHDS2ASettings::showAFHDS2Options()
{
  afhds2OptionsGroup->show();
#if defined(RADIO_NV14_FAMILY)
  bool showRFPower = (getNV14RfFwVersion() >= 0x1000E);
  afhds2RFPowerLine->show(showRFPower);
  if (showRFPower && (showRFPower != hasRFPower)) {
    hasRFPower = showRFPower;
    afhds2RFPowerChoice->update();
  }
#endif
}

void AFHDS2ASettings::onLiveCheckEvents(Window::LiveWindow& live)
{
  Window::onLiveCheckEvents(live);
  update();
}

void AFHDS2ASettings::update()
{
  if (isModuleAFHDS2A(moduleIdx)) {
    showAFHDS2Options();
  } else {
    hideAFHDS2Options();
  }
}
