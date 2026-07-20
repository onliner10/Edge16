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

#include "input_edit.h"

#include "curve_param.h"
#include "curveedit.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "fm_matrix.h"
#include "getset_helpers.h"
#include "gvar_numberedit.h"
#include "input_source.h"
#include "route.h"
#include "source_numberedit.h"
#include "switchchoice.h"
#include "tasks/mixer_task.h"
#include "textedit.h"

#define SET_DIRTY() storageDirty(EE_MODEL)
#define SET_INPUT_VALUE(value, _newValue)     \
  [=](int32_t newValue) {                     \
    MixerTaskLockGuard lock;                  \
    value = _newValue;                        \
    SET_DIRTY();                              \
  }

class InputEditAdvanced : public Page
{
 public:
  InputEditAdvanced(uint8_t input_n, uint8_t index, Route route) : Page(ICON_MODEL_INPUTS, route)
  {
    std::string title2(getSourceString(MIXSRC_FIRST_INPUT + input_n));
    header->setTitle(STR_MENUINPUTS);
    header->setTitle2(title2);

    body->setFlexLayout();

    ExpoData* input = expoAddress(index);

    // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the settings are a stack of DS
    // form rows (label 40% / control 60%, 40 px min) grouped in a Card. The
    // ds::List owns page margins and gaps; the DS owns the label/control
    // split — no per-screen coordinates.
    Window* list = new ds::List(body);
    auto* form = new ds::Card(list);

    // Side
    new ds::FormRow(form, STR_SIDE, [=](Window* slot) {
      new Choice(
          slot, rect_t{}, STR_VCURVEFUNC, 1, 3,
          [=]() -> int16_t { return 4 - input->mode; },
          [=](int16_t newValue) {
            {
              MixerTaskLockGuard lock;
              input->mode = 4 - newValue;
            }
            Messaging::send(Messaging::CURVE_UPDATE);
            SET_DIRTY();
          });
    });

    // Trim
    new ds::FormRow(form, STR_TRIM, [=](Window* slot) {
      const auto trimLast = TRIM_OFF + keysGetMaxTrims() - 1;
      auto c = new Choice(slot, rect_t{}, -TRIM_OFF, trimLast,
                          GET_VALUE(-input->trimSource),
                          SET_INPUT_VALUE(input->trimSource, -newValue));

      uint16_t srcRaw = input->srcRaw;
      c->setAvailableHandler([=](int value) {
        return value != TRIM_ON || srcRaw <= MIXSRC_LAST_STICK;
      });
      c->setTextHandler([=](int value) -> std::string {
        return getTrimSourceLabel(srcRaw, -value);
      });
    });

    // Flight modes
    if (modelFMEnabled()) {
      new ds::FormRow(form, STR_FLMODE, [=](Window* slot) {
        new FMMatrix<ExpoData>(slot, rect_t{}, input);
      });
    }
  }
};

InputEditWindow::InputEditWindow(int8_t input, uint8_t index, Route route) :
    Page(ICON_MODEL_INPUTS, route), input(input), index(index)
{
  header->setTitle(STR_MENUINPUTS);
  headerSwitchName = header->setTitle2("");

  headerSwitchName->textColor(COLOR_THEME_ACTIVE_INDEX, LV_STATE_USER_1);
  headerSwitchName->font(FONT_BOLD_INDEX, LV_STATE_USER_1);

  setTitle();

#if PORTRAIT
  body->padAll(PAD_ZERO);  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.

  auto box = new Window(body, rect_t{0, 0, body->width(), body->height() - INPUT_EDIT_CURVE_HEIGHT - PAD_TINY * 2});  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.
  box->scrollbar();
  box->padAll(PAD_SMALL);  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.

  auto form = new Window(box, rect_t{});
  buildBody(form);

  preview = new Curve(
      body, rect_t{(LCD_W - INPUT_EDIT_CURVE_WIDTH) / 2, body->height() - INPUT_EDIT_CURVE_HEIGHT - PAD_TINY, INPUT_EDIT_CURVE_WIDTH, INPUT_EDIT_CURVE_HEIGHT},  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.
      [=](int x) -> int {
        ExpoData* line = expoAddress(index);
        int16_t anas[MAX_INPUTS] = {0};
        applyExpos(anas, e_perout_mode_inactive_flight_mode, line->srcRaw, x);
        return anas[line->chn];
      },
      [=]() -> int { return getValue(expoAddress(index)->srcRaw); });
#else
  body->padAll(PAD_SMALL);  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.
  buildBody(body);

  preview = new Curve(
      this, rect_t{LCD_W - INPUT_EDIT_CURVE_WIDTH - PAD_LARGE, EdgeTxStyles::MENU_HEADER_HEIGHT + PAD_TINY, INPUT_EDIT_CURVE_WIDTH, INPUT_EDIT_CURVE_HEIGHT},  // ds-allow: input edit - settings box plus an absolutely-positioned curve-preview panel; mixed canvas+form page, not a plain DS form.
      [=](int x) -> int {
        ExpoData* line = expoAddress(index);
        int16_t anas[MAX_INPUTS] = {0};
        applyExpos(anas, e_perout_mode_inactive_flight_mode, line->srcRaw, x);
        return anas[line->chn];
      },
      [=]() -> int { return getValue(expoAddress(index)->srcRaw); });
#endif
}

