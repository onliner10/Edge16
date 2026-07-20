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

#include "output_edit.h"

#include "channel_bar.h"
#include "curve_param.h"
#include "curveedit.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "gvar_numberedit.h"
#include "pagegroup.h"
#include "textedit.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

#define ETX_STATE_MINMAX_HIGHLIGHT LV_STATE_USER_1

class OutputEditStatusBar : public Window
{
 public:
  OutputEditStatusBar(Window *parent, const rect_t &rect, int8_t channel) :
      Window(parent, rect), _channel(channel)
  {
    channelBar = new ComboChannelBar(
        this,
        {OUTPUT_EDIT_STATUS_BAR_MARGIN, 0,
         rect.w - (OUTPUT_EDIT_STATUS_BAR_MARGIN * 2), rect.h},
        channel, true);
  }

  static LAYOUT_SIZE_SCALED(OUTPUT_EDIT_STATUS_BAR_MARGIN, 3, 0)  // ds-allow: output edit - margin constant for the fixed-width channel status bar embedded in the header; not a DS list element.

 protected:
  ComboChannelBar *channelBar;
  int8_t _channel;
};

OutputEditWindow::OutputEditWindow(uint8_t channel, Route route) :
    Page(ICON_MODEL_OUTPUTS, route), channel(channel)
{
  std::string title2(getSourceString(MIXSRC_FIRST_CH + channel));
  header->setTitle(STR_MENULIMITS);
  header->setTitle2(title2);

  buildHeader(header);
  buildBody(body);
}

void OutputEditWindow::onLiveCheckEvents(Window::LiveWindow& live)
{
  int newValue = getChannelOutput(channel);
  if (value != newValue) {
    value = newValue;

    int chanVal = calcRESXto100(getRawChannelOutput(channel));

    bool minOn = chanVal < -DEADBAND;
    bool maxOn = chanVal > DEADBAND;

    // The field LABELS light up through the DS (active fill + bold); the EDITS
    // go bold through their own ETX_STATE_MINMAX_HIGHLIGHT font — same combined
    // cue as before, now with the DS owning the row geometry.
    if (minMaxRow) {
      minMaxRow->highlightField(0, minOn);
      minMaxRow->highlightField(1, maxOn);
    }
    if (minEdit) {
      if (minOn)
        minEdit->addState(ETX_STATE_MINMAX_HIGHLIGHT);
      else
        minEdit->clearState(ETX_STATE_MINMAX_HIGHLIGHT);
    }
    if (maxEdit) {
      if (maxOn)
        maxEdit->addState(ETX_STATE_MINMAX_HIGHLIGHT);
      else
        maxEdit->clearState(ETX_STATE_MINMAX_HIGHLIGHT);
    }
  }

  Window::onLiveCheckEvents(live);
}

void OutputEditWindow::buildHeader(Window *window)
{
  statusBar = new OutputEditStatusBar(
      window,
      {window->getRect().w - OUTPUT_EDIT_STATUS_BAR_WIDTH - PageGroup::PAGE_GROUP_BACK_BTN_W,
       0, OUTPUT_EDIT_STATUS_BAR_WIDTH, EdgeTxStyles::MENU_HEADER_HEIGHT},
      channel);
}

