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

#include "trainer_setup.h"

#include "button.h"
#include "channel_range.h"
#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "menu.h"
#include "numberedit.h"
#include "ppm_settings.h"
#include "textedit.h"

#if defined(BLUETOOTH)
#include "trainer_bluetooth.h"
#endif

#define SET_DIRTY()     storageDirty(EE_MODEL)

class TrainerModuleWindow : public Window
{
 public:
  TrainerModuleWindow(Window* parent);

  void onLiveCheckEvents(LiveWindow& live) override;
  void update();

 protected:
  ChannelRange* chRange = nullptr;

#if defined(BLUETOOTH)
  // StaticText *btChannelEnd = nullptr;
  // StaticText *btDistAddress = nullptr;
  // TextButton *btMasterButton = nullptr;
  // Menu *btPopUpMenu = nullptr;
  // bool btCanceled = false;

 private:
  // bool popupopen = false;
  // int devicecount = 0;
  // uint8_t lastbluetoothstate = BLUETOOTH_STATE_OFF;

  // void btDiscoverMenuItemChosen();
  // void btDiscoverMenuAddItem(const char *itm);

#endif
};

TrainerModuleWindow::TrainerModuleWindow(Window* parent) :
    Window(parent, rect_t{})
{
  setFlexLayout();
  update();
}

void TrainerModuleWindow::onLiveCheckEvents(Window::LiveWindow& live)
{
// #if defined(BLUETOOTH)
//   if (popupopen) {
//     if (bluetooth.state == BLUETOOTH_STATE_DISCOVER_START ||
//         bluetooth.state == BLUETOOTH_STATE_DISCOVER_END) {
//       int cnt = min<uint8_t>(reusableBuffer.moduleSetup.bt.devicesCount,
//                              MAX_BLUETOOTH_DISTANT_ADDR);
//       if (devicecount < cnt) {
//         for (int i = 0; i < cnt - devicecount; i++) {
//           int index = devicecount + i;
//           btDiscoverMenuAddItem(reusableBuffer.moduleSetup.bt.devices[index]);
//         }
//         devicecount = cnt;
//       }
//     }
//   }
//   if (bluetooth.state != lastbluetoothstate) {
//     // TODO:
//     // if (!popupopen && !trChoiceOpen) update();
//     lastbluetoothstate = bluetooth.state;
//   }
// #endif
  Window::onLiveCheckEvents(live);
}

void TrainerModuleWindow::update()
{
  clear();

  auto td = &g_model.trainerData;
  if (td->mode == TRAINER_MODE_OFF) return;

#if defined(BLUETOOTH)
  if (td->mode == TRAINER_MODE_MASTER_BLUETOOTH ||
      td->mode == TRAINER_MODE_SLAVE_BLUETOOTH) {

    auto bt = new BluetoothTrainerWindow(this);
    if (td->mode == TRAINER_MODE_SLAVE_BLUETOOTH)
      bt->setMaster(false);

    bt->refresh();
    // TODO: slave: channel range
  }
#endif

  if (td->mode == TRAINER_MODE_SLAVE) {
    // DESIGN SYSTEM (see DESIGN_SYSTEM.md): each line pairs one label with
    // one composite control (the channel-range selector, the PPM-frame
    // settings widget) — a plain ds::FormRow per line. Their own internal
    // layout (e.g. the per-channel calibration table inside the channel
    // range selector) is untouched by this migration.

    // Channel range
    new ds::FormRow(this, STR_CHANNELRANGE, [=](Window* slot) {
      chRange = new TrainerChannelRange(slot);
    });

    // PPM frame
    new ds::FormRow(this, STR_PPMFRAME, [=](Window* slot) {
      auto obj = new PpmFrameSettings<TrainerModuleData>(slot, td);

      // copy pointer to frame len edit object to channel range
      chRange->setPpmFrameLenEditObject(obj->getPpmFrameLenEditObject());
    });
  }
}

TrainerPage::TrainerPage(Route route) : Page(ICON_MODEL_SETUP, route)
{
  header->setTitle(STR_MAIN_MENU_MODEL_SETTINGS);
  header->setTitle2(STR_TRAINER);

  body->setFlexLayout();

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the Mode line is a single
  // label->control ds::FormRow; it delegates to a TrainerModuleWindow
  // sub-form (rebuilt on mode change) below it.
  Choice* trainerChoice = nullptr;
  new ds::FormRow(body, STR_MODE, [&](Window* slot) {
    trainerChoice =
        new Choice(slot, rect_t{}, STR_VTRAINERMODES, 0, TRAINER_MODE_MAX(),
                   GET_SET_DEFAULT(g_model.trainerData.mode));
    trainerChoice->setAvailableHandler(isTrainerModeAvailable);
  });

  auto trainerModule = new TrainerModuleWindow(body);

  TrainerModuleData* tr = &g_model.trainerData;
  trainerChoice->setSetValueHandler([=](int32_t newValue) {
    //TODO: move the BT stuff somewhere else?
#if defined(BLUETOOTH)
    memclear(bluetooth.distantAddr, sizeof(bluetooth.distantAddr));
    bluetooth.state = BLUETOOTH_STATE_OFF;
#endif
    tr->mode = newValue;
    trainerModule->update();
    SET_DIRTY();
  });
}
