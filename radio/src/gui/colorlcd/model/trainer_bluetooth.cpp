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

#include "trainer_bluetooth.h"

#include "dialog.h"
#include "ds_core.h"
#include "edgetx.h"
#include "menu.h"

#define SET_DIRTY()     storageDirty(EE_MODEL)

class BTDiscoverMenu : public Menu
{
  uint8_t devCount = 0;

  void onLiveCheckEvents(LiveWindow& live) override;
  void selectAddr(const char* addr);

public:
  BTDiscoverMenu();
};

BTDiscoverMenu::BTDiscoverMenu() :
  Menu()
{
  setTitle(STR_BT_SELECT_DEVICE);
  // TODO: set minimum height

}

void BTDiscoverMenu::onLiveCheckEvents(Window::LiveWindow& live)
{
  if (bluetooth.state == BLUETOOTH_STATE_DISCOVER_START ||
      bluetooth.state == BLUETOOTH_STATE_DISCOVER_END) {
    int cnt = min<uint8_t>(reusableBuffer.moduleSetup.bt.devicesCount,
                           MAX_BLUETOOTH_DISTANT_ADDR);

    if (devCount < cnt) {
      for (int i = 0; i < cnt - devCount; i++) {
        int index = devCount + i;
        const char* item = reusableBuffer.moduleSetup.bt.devices[index];
        addLine(item, [=]() { selectAddr(item); });
      }
      devCount = cnt;
    }
  }
}

void BTDiscoverMenu::selectAddr(const char* addr)
{
  strncpy(bluetooth.distantAddr, addr, LEN_BLUETOOTH_ADDR);
  bluetooth.state = BLUETOOTH_STATE_BIND_REQUESTED;
  SET_DIRTY();
}

BluetoothTrainerWindow::BluetoothTrainerWindow(Window* parent) :
    Window(parent, rect_t{})
{
  setFlexLayout();

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): connection state and remote address
  // are two fixed, side-by-side controls on the one line — the state text
  // itself stands in for a caption, so it is a ds::FieldRow with blank field
  // captions rather than a labelled FormRow. Local address is a plain
  // label->value FormRow. The bind/scan button line has no caption either, so
  // it keeps the label column blank (matches the button-only lines elsewhere)
  // to stay x-aligned with the rows above it.
  new ds::FieldRow(this, {
      {"", [=](Window* slot) { state = new StaticText(slot, rect_t{}, ""); }},
      {"", [=](Window* slot) { r_addr = new StaticText(slot, rect_t{}, ""); }},
  });

  new ds::FormRow(this, STR_BLUETOOTH_LOCAL_ADDR, [=](Window* slot) {
    new StaticText(slot, rect_t{}, bluetooth.localAddr);
  });

  btn_line = new ds::FormRow(this, "", [=](Window* slot) {
    btn = new TextButton(slot, rect_t{}, "");
  });
}

void BluetoothTrainerWindow::setMaster(bool master)
{
  btn_line->show(master);
  is_master = master;
}

static const char _empty_addr[] = "---";

void BluetoothTrainerWindow::onLiveCheckEvents(Window::LiveWindow& live)
{
  if(bluetooth.state != lastbtstate ||
     reusableBuffer.moduleSetup.bt.devicesCount != devcount)
    refresh();
  lastbtstate = bluetooth.state;
  devcount = reusableBuffer.moduleSetup.bt.devicesCount;
  Window::onLiveCheckEvents(live);
}

void BluetoothTrainerWindow::refresh()
{
  if (is_master) {
    if (bluetooth.state == BLUETOOTH_STATE_DISCOVER_SENT) {
      btn->setText(STR_BLUETOOTH_SCANNING);
      btn->setPressHandler([=]() {
        startScan(); // Allow restart scan if stuck here
        return 0;});
    } else if (bluetooth.state == BLUETOOTH_STATE_DISCOVER_START) {
      if(reusableBuffer.moduleSetup.bt.devicesCount && !menuopened) { // On first item found, open menu
        auto btdm = new BTDiscoverMenu();
        btdm->setCloseHandler([=]() {
          menuopened = false;
          if(bluetooth.state != BLUETOOTH_STATE_BIND_REQUESTED)
            bluetooth.state = BLUETOOTH_STATE_IDLE;
        });
        menuopened = true;
      }
    } else if (bluetooth.state == BLUETOOTH_STATE_DISCOVER_END) {
      if(reusableBuffer.moduleSetup.bt.devicesCount == 0) {
        new MessageDialog(STR_BLUETOOTH, STR_BLUETOOTH_NODEVICES);
        bluetooth.state = BLUETOOTH_STATE_OFF;
      }
    } else if (bluetooth.distantAddr[0]) {
      r_addr->setText(bluetooth.distantAddr);
      btn->setText(STR_CLEAR);
      btn->setPressHandler([]() {
        bluetooth.state = BLUETOOTH_STATE_CLEAR_REQUESTED;
        memclear(bluetooth.distantAddr, sizeof(bluetooth.distantAddr));
        return 0;
      });
    } else if (bluetooth.state < BLUETOOTH_STATE_IDLE) {
      r_addr->setText(_empty_addr);
      btn->setText(STR_BLUETOOTH_INIT);
      btn->setPressHandler([]() {
        bluetooth.state = BLUETOOTH_STATE_OFF;
        return 0;
      });
    } else {
      r_addr->setText(_empty_addr);
      btn->setText(STR_DISCOVER);
      btn->setPressHandler([=]() {
        startScan();
        return 0;
      });
    }
  }

  if (bluetooth.state == BLUETOOTH_STATE_CONNECTED) {
    state->setText(STR_CONNECTED);
    if (!is_master) r_addr->setText(bluetooth.distantAddr);
  } else if (bluetooth.state != BLUETOOTH_STATE_DISCOVER_REQUESTED ||
             bluetooth.state != BLUETOOTH_STATE_DISCOVER_SENT ||
             !is_master) {
    state->setText(STR_NOT_CONNECTED);
    if (!is_master) r_addr->setText(_empty_addr);
  }
}

void BluetoothTrainerWindow::startScan()
{
  reusableBuffer.moduleSetup.bt.devicesCount = 0;
  devcount = 0;
  bluetooth.state = BLUETOOTH_STATE_DISCOVER_REQUESTED;
}