void OutputEditWindow::buildBody(Window *form)
{
  form->setFlexLayout();

  int limit = (g_model.extendedLimits ? LIMIT_EXT_MAX : LIMIT_STD_MAX);
  LimitData *output = limitAddress(channel);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): each limits line carries two controls
  // laid out side-by-side, so it is a ds::FieldRow (two even 40%/60%
  // label+control fields). The ds::List owns page margins and inter-row gaps;
  // the ds::Card groups the lines. No per-screen coordinates or grid templates.
  Window* list = new ds::List(form);
  auto* card = new ds::Card(list);

  // In percent mode the default PREC1 formatting is used so the rotary
  // wheel's additive split layout stays available; the custom display
  // handler is only installed for the µs unit, whose x128/25 scaling cannot
  // be represented by additive fine offsets (those fields keep the keypad).
  auto usDisplayHandler = [](GVarNumberEdit* fld) {
    if (g_eeGeneral.ppmunit != PPM_US) return;
    fld->setDisplayHandler([](int value) {
      return formatNumberAsString(value * 128 / 25, PREC1);
    });
  };

  // Name | Offset
  new ds::FieldRow(
      card,
      {{STR_NAME,
        [=](Window* slot) {
          new ModelTextEdit(slot, rect_t{}, output->name,
                            sizeof(output->name));
        }},
       {STR_LIMITS_HEADERS_SUBTRIM, [=](Window* slot) {
          auto off =
              new GVarNumberEdit(slot, -LIMIT_STD_MAX, +LIMIT_STD_MAX,
                                 GET_SET_DEFAULT(output->offset), PREC1, 0, 0);
          off->setFastStep(20);
          off->setAccelFactor(16);
          off->setEditTitle(STR_LIMITS_HEADERS_SUBTRIM);
          usDisplayHandler(off);
        }}});

  // Min | Max — the field labels light up (highlightField, see
  // onLiveCheckEvents) toward the side the channel is currently pushed, and the
  // matching edit goes bold via its own ETX_STATE_MINMAX_HIGHLIGHT font.
  minMaxRow = new ds::FieldRow(
      card,
      {{STR_MIN,
        [=](Window* slot) {
          minEdit = new GVarNumberEdit(slot, -limit, 0,
                                       GET_SET_DEFAULT(output->min), PREC1,
                                       -LIMIT_STD_MAX, -limit);
          minEdit->font(FONT_BOLD_INDEX, ETX_STATE_MINMAX_HIGHLIGHT);
          minEdit->setFastStep(20);
          minEdit->setAccelFactor(16);
          minEdit->setEditTitle(STR_MIN);
          usDisplayHandler(minEdit);
        }},
       {STR_MAX, [=](Window* slot) {
          maxEdit = new GVarNumberEdit(slot, 0, +limit,
                                       GET_SET_DEFAULT(output->max), PREC1,
                                       +LIMIT_STD_MAX, limit);
          maxEdit->font(FONT_BOLD_INDEX, ETX_STATE_MINMAX_HIGHLIGHT);
          maxEdit->setFastStep(20);
          maxEdit->setAccelFactor(16);
          maxEdit->setEditTitle(STR_MAX);
          usDisplayHandler(maxEdit);
        }}});

  // Inverted | Curve
  new ds::FieldRow(
      card,
      {{STR_INVERTED,
        [=](Window* slot) {
          new ToggleSwitch(slot, rect_t{}, GET_DEFAULT(output->revert),
                           [output, this](uint8_t newValue) {
                             output->revert = newValue;
                             SET_DIRTY();
                           });
        }},
       {STR_CURVE, [=](Window* slot) {
          new CurveChoice(slot, GET_SET_DEFAULT(output->curve),
                          channel + MIXSRC_FIRST_CH);
        }}});

  // PPM center | Subtrims mode
  new ds::FieldRow(
      card,
      {{STR_LIMITS_HEADERS_PPMCENTER,
        [=](Window* slot) {
          auto center = new NumberEdit(
              slot, rect_t{}, PPM_CENTER - PPM_CENTER_MAX,
              PPM_CENTER + PPM_CENTER_MAX,
              GET_VALUE(output->ppmCenter + PPM_CENTER),
              SET_VALUE(output->ppmCenter, newValue - PPM_CENTER));
          center->setFastStep(20);
          center->setAccelFactor(8);
          center->setDefault(PPM_CENTER);
          center->setDirectKeyboard(false);
          center->setEditTitle(STR_LIMITS_HEADERS_PPMCENTER);
        }},
       {STR_LIMITS_HEADERS_SUBTRIMMODE, [=](Window* slot) {
          new Choice(slot, rect_t{}, STR_SUBTRIMMODES, 0, 1,
                     GET_SET_DEFAULT(output->symetrical));
        }}});
}
