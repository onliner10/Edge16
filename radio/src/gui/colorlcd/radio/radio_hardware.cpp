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

#include "radio_hardware.h"

#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "hal/adc_driver.h"
#include "hw_extmodule.h"
#include "hw_inputs.h"
#include "hw_intmodule.h"
#include "hw_serial.h"
#include "numberedit.h"
#include "radio_calibration.h"
#include "radio_diaganas.h"
#include "radio_diagkeys.h"
#include "radio_setup.h"

#if defined(FUNCTION_SWITCHES)
#include "radio_cfs.h"
#include "radio_diagcustswitches.h"
#endif

#if defined(BLUETOOTH)
#include "hw_bluetooth.h"
#endif

#define SET_DIRTY() storageDirty(EE_GENERAL)

#if PORTRAIT
static const lv_coord_t col_dsc[] = {LV_GRID_FR(13), LV_GRID_FR(19),
                                     LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2),
                                     LV_GRID_TEMPLATE_LAST};
#endif

static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

RadioHardwarePage::RadioHardwarePage(const PageDef& pageDef) :
    PageGroupItem(pageDef, PAD_TINY)  // ds-allow: radio hardware — tiny inter-section padding for a page mixing the DS settings list with multi-column hardware-module grids and nav-tile button groups; not a single DS FormRow control.
{
  enableVBatBridge();
}

void RadioHardwarePage::checkEvents() { enableVBatBridge(); }

void RadioHardwarePage::cleanup()
{
  disableVBatBridge();
}

class BatCalEdit : public NumberEdit
{
 public:
  BatCalEdit(Window* parent, const rect_t& rect) :
      NumberEdit(parent, rect, -127, 127,
                 GET_SET_DEFAULT(g_eeGeneral.txVoltageCalibration))
  {
    setDisplayHandler([](int32_t value) {
      return formatNumberAsString(getBatteryVoltage(), PREC2, 0, nullptr, "V");
    });
    lastBatVolts = getBatteryVoltage();
  }

 protected:
  uint16_t lastBatVolts = 0;

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (getBatteryVoltage() != lastBatVolts) {
      lastBatVolts = getBatteryVoltage();
      setValue(g_eeGeneral.txVoltageCalibration);
    }
  }
};

const static PageButtonDef calibrationButtons[] = {
  {STR_DEF(STR_MENUCALIBRATION), [](Route r) { new RadioCalibrationPage(r); }},
  {STR_DEF(STR_STICKS), [](Route) { new HWInputDialog<HWSticks>(STR_STICKS); }},
  {STR_DEF(STR_POTS), [](Route) { new HWInputDialog<HWPots>(STR_POTS, HWPots::POTS_WINDOW_WIDTH); }},
  {STR_DEF(STR_SWITCHES), [](Route) { new HWInputDialog<HWSwitches>(STR_SWITCHES, HWSwitches::SW_WINDOW_WIDTH); }},
#if defined(FUNCTION_SWITCHES)
  {STR_DEF(STR_FUNCTION_SWITCHES), [](Route r) { new RadioFunctionSwitches(r); }},
#endif
  {nullptr},
};

const static PageButtonDef debugButtons[] = {
  {STR_DEF(STR_ANALOGS_BTN), [](Route) { new RadioAnalogsDiagsViewPageGroup(); }},
  {STR_DEF(STR_KEYS_BTN), [](Route r) { new RadioKeyDiagsPage(r); }},
#if defined(FUNCTION_SWITCHES)
  {STR_DEF(STR_FS_BTN), [](Route r) { new RadioCustSwitchesDiagsPage(r); }},
#endif
  {nullptr},
};

