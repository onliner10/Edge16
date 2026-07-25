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

#include "model_flightmodes.h"

#include <array>

#include "button.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "list_line_button.h"
#include "numberedit.h"
#include "page.h"
#include "route.h"
#include "switchchoice.h"
#include "textedit.h"
#include "ui_events.h"

#define SET_DIRTY()                  \
  do {                               \
    storageDirty(EE_MODEL);          \
    publishModelFlightModesChanged(); \
  } while (0)

static void publishModelFlightModesChanged()
{
  UiEventHub::publish(UiTopic::ModelFlightModesChanged);
}

static std::string getFMTrimStr(uint8_t mode, bool spacer)
{
  mode &= 0x1F;
  if (mode == TRIM_MODE_NONE) return "-";
  if (mode == TRIM_MODE_3POS) return "3P";
  std::string str((mode & 1) ? "+" : "=");
  if (spacer) str += " ";
  mode >>= 1;
  if (mode > MAX_FLIGHT_MODES - 1) mode = MAX_FLIGHT_MODES - 1;
  str += '0' + mode;
  return str;
}

static const lv_coord_t line_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_TEMPLATE_LAST};

static const lv_coord_t line_row_dsc[] = {LV_GRID_CONTENT,
                                          LV_GRID_TEMPLATE_LAST};

#if LANDSCAPE
static const lv_coord_t trims_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                           LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t trims_col_dsc[] = {LV_GRID_FR(1),
                                           LV_GRID_TEMPLATE_LAST};
#endif

class TrimEdit : public Window
{
 public:
  TrimEdit(Window* parent, int trimId, int fmId) :
      Window(parent, rect_t{}), trimId(trimId), fmId(fmId)
  {
    setWindowFlag(NO_FOCUS);

    padAll(PAD_TINY);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.
    setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL, LV_SIZE_CONTENT);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.

    trim_t* tr = &g_model.flightModeData[fmId].trim[trimId];

    lastTrim = tr->value;

    auto tr_btn = new TextButton(
        this, rect_t{0, 0, TR_BTN_W, 0}, getSourceString(MIXSRC_FIRST_TRIM + trimId),
        [=]() {
          tr->mode = (tr->mode == TRIM_MODE_NONE) ? 0 : TRIM_MODE_NONE;
          tr_mode->setValue(tr->mode);
          showControls();
          SET_DIRTY();
          return tr->mode == 0;
        });

    if (tr->mode != TRIM_MODE_NONE) tr_btn->check();

