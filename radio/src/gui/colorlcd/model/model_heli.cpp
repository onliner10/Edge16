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

#include "edgetx.h"
#include "getset_helpers.h"
#include "numberedit.h"
#include "sourcechoice.h"

#define SET_DIRTY()     storageDirty(EE_MODEL)

#if LANDSCAPE
static const lv_coord_t col_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1),
                                     LV_GRID_FR(1), LV_GRID_FR(2),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                     LV_GRID_TEMPLATE_LAST};
#endif

ModelHeliPage::ModelHeliPage(Route route):
  SubPage(ICON_MODEL_HELI, route, STR_MAIN_MENU_MODEL_SETTINGS, STR_MENUHELISETUP)
{
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  body->setFlexLayout();

  // Swash type
  auto line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_SWASHTYPE);
  new Choice(line, rect_t{}, STR_VSWASHTYPE, 0, SWASH_TYPE_MAX,
             GET_SET_DEFAULT(g_model.swashR.type));

  // Swash ring
  line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_SWASHRING);
  auto* swashRing = new NumberEdit(line, rect_t{}, 0, 100, GET_SET_DEFAULT(g_model.swashR.value));
  swashRing->setDirectKeyboard(false);
  swashRing->setEditTitle(STR_ROLLER_SWASH_RING);

  // Elevator source
  line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_ELEVATOR);
  new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_CH,
                   GET_SET_DEFAULT(g_model.swashR.elevatorSource));

  // Elevator weight
  auto w = new StaticText(line, rect_t{}, STR_WEIGHT, COLOR_THEME_PRIMARY1_INDEX, RIGHT);
  w->padRight(PAD_LARGE);
  auto* elevWeight = new NumberEdit(line, rect_t{}, -100, 100,
                 GET_SET_DEFAULT(g_model.swashR.elevatorWeight));
  elevWeight->setDirectKeyboard(false);
  elevWeight->setEditTitle(STR_ROLLER_ELEVATOR_WEIGHT);

  // Aileron source
  line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_AILERON);
  new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_CH,
                   GET_SET_DEFAULT(g_model.swashR.aileronSource));

  // Aileron weight
  w = new StaticText(line, rect_t{}, STR_WEIGHT, COLOR_THEME_PRIMARY1_INDEX, RIGHT);
  w->padRight(PAD_LARGE);
  auto* ailWeight = new NumberEdit(line, rect_t{}, -100, 100,
                 GET_SET_DEFAULT(g_model.swashR.aileronWeight));
  ailWeight->setDirectKeyboard(false);
  ailWeight->setEditTitle(STR_ROLLER_AILERON_WEIGHT);

  // Collective source
  line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_COLLECTIVE);
  new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_CH,
                   GET_SET_DEFAULT(g_model.swashR.collectiveSource));

  // Collective weight
  w = new StaticText(line, rect_t{}, STR_WEIGHT, COLOR_THEME_PRIMARY1_INDEX, RIGHT);
  w->padRight(PAD_LARGE);
  auto* colWeight = new NumberEdit(line, rect_t{}, -100, 100,
                 GET_SET_DEFAULT(g_model.swashR.collectiveWeight));
  colWeight->setDirectKeyboard(false);
  colWeight->setEditTitle(STR_ROLLER_COLLECTIVE_WEIGHT);


}