void RadioHardwarePage::build(Window* window)
{
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_TINY);  // ds-allow: radio hardware — column gap for a page mixing the DS settings list with multi-column hardware-module grids and nav-tile button groups; not a single DS FormRow control.

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the plain hardware settings are a
  // stack of ds::FormRow (label 40% / control 60%, 40 px min) grouped in a
  // Card. The ds::List owns page margins and inter-row gaps; the DS owns the
  // label/control split — no per-screen coordinates. The module-config
  // sub-grids and nav-tile button groups below stay on their existing
  // hand-rolled layout (they are not settings-form lines).
  Window* list = new ds::List(window);
  auto* form = new ds::Card(list);

  // Batt meter range - Range 3.0v to 16v
  new ds::FormRow(form, STR_BATTERY_RANGE, [=](Window* slot) {
    auto* group = new Window(slot, rect_t{});
    group->setFlexLayout(LV_FLEX_FLOW_ROW);

    auto batMin = new NumberEdit(
        group, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, -60 + 90, g_eeGeneral.vBatMax + 29 + 90,
        GET_SET_WITH_OFFSET(g_eeGeneral.vBatMin, 90), PREC1);
    batMin->setSuffix("V");
    new StaticText(group, rect_t{}, "-");
    auto batMax = new NumberEdit(
        group, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, g_eeGeneral.vBatMin - 29 + 120, 40 + 120,
        GET_SET_WITH_OFFSET(g_eeGeneral.vBatMax, 120), PREC1);
    batMax->setSuffix("V");

    batMin->setSetValueHandler([=](int32_t newValue) {
      g_eeGeneral.vBatMin = newValue - 90;
      SET_DIRTY();
      batMax->setMin(g_eeGeneral.vBatMin - 29 + 120);
    });

    batMax->setSetValueHandler([=](int32_t newValue) {
      g_eeGeneral.vBatMax = newValue - 120;
      SET_DIRTY();
      batMin->setMax(g_eeGeneral.vBatMax + 29 + 90);
    });
  });

  // Bat calibration
  new ds::FormRow(form, STR_BATT_CALIB, [](Window* slot) {
    new BatCalEdit(slot, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0});
  });

  // RTC Batt check enable
  new ds::FormRow(form, STR_RTC_CHECK, [](Window* slot) {
    auto* group = new Window(slot, rect_t{});
    group->setFlexLayout(LV_FLEX_FLOW_ROW);

    new ToggleSwitch(group, rect_t{}, GET_SET_INVERTED(g_eeGeneral.disableRtcWarning));

    // RTC Batt display
    new DynamicNumber<uint16_t>(
        group, rect_t{},
        [] { return getRTCBatteryVoltage(); }, COLOR_THEME_PRIMARY1_INDEX, PREC2,
        "", "V");
  });

  // ADC filter
  new ds::FormRow(form, STR_JITTER_FILTER, [](Window* slot) {
    new ToggleSwitch(slot, rect_t{}, GET_SET_INVERTED(g_eeGeneral.noJitterFilter));
  });

#if defined(AUDIO_MUTE_GPIO)
  // Mute audio
  new ds::FormRow(form, STR_AUDIO_MUTE, [](Window* slot) {
    new ToggleSwitch(slot, rect_t{}, GET_SET_DEFAULT(g_eeGeneral.audioMuteEnable));
  });
#endif

  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);  // ds-allow: radio hardware — column gap for the module-config sub-grids on the hardware page; not a single DS FormRow control.

#if defined(HARDWARE_INTERNAL_MODULE)
  new Subtitle(window, STR_INTERNALRF);
  new InternalModuleWindow(window, grid);
#endif

#if defined(HARDWARE_EXTERNAL_MODULE) && defined(STM32F4)
  new Subtitle(window, STR_EXTERNALRF);
  new ExternalModuleWindow(window, grid);
#endif

#if defined(BLUETOOTH)
  new Subtitle(window, STR_BLUETOOTH);
  new BluetoothConfigWindow(window, grid);
#endif

  new Subtitle(window, STR_AUX_SERIAL_MODE);
  new SerialConfigWindow(window, grid);

  // Calibration
  new SetupButtonGroup(window, {0, 0, LCD_W - padding * 2, 0}, STR_INPUTS, BTN_COLS, PAD_ZERO, calibrationButtons, route());  // ds-allow: radio hardware — multi-column calibration button group spanning the page width; not a single DS FormRow control.

  // Debugs
  new SetupButtonGroup(window, {0, 0, LCD_W - padding * 2, 0}, STR_DEBUG, FS_BTN_COLS, PAD_ZERO, debugButtons, route());  // ds-allow: radio hardware — multi-column debug button group spanning the page width; not a single DS FormRow control.
}