    tr_mode = new Choice(this, rect_t{0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, 0, 2 * MAX_FLIGHT_MODES,
                         GET_DEFAULT(tr->mode), [=](int val) {
                           tr->mode = val;
                           showControls();
                           SET_DIRTY();
                         });
    tr_mode->setTextHandler(
        [=](uint8_t mode) { return getFMTrimStr(mode, true); });
    tr_mode->setAvailableHandler([=](int mode) {
      if (fmId > 0)
        return ((mode & 1) == 0) || ((mode >> 1) != fmId) ||
               (mode == TRIM_MODE_3POS);
      return (mode == 0) || (mode == TRIM_MODE_3POS);
    });

    tr_value = new NumberEdit(
        this, rect_t{0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, g_model.extendedTrims ? -512 : -128,
        g_model.extendedTrims ? 512 : 128, GET_SET_DEFAULT(tr->value));
    tr_value->setDirectKeyboard(false);
    tr_value->setEditTitle(STR_ROLLER_TRIM_VALUE);

    showControls();
  }

  static LAYOUT_VAL_SCALED(TR_BTN_W, 65)  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.

 protected:
  int trimId;
  int fmId;
  int lastTrim;
  Choice* tr_mode = {nullptr};
  NumberEdit* tr_value = {nullptr};

  void showControls()
  {
    uint8_t mode = g_model.flightModeData[fmId].trim[trimId].mode;

    bool checked = (mode != TRIM_MODE_NONE);
    bool showValue = (fmId == 0 && mode != TRIM_MODE_3POS) || ((mode & 1) || (mode >> 1 == fmId));

    tr_mode->show(checked);
    tr_value->show(checked && showValue);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    const auto& fm = g_model.flightModeData[fmId];
    if (lastTrim != fm.trim[trimId].value) {
      lastTrim = fm.trim[trimId].value;
      tr_value->setValue(lastTrim);
    }
    Window::onLiveCheckEvents(live);
  }
};

class FlightModeEdit : public Page
{
 public:
  FlightModeEdit(uint8_t index, Route route) : Page(ICON_MODEL_FLIGHT_MODES, route), index(index)
  {
    std::string title2 = std::string(STR_FM) + std::to_string(index);
    header->setTitle(STR_MENUFLIGHTMODES);
    header->setTitle2(title2);

    FlexGridLayout grid(line_col_dsc, line_row_dsc, PAD_TINY);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.
    body->setFlexLayout();

    FlightModeData* p_fm = &g_model.flightModeData[index];

    // Flight mode name
    auto line = body->newLine(grid);
    new StaticText(line, rect_t{}, STR_NAME);
    new ModelTextEdit(line, rect_t{}, p_fm->name, LEN_FLIGHT_MODE_NAME);

    if (index > 0) {
      // Switch
      line = body->newLine(grid);
      new StaticText(line, rect_t{}, STR_SWITCH);
      new SwitchChoice(line, rect_t{}, SWSRC_FIRST_IN_MIXES,
                       SWSRC_LAST_IN_MIXES, GET_SET_DEFAULT(p_fm->swtch));
    }

    // Fade in
    line = body->newLine(grid);
    new StaticText(line, rect_t{}, STR_FADEIN);
    auto* fadeInEdit = new NumberEdit(line, rect_t{}, 0, DELAY_MAX, GET_DEFAULT(p_fm->fadeIn),
                   SET_VALUE(p_fm->fadeIn, newValue), PREC1);
    fadeInEdit->setSuffix("s");
    fadeInEdit->setDirectKeyboard(false);
    fadeInEdit->setEditTitle(STR_ROLLER_FADE_IN);

    // Fade out
    line = body->newLine(grid);
    new StaticText(line, rect_t{}, STR_FADEOUT);
    auto* fadeOutEdit = new NumberEdit(line, rect_t{}, 0, DELAY_MAX, GET_DEFAULT(p_fm->fadeOut),
                   SET_VALUE(p_fm->fadeOut, newValue), PREC1);
    fadeOutEdit->setSuffix("s");
    fadeOutEdit->setDirectKeyboard(false);
    fadeOutEdit->setEditTitle(STR_ROLLER_FADE_OUT);

    // Trims
    line = body->newLine(grid);
    new StaticText(line, rect_t{}, STR_TRIMS);

    FlexGridLayout trim_grid(trims_col_dsc, line_row_dsc, PAD_SMALL);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.

    for (int t = 0; t < keysGetMaxTrims(); t++) {
      if ((t % TRIMS_PER_LINE) == 0) {
        line = body->newLine(trim_grid);
        line->padAll(PAD_TINY);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.
        line->padLeft(10);  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.
      }

      new TrimEdit(line, t, index);
    }
  }

  static LAYOUT_SIZE(TRIMS_PER_LINE, 2, 1)  // ds-allow: flight-mode edit - custom per-trim composite (button+mode+value) laid out in a multi-control grid; not a single DS FormRow control.

 protected:
  uint8_t index;
};

class FlightModeBtn : public ListLineButton
{
 public:
  FlightModeBtn(Window* parent, int index) :
      ListLineButton(parent, index, LineDependencies::LiveValues)
  {
    padAll(PAD_ZERO);  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
    setHeight(BTN_H);
  }

  void onLineLoaded() override
  {
    if (!withLive([&](LiveWindow& live) {
          lastActive = isActive();
          check(lastActive);
          refreshCachedText();
          lv_obj_invalidate(live.lvobj());
          return true;
        }))
      return;

  }

