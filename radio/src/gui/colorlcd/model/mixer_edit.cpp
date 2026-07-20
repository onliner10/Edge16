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

#include "mixer_edit.h"

#include "channel_bar.h"
#include "curve_param.h"
#include "curveedit.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "gvar_numberedit.h"
#include "mixer_edit_adv.h"
#include "mixes.h"
#include "pagegroup.h"
#include "route.h"
#include "source_numberedit.h"
#include "sourcechoice.h"
#include "switchchoice.h"
#include "tasks/mixer_task.h"
#include "textedit.h"

#define SET_DIRTY() storageDirty(EE_MODEL)
#define SET_MIXER_DEFAULT(value)              \
  [=](int32_t newValue) {                     \
    MixerTaskLockGuard lock;                  \
    value = newValue;                         \
    SET_DIRTY();                              \
  }

class MixerEditStatusBar : public Window
{
 public:
  MixerEditStatusBar(Window *parent, const rect_t &rect, int8_t channel) :
      Window(parent, rect), _channel(channel)
  {
    channelBar =
        new ComboChannelBar(this,
                            {MIX_STATUS_BAR_MARGIN, 0,
                             rect.w - (MIX_STATUS_BAR_MARGIN * 2), rect.h},
                            channel, true);
  }

  static LAYOUT_SIZE_SCALED(MIX_STATUS_BAR_MARGIN, 3, 0)  // ds-allow: mixer edit - margin constant for the fixed-width channel status bar embedded in the header; not a DS list element.

 protected:
  ComboChannelBar *channelBar;
  int8_t _channel;
};

MixEditWindow::MixEditWindow(int8_t channel, uint8_t index, Route route) :
    Page(ICON_MODEL_MIXER, route), channel(channel), index(index)
{
  buildBody(body);
  buildHeader(header);
}

void MixEditWindow::buildHeader(Window *window)
{
  std::string title2(getSourceString(MIXSRC_FIRST_CH + channel));
  header->setTitle(STR_MIXES);
  header->setTitle2(title2);

  new MixerEditStatusBar(
      window,
      {window->getRect().w - MIX_STATUS_BAR_WIDTH - PageGroup::PAGE_GROUP_BACK_BTN_W, 0,
       MIX_STATUS_BAR_WIDTH, EdgeTxStyles::MENU_HEADER_HEIGHT},
      channel);
}

bool MixEditWindow::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_MIX_ADVANCED) return false;
  new MixEditAdvanced(channel, index, r);
  return true;
}

void MixEditWindow::buildBody(Window *form)
{
  form->setFlexLayout();

  MixData *mix = mixAddress(index);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the mix settings form is a ds::List of
  // ds::FormRow / ds::FieldRow lines grouped in a ds::Card. The List owns page
  // margins and inter-row gaps; single-control lines are FormRows, the two
  // side-by-side lines (weight/offset, switch/curve) are FieldRows. No
  // per-screen grid template or coordinates.
  Window* list = new ds::List(form);
  auto* card = new ds::Card(list);

  // Mix name
  new ds::FormRow(card, STR_NAME, [=](Window* slot) {
    new ModelTextEdit(slot, rect_t{}, mix->name, sizeof(mix->name));
  });

  // Source
  new ds::FormRow(card, STR_SOURCE, [=](Window* slot) {
    auto sourceChoice = new SourceChoice(slot, rect_t{}, 0, MIXSRC_MODEL_ARMED,
                                         GET_DEFAULT(mix->srcRaw),
                                         SET_MIXER_DEFAULT(mix->srcRaw), true);
    sourceChoice->setAvailableHandler([](int source) {
      source = abs(source);
      return source == MIXSRC_MODEL_ARMED ||
             (source <= MIXSRC_LAST && isSourceAvailable(source));
    });
  });

  // Weight | Offset
  new ds::FieldRow(
      card,
      {{STR_WEIGHT,
        [=](Window* slot) {
          auto svar = new SourceNumberEdit(slot, MIX_WEIGHT_MIN, MIX_WEIGHT_MAX,
                                           GET_DEFAULT(mix->weight),
                                           SET_MIXER_DEFAULT(mix->weight),
                                           MIXSRC_FIRST);
          svar->setSuffix("%");
          svar->setDefault(100);
          svar->setEditTitle(STR_ROLLER_MIXER_WEIGHT);
        }},
       {STR_OFFSET, [=](Window* slot) {
          auto gvar = new SourceNumberEdit(slot, MIX_OFFSET_MIN, MIX_OFFSET_MAX,
                                           GET_DEFAULT(mix->offset),
                                           SET_MIXER_DEFAULT(mix->offset),
                                           MIXSRC_FIRST);
          gvar->setSuffix("%");
          gvar->setDefault(0);
          gvar->setEditTitle(STR_ROLLER_MIXER_OFFSET);
        }}});

  // Switch | Curve
  new ds::FieldRow(
      card,
      {{STR_SWITCH,
        [=](Window* slot) {
          new SwitchChoice(slot, rect_t{}, SWSRC_FIRST_IN_MIXES,
                           SWSRC_LAST_IN_MIXES, GET_DEFAULT(mix->swtch),
                           SET_MIXER_DEFAULT(mix->swtch));
        }},
       {STR_CURVE, [=](Window* slot) {
          new CurveParam(slot, rect_t{}, &mix->curve,
                         SET_MIXER_DEFAULT(mix->curve.value), MIXSRC_FIRST,
                         mix->srcRaw);
        }}});

  // Advanced settings — a full-width action button that opens the advanced
  // page. Not a form field, so it sits in the list below the form card; the
  // List owns its margins and gap.
  auto btn =
      new TextButton(list, rect_t{}, LV_SYMBOL_SETTINGS, [=]() -> uint8_t {
        new MixEditAdvanced(channel, index,
                            route().appended(RP_MIX_ADVANCED, static_cast<int16_t>(index)));
        return 0;
      });
  btn->setWidth(lv_pct(100));
}