void InputEditWindow::setTitle()
{
  headerSwitchName->setText(getSourceString(MIXSRC_FIRST_INPUT + input));
}

bool InputEditWindow::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_INPUT_ADVANCED) return false;
  new InputEditAdvanced(input, index, r);
  return true;
}

void InputEditWindow::buildBody(Window* form)
{
  form->setFlexLayout();

  ExpoData* input = expoAddress(index);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the settings are a stack of DS form
  // rows (label 40% / control 60%, 40 px min) grouped in a Card. The ds::List
  // owns page margins and gaps; the DS owns the label/control split — no
  // per-screen coordinates.
  Window* list = new ds::List(form);
  auto* card = new ds::Card(list);

  // Input Name
  new ds::FormRow(card, STR_INPUTNAME, [=](Window* slot) {
    new ModelTextEdit(slot, rect_t{}, g_model.inputNames[input->chn],
                      LEN_INPUT_NAME,
                      [=]() {
                        setTitle();
                      });
  });

  // Line Name
  new ds::FormRow(card, STR_EXPONAME, [=](Window* slot) {
    new ModelTextEdit(slot, rect_t{}, input->name, LEN_EXPOMIX_NAME);
  });

  // Source
  new ds::FormRow(card, STR_SOURCE, [=](Window* slot) {
    auto src = new InputSource(slot, input);
    src->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  });

  // Weight
  new ds::FormRow(card, STR_WEIGHT, [=](Window* slot) {
    auto gvar =
        new SourceNumberEdit(slot, -100, 100, GET_DEFAULT(input->weight),
                             [=](int32_t newValue) {
                               {
                                 MixerTaskLockGuard lock;
                                 input->weight = newValue;
                               }
                               updatePreview = true;
                               SET_DIRTY();
                             }, MIXSRC_FIRST);
    gvar->setSuffix("%");
    gvar->setDefault(100);
    gvar->setEditTitle(STR_ROLLER_INPUT_WEIGHT);
  });

  // Offset
  new ds::FormRow(card, STR_OFFSET, [=](Window* slot) {
    auto gvar = new SourceNumberEdit(slot, -100, 100,
                                GET_DEFAULT(input->offset), [=](int32_t newValue) {
                                  {
                                    MixerTaskLockGuard lock;
                                    input->offset = newValue;
                                  }
                                  updatePreview = true;
                                  SET_DIRTY();
                                }, MIXSRC_FIRST);
    gvar->setSuffix("%");
    gvar->setDefault(0);
    gvar->setEditTitle(STR_ROLLER_INPUT_OFFSET);
  });

  // Switch
  new ds::FormRow(card, STR_SWITCH, [=](Window* slot) {
    new SwitchChoice(slot, rect_t{}, SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES,
                     GET_DEFAULT(input->swtch),
                     [=](int newValue) {
                       {
                         MixerTaskLockGuard lock;
                         input->swtch = newValue;
                       }
                       updatePreview = true;
                       SET_DIRTY();
                     });
  });

  // Curve
  new ds::FormRow(card, STR_CURVE, [=](Window* slot) {
    auto param =
        new CurveParam(slot, rect_t{}, &input->curve,
          [=](int32_t newValue) {
            {
              MixerTaskLockGuard lock;
              input->curve.value = newValue;
            }
            updatePreview = true;
            SET_DIRTY();
          }, MIXSRC_FIRST, input->srcRaw);
    param->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  });

  // Advanced settings — icon-only full-width navigation action (no label, so
  // it isn't a label:control FormRow line); placed as the list's last item to
  // keep it directly under the settings card, same as before.
  auto btn = new TextButton(
      list, rect_t{}, LV_SYMBOL_SETTINGS, [=]() -> uint8_t {
        new InputEditAdvanced(this->input, index,
                            route().appended(RP_INPUT_ADVANCED, static_cast<int16_t>(index)));
        return 0;
      });
  btn->setWidth(lv_pct(100));
}

void InputEditWindow::onLiveCheckEvents(Window::LiveWindow& live)
{
  ExpoData* input = expoAddress(index);

  getvalue_t val;
  SourceNumVal v;

  v.rawValue = input->weight;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastWeightVal) {
      lastWeightVal = val;
      updatePreview = true;
    }
  }

  v.rawValue = input->offset;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastOffsetVal) {
      lastOffsetVal = val;
      updatePreview = true;
    }
  }

  v.rawValue = input->curve.value;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastCurveVal) {
      lastCurveVal = val;
      updatePreview = true;
    }
  }

  uint8_t activeIdx = 255;
  for (int i = 0; i < MAX_EXPOS; i += 1) {
    auto inp = expoAddress(i);
    if (inp->chn == input->chn) {
      if (getSwitch(inp->swtch)) {
        activeIdx = i;
        break;
      }
    }
  }
  if (activeIdx != lastActiveIndex) {
    updatePreview = true;
    lastActiveIndex = activeIdx;
  }

  if (lastActiveIndex == index) {
    headerSwitchName->addState(LV_STATE_USER_1);
  } else {
    headerSwitchName->clearState(LV_STATE_USER_1);
  }

  if (updatePreview) {
    updatePreview = false;
    Messaging::send(Messaging::CURVE_UPDATE);
  }

  Page::onLiveCheckEvents(live);
}
