/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "dialogs/battery_confirm_dialog.h"

#include "button.h"
#include "fonts.h"
#include "static.h"
#include "edgetx.h"
#include "telemetry/battery_monitor.h"

BatteryConfirmDialog::BatteryConfirmDialog(uint8_t monitor, uint16_t packMask)
    : FullScreenDialog(WARNING_TYPE_ASTERISK, "Select battery", "",
                       "", nullptr),
      monitor(monitor), packMask(packMask)
{
  markFlightBatteryPromptShown(monitor);
  buildBody();
}

void BatteryConfirmDialog::buildBody()
{
  auto background = Window::makeLive<Window>(this, rect_t{0, 0, LCD_W, LCD_H});
  if (background) {
    background->setWindowFlag(NO_FOCUS | NO_SCROLL);
    background->withLive([](Window::LiveWindow& live) {
      etx_solid_bg(live.lvobj(), COLOR_THEME_PRIMARY2_INDEX);
    });
  }

  constexpr coord_t margin = PAD_LARGE;
  constexpr coord_t titleTop = PAD_LARGE;
  constexpr coord_t titleHeight = 36;
  constexpr coord_t subtitleTop = titleTop + titleHeight;
  constexpr coord_t subtitleHeight = 24;
  constexpr coord_t listTop = subtitleTop + subtitleHeight + PAD_SMALL;
  constexpr coord_t listWidth = LCD_W - margin * 2;
  constexpr coord_t listHeight = LCD_H - listTop - PAD_LARGE;
  constexpr coord_t rowHeight = EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_LARGE;

  Window::makeLive<StaticText>(
      this, rect_t{margin, titleTop, listWidth, titleHeight},
      "Select battery", COLOR_THEME_PRIMARY1_INDEX, FONT(XL) | CENTERED);

  Window::makeLive<StaticText>(
      this, rect_t{margin, subtitleTop, listWidth, subtitleHeight},
      "Choose connected pack", COLOR_THEME_PRIMARY1_INDEX, FONT(STD) | CENTERED);

  auto body = Window::makeLive<Window>(
      this, rect_t{margin, listTop, listWidth, listHeight});
  if (!body) return;

  body->padAll(PAD_ZERO);
  body->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL, listWidth, listHeight);
  body->withLive([](Window::LiveWindow& live) {
    auto obj = live.lvobj();
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
  });

  bool hasChoices = false;
  bool specSeen[MAX_BATTERY_PACKS] = {};
  for (uint8_t slot = 0; slot < MAX_BATTERY_PACKS; slot++) {
    if (specSeen[slot] || !(packMask & (1 << slot))) continue;

    BatteryPackData* pack = &g_eeGeneral.batteryPacks[slot];
    if (!pack->active) continue;

    for (uint8_t j = slot + 1; j < MAX_BATTERY_PACKS; j++) {
      if ((packMask & (1 << j)) && g_eeGeneral.batteryPacks[j].active &&
          batterySpecEquals(*pack, g_eeGeneral.batteryPacks[j]))
        specSeen[j] = true;
    }

    char label[32];
    snprintf(label, sizeof(label), "%s %dS %dmAh",
             batteryTypeToString((BatteryType)pack->batteryType),
             pack->cellCount, pack->capacity);

    hasChoices = true;
    auto btn = Window::makeLive<TextButton>(
        body, rect_t{0, 0, LV_PCT(100), rowHeight},
        label, [=]() -> uint8_t {
          onConfirmPack(slot + 1);
          return 0;
        });
    if (btn) {
      btn->setFont(FONT_BOLD_INDEX);
      btn->setWrap();
    }
  }

  if (!hasChoices) {
    Window::makeLive<StaticText>(body, rect_t{0, 0, LV_PCT(100), rowHeight},
                                 "No compatible pack", COLOR_THEME_WARNING_INDEX,
                                 FONT(STD) | CENTERED);
  }
}

void BatteryConfirmDialog::onConfirmPack(uint8_t slot)
{
  if (flightBatterySessionState(monitor) != FlightBatterySessionState::NeedsConfirmation) {
    closeDialog();
    return;
  }
  if (!confirmFlightBatteryPack(monitor, slot)) {
    return;
  }
  closeDialog();
}

void BatteryConfirmDialog::closeDialog()
{
  deleteLater();
}
