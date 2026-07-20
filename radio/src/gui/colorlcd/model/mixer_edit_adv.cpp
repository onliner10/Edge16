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

#include "mixer_edit_adv.h"

#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "fm_matrix.h"
#include "getset_helpers.h"
#include "mixes.h"
#include "numberedit.h"
#include "tasks/mixer_task.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)
#define SET_MIXER_DEFAULT(value)              \
  [=](int32_t newValue) {                     \
    MixerTaskLockGuard lock;                  \
    value = newValue;                         \
    SET_DIRTY();                              \
  }
#define SET_MIXER_VALUE(value, _newValue)     \
  [=](int32_t newValue) {                     \
    MixerTaskLockGuard lock;                  \
    value = _newValue;                        \
    SET_DIRTY();                              \
  }
#define SET_MIXER_INVERTED(value)             \
  [=](uint8_t newValue) {                     \
    MixerTaskLockGuard lock;                  \
    value = !newValue;                        \
    SET_DIRTY();                              \
  }

MixEditAdvanced::MixEditAdvanced(int8_t channel, uint8_t index, Route route) :
    Page(ICON_MODEL_MIXER, route), channel(channel), index(index)
{
  std::string title2(getSourceString(MIXSRC_FIRST_CH + channel));
  header->setTitle(STR_MIXES);
  header->setTitle2(title2);

  buildBody(body);
}

void MixEditAdvanced::buildBody(Window* form)
{
  form->setFlexLayout();

  MixData* mix = mixAddress(index);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the advanced mix settings are a
  // ds::List of ds::FormRow / ds::FieldRow lines grouped in a ds::Card.
  // Single-control lines (multiplex, flight modes, the delay/slow precision
  // selectors) are FormRows; the paired lines (trim/warning, delay up/down,
  // slow up/down) are FieldRows. Conditional lines (multiplex, flight modes)
  // are still built only when their gate is satisfied, exactly as before.
  Window* list = new ds::List(form);
  auto* card = new ds::Card(list);

  // Multiplex
  if (index > 0 && mixAddress(index - 1)->destCh == channel) {
    new ds::FormRow(card, STR_MULTPX, [=](Window* slot) {
      new Choice(slot, rect_t{}, STR_VMLTPX, 0, 2, GET_DEFAULT(mix->mltpx),
                 SET_MIXER_DEFAULT(mix->mltpx));
    });
  }

  // Flight modes
  if (modelFMEnabled()) {
    new ds::FormRow(card, STR_FLMODE, [=](Window* slot) {
      new FMMatrix<MixData>(slot, rect_t{}, mix);
    });
  }

  // Trim | Warning
  new ds::FieldRow(
      card,
      {{STR_TRIM,
        [=](Window* slot) {
          new ToggleSwitch(slot, rect_t{}, GET_INVERTED(mix->carryTrim),
                           SET_MIXER_INVERTED(mix->carryTrim));
        }},
       {STR_MIXWARNING, [=](Window* slot) {
          auto edit = new NumberEdit(slot, rect_t{}, 0, 3,
                                     GET_DEFAULT(mix->mixWarn),
                                     SET_MIXER_DEFAULT(mix->mixWarn));
          edit->setZeroText(STR_OFF);
          edit->setDirectKeyboard(false);
          edit->setEditTitle(STR_ROLLER_MIX_WARNING);
        }}});

  // Delay up/down precision
  new ds::FormRow(card, STR_MIX_DELAY_PREC, [=](Window* slot) {
    new Choice(slot, rect_t{}, &STR_VPREC[1], 0, 1,
               GET_DEFAULT(mix->delayPrec), [=](int newValue) {
                 {
                   MixerTaskLockGuard lock;
                   mix->delayPrec = newValue;
                 }
                 delayUp->clearTextFlag(PREC2);
                 delayUp->setTextFlag(newValue ? PREC2 : PREC1);
                 delayUp->update();
                 delayDn->clearTextFlag(PREC2);
                 delayDn->setTextFlag(newValue ? PREC2 : PREC1);
                 delayDn->update();
                 SET_DIRTY();
               });
  });

  // Delay up | Delay down
  new ds::FieldRow(
      card,
      {{STR_DELAYUP,
        [=](Window* slot) {
          delayUp = new NumberEdit(slot, rect_t{}, 0, DELAY_MAX,
                                   GET_DEFAULT(mix->delayUp),
                                   SET_MIXER_VALUE(mix->delayUp, newValue),
                                   mix->delayPrec ? PREC2 : PREC1);
          delayUp->setSuffix("s");
          delayUp->setDirectKeyboard(false);
          delayUp->setEditTitle(STR_ROLLER_DELAY_UP);
        }},
       {STR_DELAYDOWN, [=](Window* slot) {
          delayDn = new NumberEdit(slot, rect_t{}, 0, DELAY_MAX,
                                   GET_DEFAULT(mix->delayDown),
                                   SET_MIXER_VALUE(mix->delayDown, newValue),
                                   mix->delayPrec ? PREC2 : PREC1);
          delayDn->setSuffix("s");
          delayDn->setDirectKeyboard(false);
          delayDn->setEditTitle(STR_ROLLER_DELAY_DOWN);
        }}});

  // Slow up/down precision
  new ds::FormRow(card, STR_MIX_SLOW_PREC, [=](Window* slot) {
    new Choice(slot, rect_t{}, &STR_VPREC[1], 0, 1,
               GET_DEFAULT(mix->speedPrec), [=](int newValue) {
                 {
                   MixerTaskLockGuard lock;
                   mix->speedPrec = newValue;
                 }
                 slowUp->clearTextFlag(PREC2);
                 slowUp->setTextFlag(newValue ? PREC2 : PREC1);
                 slowUp->update();
                 slowDn->clearTextFlag(PREC2);
                 slowDn->setTextFlag(newValue ? PREC2 : PREC1);
                 slowDn->update();
                 SET_DIRTY();
               });
  });

  // Slow up | Slow down
  new ds::FieldRow(
      card,
      {{STR_SLOWUP,
        [=](Window* slot) {
          slowUp = new NumberEdit(slot, rect_t{}, 0, DELAY_MAX,
                                  GET_DEFAULT(mix->speedUp),
                                  SET_MIXER_VALUE(mix->speedUp, newValue),
                                  mix->speedPrec ? PREC2 : PREC1);
          slowUp->setSuffix("s");
          slowUp->setDirectKeyboard(false);
          slowUp->setEditTitle(STR_ROLLER_SLOW_UP);
        }},
       {STR_SLOWDOWN, [=](Window* slot) {
          slowDn = new NumberEdit(slot, rect_t{}, 0, DELAY_MAX,
                                  GET_DEFAULT(mix->speedDown),
                                  SET_MIXER_VALUE(mix->speedDown, newValue),
                                  mix->speedPrec ? PREC2 : PREC1);
          slowDn->setSuffix("s");
          slowDn->setDirectKeyboard(false);
          slowDn->setEditTitle(STR_ROLLER_SLOW_DOWN);
        }}});
}
