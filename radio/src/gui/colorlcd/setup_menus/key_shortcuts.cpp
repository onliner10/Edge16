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

#include "key_shortcuts.h"

#include "button.h"
#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "pagegroup.h"
#include "qmpagechoice.h"
#include "radio_tools.h"

#include <new>

#define SET_DIRTY() storageDirty(EE_GENERAL)

static bool hasKey(event_t event)
{
#if defined(USE_HATS_AS_KEYS)
  return true;
#else
  EnumKeys key = (EnumKeys)EVT_KEY_MASK(event);
  return (keysGetSupported() & (1 << key));
#endif
}

void QMKeyShortcutsPage::addKey(Window* parent, event_t event, std::vector<std::string> qmPages, const char* nm)
{
  if (hasKey(event)) {
    extern uint16_t keyMapping(uint16_t event);
    event = keyMapping(event);

    new ds::FormRow(parent, nm, [=](Window* slot) {
          auto c = new (std::nothrow) QMPageChoice(
              slot, rect_t{}, qmPages, QM_NONE, qmPages.size() - 1,
              [=]() -> int {
                auto pg = g_eeGeneral.getKeyShortcut(event);
                if (pg < QM_APP)
                  return pg;
                std::string nm = g_eeGeneral.getKeyToolName(event);
                int idx = getLuaToolId(nm);
                if (idx >= 0)
                  return pg + idx;
                return QM_NONE;
              },
              [=](int32_t newValue) {
                if (newValue < QM_APP) {
                  g_eeGeneral.setKeyShortcut(event, (QMPage)newValue);
                  g_eeGeneral.setKeyToolName(event, "");
                } else {
                  g_eeGeneral.setKeyShortcut(event, QM_APP);
                  g_eeGeneral.setKeyToolName(event, getLuaTool(newValue - QM_APP)->label);
                }
                SET_DIRTY();
              }, STR_KEY_SHORTCUTS);

          if (c) {
            c->setAvailableHandler(
              [=](int pg) {
                if (pg == QM_NONE) return true;
                if (g_eeGeneral.hasKeyShortcut((QMPage)pg, event))
                  return false;
                return pg <= QM_UI_SCREEN1 || pg > QM_UI_ADD_PG; }
              );
          }
        });
  }
}

QMKeyShortcutsPage::QMKeyShortcutsPage(Route route):
        SubPage(ICON_RADIO, route, STR_MAIN_MENU_RADIO_SETTINGS, STR_KEY_SHORTCUTS, true)
{
  auto qmPages = QuickMenu::menuPageNames(false);

  // DESIGN SYSTEM: each key row is a plain label (key name) -> single control
  // (the shortcut page choice), i.e. a settings form. The two press groups are
  // ds::SectionHeader dividers; the rows are ds::FormRow grouped in ds::Cards
  // inside the page ds::List; the reset action is a ds::DSButton. The DS owns
  // margins, the 40/60 split and the touch floor.
  body->setFlexLayout();
  auto* list = new ds::List(body);

  new ds::SectionHeader(list, STR_SHORT_PRESS);
  auto* shortCard = new ds::Card(list);
  addKey(shortCard, EVT_KEY_BREAK(KEY_SYS), qmPages, "SYS");
  addKey(shortCard, EVT_KEY_BREAK(KEY_MODEL), qmPages, "MDL");
  addKey(shortCard, EVT_KEY_BREAK(KEY_TELE), qmPages, "TELE");

  new ds::SectionHeader(list, STR_LONG_PRESS);
  auto* longCard = new ds::Card(list);
  addKey(longCard, EVT_KEY_LONG(KEY_SYS), qmPages, "SYS");
  addKey(longCard, EVT_KEY_LONG(KEY_MODEL), qmPages, "MDL");
  addKey(longCard, EVT_KEY_LONG(KEY_TELE), qmPages, "TELE");

  new ds::DSButton(list, STR_SF_RESET, ds::ButtonRole::Secondary,
                   [=]() -> uint8_t {
                     g_eeGeneral.defaultKeyShortcuts();
                     SET_DIRTY();
                     return 0;
                   });

  enableRefresh();
}