  void describeLine(LineView& view) const override
  {
    view.text(FMID_X, FMID_Y, FMID_W, EdgeTxStyles::STD_FONT_HEIGHT,
              fmIDText.data());
    view.text(NAME_X, NAME_Y, NAME_W, EdgeTxStyles::STD_FONT_HEIGHT,
              fmNameText.data(), FONT(XS));
    view.text(SWTCH_X, SWTCH_Y, SWTCH_W, EdgeTxStyles::STD_FONT_HEIGHT,
              fmSwitchText.data());

    for (int i = 0; i < keysGetMaxTrims() && i < MAX_FMTRIMS; i += 1) {
      view.text(TRIM_X + i * TRIM_W, TRIM_Y, TRIM_W, TRIM_H,
                fmTrimModeText[i].data(), 0, LV_TEXT_ALIGN_CENTER);
      view.text(TRIM_X + i * TRIM_W, TRIM_Y + TRIM_H, TRIM_W, TRIM_H,
                fmTrimValueText[i].data(), FONT(XS), LV_TEXT_ALIGN_CENTER);
    }

    view.text(FADE_X, FADE_Y, FADE_W, EdgeTxStyles::STD_FONT_HEIGHT,
              fmFadeInText.data(), 0, LV_TEXT_ALIGN_RIGHT);
    view.text(FADE_X + FADE_W + PAD_TINY, FADE_Y, FADE_W,  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
              EdgeTxStyles::STD_FONT_HEIGHT, fmFadeOutText.data(), 0,
              LV_TEXT_ALIGN_RIGHT);
  }

  bool isActive() const override { return (getFlightMode() == index); }

  void setTrimValue(uint8_t t)
  {
    lastTrim[t] = g_model.flightModeData[index].trim[t].value;

    uint8_t mode = g_model.flightModeData[index].trim[t].mode;
    bool checked = (mode != TRIM_MODE_NONE);
    bool showValue = (index == 0) || ((mode & 1) || (mode >> 1 == index));

    if (checked && showValue)
      formatNumberAsString(fmTrimValueText[t].data(),
                           fmTrimValueText[t].size(), lastTrim[t]);
    else
      fmTrimValueText[t][0] = '\0';
  }

  void onLinePresentationSync(LiveWindow& live) override
  {
    if (!refreshing) {
      refreshing = true;
      const bool active = isActive();
      bool changed = active != lastActive;
      if (changed) {
        lastActive = active;
        check(active);
      }
      for (int t = 0; t < keysGetMaxTrims() && t < MAX_FMTRIMS; t += 1) {
        const auto& trim = g_model.flightModeData[index].trim[t];
        if (lastTrim[t] != trim.value || lastTrimMode[t] != trim.mode) {
          lastTrimMode[t] = trim.mode;
          setText(fmTrimModeText[t], getFMTrimStr(trim.mode, false).c_str());
          setTrimValue(t);
          changed = true;
        }
      }
      if (changed) lv_obj_invalidate(live.lvobj());
      refreshing = false;
    }
  }

