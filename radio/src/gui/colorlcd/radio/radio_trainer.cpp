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

#include "radio_trainer.h"

#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "hal/adc_driver.h"
#include "input_mapping.h"
#include "numberedit.h"
#include "static.h"
#include "strhelpers.h"

#define SET_DIRTY() storageDirty(EE_GENERAL)

RadioTrainerPage::RadioTrainerPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

#if LANDSCAPE
static const lv_coord_t col_dsc[] = {LV_GRID_FR(7),  LV_GRID_FR(13),
                                     LV_GRID_FR(10), LV_GRID_FR(10),
                                     LV_GRID_FR(10), LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t col_dsc[] = {LV_GRID_FR(7), LV_GRID_FR(15),
                                     LV_GRID_FR(9), LV_GRID_FR(9),
                                     LV_GRID_TEMPLATE_LAST};
#endif

static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

void RadioTrainerPage::build(Window* form)
{
  form->padAll(PAD_SMALL);  // ds-allow: trainer setup — outer page padding for a form of custom multi-control stick lines; not a single DS FormRow control.

  if (SLAVE_MODE()) {
    auto txt = new StaticText(form, rect_t{}, STR_SLAVE, COLOR_THEME_PRIMARY1_INDEX, FONT(L));
    txt->align(LV_ALIGN_CENTER);
  } else {
    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);  // ds-allow: trainer setup — column gap for the custom multi-column stick grid packing mode/chan/weight per line; not a single DS FormRow control.
    form->setFlexLayout();

    auto max_sticks = adcGetMaxInputs(ADC_INPUT_MAIN);
    for (uint8_t i = 0; i < max_sticks; i++) {
      uint8_t chan = inputMappingChannelOrder(i);
      TrainerMix* td = &g_eeGeneral.trainer.mix[chan];

      auto line = form->newLine(grid);
      new StaticText(line, rect_t{}, getMainControlLabel(chan));

      new Choice(line, rect_t{}, STR_TRNMODE, 0, 2, GET_SET_DEFAULT(td->mode));
      new Choice(line, rect_t{}, STR_TRNCHN, 0, 3, GET_SET_DEFAULT(td->srcChn));
      auto weight = new NumberEdit(line, rect_t{0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, -125, 125,
                                   GET_SET_DEFAULT(td->studWeight));
      weight->setSuffix("%");
      weight->setEditTitle(std::string(getMainControlLabel(chan)) + " " + STR_WEIGHT);
      weight->setDirectKeyboard(false);

#if PORTRAIT
      line = form->newLine(grid);
      line->padLeft(PAD_LARGE * 3 + PAD_MEDIUM);  // ds-allow: trainer setup — portrait indentation of the wrapped continuation line under each stick row; not a single DS FormRow control.
      line->padBottom(PAD_LARGE);  // ds-allow: trainer setup — portrait spacing after the wrapped continuation line under each stick row; not a single DS FormRow control.
#endif

      LcdFlags flags = LEFT;
      if (g_eeGeneral.ppmunit == PPM_PERCENT_PREC1) flags |= PREC1;

      new DynamicNumber<int16_t>(
          line, rect_t{},
          [=]() {
            return (trainerInput[i] - g_eeGeneral.trainer.calib[i]) * 2;
          },
          COLOR_THEME_PRIMARY1_INDEX, flags);
    }

    // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the multiplier/calibration
    // settings below the per-stick calibration table are a stack of
    // ds::FormRow (label 40% / control 60%, 40 px min) grouped in a Card.
    // The ds::List owns the gap from the table above and the row spacing
    // below — no per-screen coordinates. The per-stick table above keeps its
    // own hand-rolled FlexGridLayout grid (column-aligned mode/chan/weight
    // readout table) untouched; it is not a settings-form line.
    Window* list = new ds::List(form);
    auto* card = new ds::Card(list);

    // Trainer multiplier
    if (g_model.trainerData.mode == TRAINER_MODE_MASTER_TRAINER_JACK) {
      new ds::FormRow(card, STR_MULTIPLIER, [](Window* slot) {
        auto multiplier = new NumberEdit(
            slot, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, -10, 40,
            GET_SET_DEFAULT(g_eeGeneral.PPM_Multiplier));
        multiplier->setDisplayHandler([](int32_t value) {
          return formatNumberAsString(value + 10, PREC1);
        });
      });
    }

    // Trainer calibration
    new ds::FormRow(card, "", [](Window* slot) {
      new TextButton(slot, rect_t{}, STR_MENUCALIBRATION, []() -> uint8_t {
        memcpy(g_eeGeneral.trainer.calib, trainerInput,
               sizeof(g_eeGeneral.trainer.calib));
        SET_DIRTY();
        return 0;
      });
    });
  }
}
