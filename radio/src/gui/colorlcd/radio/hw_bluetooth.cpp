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

#include "hw_bluetooth.h"

#include "choice.h"
#include "dialog.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "static.h"
#include "textedit.h"

#define SET_DIRTY() storageDirty(EE_GENERAL)

class BTDetailsDialog : public BaseDialog
{
 public:
  BTDetailsDialog() : BaseDialog(STR_BLUETOOTH, true)
  {
    // Read-only info lines: each a single labelled value -> ds::FormRow.
    form.with([](Window& content) {
      if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY) {
        new ds::FormRow(&content, STR_BLUETOOTH_PIN_CODE, [](Window* slot) {
          new StaticText(slot, rect_t{}, "000000");
        });
      }

      // Local MAC
      new ds::FormRow(&content, STR_BLUETOOTH_LOCAL_ADDR, [](Window* slot) {
        new DynamicText(slot, rect_t{}, []() {
          return std::string(bluetooth.localAddr[0] ? bluetooth.localAddr
                                                    : "---");
        });
      });

      // Remote MAC
      new ds::FormRow(&content, STR_BLUETOOTH_DIST_ADDR, [](Window* slot) {
        new DynamicText(slot, rect_t{}, []() {
          return std::string(bluetooth.distantAddr[0] ? bluetooth.distantAddr
                                                      : "---");
        });
      });
    });
  }
};

BluetoothConfigWindow::BluetoothConfigWindow(Window* parent, FlexGridLayout& grid)
{
  // Mode + settings shortcut: a Choice plus a settings button that shows only
  // when Bluetooth is on -> ds::FieldGroup (variable-count option box).
  new ds::FieldGroup(parent, STR_MODE, [=](Window* box) {
    new Choice(
        box, rect_t{}, STR_BLUETOOTH_MODES, BLUETOOTH_OFF, BLUETOOTH_TRAINER,
        GET_DEFAULT(g_eeGeneral.bluetoothMode), [=](int value) {
          g_eeGeneral.bluetoothMode = value;
          settingsBtn->show(g_eeGeneral.bluetoothMode != BLUETOOTH_OFF);
          nameEdit->show(g_eeGeneral.bluetoothMode != BLUETOOTH_OFF);
        });

    settingsBtn =
        new TextButton(box, rect_t{}, LV_SYMBOL_SETTINGS, [=]() -> uint8_t {
          new BTDetailsDialog();
          return 0;
        });
    settingsBtn->show(g_eeGeneral.bluetoothMode != BLUETOOTH_OFF);
  });

  // BT radio name: single labelled control, whole row gated on mode being on
  // (nameEdit->show below and from the mode Choice callback above).
  nameEdit = new ds::FormRow(parent, STR_NAME, [=](Window* slot) {
    new RadioTextEdit(slot, rect_t{}, g_eeGeneral.bluetoothName,
                      LEN_BLUETOOTH_NAME);
  });
  nameEdit->show(g_eeGeneral.bluetoothMode != BLUETOOTH_OFF);
}