  void onRefresh() override
  {
    refreshCachedText();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

  static LAYOUT_SIZE_SCALED(BTN_H, 36, 56)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE(MAX_FMTRIMS, 6, 4)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static constexpr coord_t FMID_X = PAD_TINY;  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(FMID_Y, 6, 16)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(FMID_W, 36, 46)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static constexpr coord_t NAME_X = FMID_X + FMID_W + PAD_TINY;  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(NAME_Y, 6, 0)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(NAME_W, 95, 160)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static constexpr coord_t SWTCH_X = NAME_X + NAME_W + PAD_TINY;  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(SWTCH_Y, 6, 0)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_VAL_SCALED(SWTCH_W, 50)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE(TRIM_X, SWTCH_X + SWTCH_W + PAD_TINY, FMID_X + FMID_W + PAD_TINY)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(TRIM_Y, 0, 20)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(TRIM_W, 30, 40)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_VAL_SCALED(TRIM_H, 16)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static constexpr coord_t TRIMC_W = MAX_FMTRIMS * TRIM_W;
  static LAYOUT_VAL_SCALED(FADE_W, 45)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static LAYOUT_SIZE_SCALED(FADE_Y, 6, 24)  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  static constexpr coord_t FADE_X = ListLineButton::GRP_W - PAD_BORDER * 2 - FADE_W * 2 - PAD_TINY * 2;  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.

 protected:
  template <size_t N>
  void setText(std::array<char, N>& dest, const char* text)
  {
    dest[0] = '\0';
    strAppend(dest.data(), text ? text : "", N - 1);
  }

  void refreshCachedText()
  {
    const auto& fm = g_model.flightModeData[index];

    fmIDText[0] = '\0';
    getFlightModeString(fmIDText.data(), index + 1);
    setAutomationText(fmIDText.data());

    setText(fmNameText, fm.name[0] != '\0' ? fm.name : "");

    if ((index > 0) && (fm.swtch != SWSRC_NONE)) {
      char label[16] = {};
      getSwitchPositionName(label, fm.swtch);
      setText(fmSwitchText, label);
    } else {
      fmSwitchText[0] = '\0';
    }

    for (int i = 0; i < keysGetMaxTrims() && i < MAX_FMTRIMS; i += 1) {
      lastTrimMode[i] = fm.trim[i].mode;
      setText(fmTrimModeText[i], getFMTrimStr(fm.trim[i].mode, false).c_str());
      setTrimValue(i);
    }

    formatNumberAsString(fmFadeInText.data(), fmFadeInText.size(), fm.fadeIn,
                         PREC1, 0, nullptr, "s");
    formatNumberAsString(fmFadeOutText.data(), fmFadeOutText.size(),
                         fm.fadeOut, PREC1, 0, nullptr, "s");
  }

  bool refreshing = false;
  bool lastActive = false;
  int lastTrim[MAX_FMTRIMS] = {0};
  uint8_t lastTrimMode[MAX_FMTRIMS] = {0};
  std::array<char, 8> fmIDText = {};
  std::array<char, 32> fmNameText = {};
  std::array<char, 16> fmSwitchText = {};
  std::array<std::array<char, 16>, MAX_FMTRIMS> fmTrimModeText = {};
  std::array<std::array<char, 16>, MAX_FMTRIMS> fmTrimValueText = {};
  std::array<char, 16> fmFadeInText = {};
  std::array<char, 16> fmFadeOutText = {};
};

ModelFlightModesPage::ModelFlightModesPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

bool ModelFlightModesPage::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_FLIGHT_MODE_EDIT) return false;

  uint8_t index = static_cast<uint8_t>(r.params[depth]);
  if (index >= MAX_FLIGHT_MODES) return false;

  (new FlightModeEdit(index, r))->setCloseHandler([=]() {
    if (pageWindow) {
      pageWindow->clear();
      build(pageWindow);
    }
  });
  return true;
}

static const lv_coord_t fmt_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

static const lv_coord_t fmt_row_dsc[] = {LV_GRID_CONTENT,
                                         LV_GRID_TEMPLATE_LAST};

void ModelFlightModesPage::build(Window* form)
{
  pageWindow = form;
  form->padAll(PAD_ZERO);  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
  form->padBottom(PAD_LARGE);  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.

  for (int i = 0; i < MAX_FLIGHT_MODES; i++) {
    auto btn = new FlightModeBtn(form, i);
    btn->setPos(PAD_SMALL, i * (FlightModeBtn::BTN_H + PAD_THREE) + PAD_SMALL);  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
    btn->setWidth(ListLineButton::GRP_W);

    btn->setPressHandler([=]() {
      (new FlightModeEdit(i, route().appended(RP_FLIGHT_MODE_EDIT, static_cast<int16_t>(i))))->setCloseHandler([=]() {
        btn->requestLineUpdate();
        publishModelFlightModesChanged();
      });
      return 0;
    });
  }

  trimCheck = new TextButton(
      form, rect_t{6, MAX_FLIGHT_MODES * (FlightModeBtn::BTN_H + PAD_THREE) + PAD_LARGE, ListLineButton::GRP_W, EdgeTxStyles::UI_ELEMENT_HEIGHT}, STR_CHECKTRIMS, [&]() -> uint8_t {  // ds-allow: flight-mode list row draws per-trim two-line stacked (mode over value) cells plus id/name/switch/fade columns at absolute offsets; ds::Grid's single-text cells can't represent the stacked trim pairs.
        if (trimsCheckTimer)
          trimsCheckTimer = 0;
        else
          trimsCheckTimer = 200;  // 2 seconds trims cancelled
        return trimsCheckTimer;
      });
}

void ModelFlightModesPage::checkEvents()
{
  trimCheck->check(trimsCheckTimer > 0);
}
