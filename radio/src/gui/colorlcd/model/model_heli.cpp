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

#include "model_heli.h"

#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "numberedit.h"
#include "sourcechoice.h"

#define SET_DIRTY()     storageDirty(EE_MODEL)

ModelHeliPage::ModelHeliPage(Route route):
  SubPage(ICON_MODEL_HELI, route, STR_MAIN_MENU_MODEL_SETTINGS, STR_MENUHELISETUP)
{
  body->setFlexLayout();

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): swash type/ring are plain
  // label+control lines (ds::FormRow); each swash channel packs a source
  // picker and its weight side-by-side on one line, so those are
  // ds::FieldRow (source | weight).
  Window* list = new ds::List(body);
  auto* card = new ds::Card(list);

  // Swash type
  new ds::FormRow(card, STR_SWASHTYPE, [=](Window* slot) {
    new Choice(slot, rect_t{}, STR_VSWASHTYPE, 0, SWASH_TYPE_MAX,
               GET_SET_DEFAULT(g_model.swashR.type));
  });

  // Swash ring
  new ds::FormRow(card, STR_SWASHRING, [=](Window* slot) {
    auto* swashRing = new NumberEdit(slot, rect_t{}, 0, 100,
                                     GET_SET_DEFAULT(g_model.swashR.value));
    swashRing->setDirectKeyboard(false);
    swashRing->setEditTitle(STR_ROLLER_SWASH_RING);
  });

  // Elevator source | weight
  new ds::FieldRow(
      card,
      {{STR_ELEVATOR,
        [=](Window* slot) {
          new SourceChoice(slot, rect_t{}, 0, MIXSRC_LAST_CH,
                           GET_SET_DEFAULT(g_model.swashR.elevatorSource));
        }},
       {STR_WEIGHT, [=](Window* slot) {
          auto* elevWeight = new NumberEdit(
              slot, rect_t{}, -100, 100,
              GET_SET_DEFAULT(g_model.swashR.elevatorWeight));
          elevWeight->setDirectKeyboard(false);
          elevWeight->setEditTitle(STR_ROLLER_ELEVATOR_WEIGHT);
        }}});

  // Aileron source | weight
  new ds::FieldRow(
      card,
      {{STR_AILERON,
        [=](Window* slot) {
          new SourceChoice(slot, rect_t{}, 0, MIXSRC_LAST_CH,
                           GET_SET_DEFAULT(g_model.swashR.aileronSource));
        }},
       {STR_WEIGHT, [=](Window* slot) {
          auto* ailWeight = new NumberEdit(
              slot, rect_t{}, -100, 100,
              GET_SET_DEFAULT(g_model.swashR.aileronWeight));
          ailWeight->setDirectKeyboard(false);
          ailWeight->setEditTitle(STR_ROLLER_AILERON_WEIGHT);
        }}});

  // Collective source | weight
  new ds::FieldRow(
      card,
      {{STR_COLLECTIVE,
        [=](Window* slot) {
          new SourceChoice(slot, rect_t{}, 0, MIXSRC_LAST_CH,
                           GET_SET_DEFAULT(g_model.swashR.collectiveSource));
        }},
       {STR_WEIGHT, [=](Window* slot) {
          auto* colWeight = new NumberEdit(
              slot, rect_t{}, -100, 100,
              GET_SET_DEFAULT(g_model.swashR.collectiveWeight));
          colWeight->setDirectKeyboard(false);
          colWeight->setEditTitle(STR_ROLLER_COLLECTIVE_WEIGHT);
        }}});
}
