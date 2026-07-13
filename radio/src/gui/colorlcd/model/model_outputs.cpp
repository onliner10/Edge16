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

#include "model_outputs.h"

#include <array>

#include "channel_bar.h"
#include "dialog.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "list_line_button.h"
#include "menu.h"
#include "messaging.h"
#include "output_edit.h"
#include "page.h"
#include "route.h"
#include "toggleswitch.h"
#include "ui_events.h"

#define SET_DIRTY()                \
  do {                             \
    storageDirty(EE_MODEL);        \
    publishModelOutputsChanged();  \
  } while (0)

#define ETX_STATE_MINMAX_BOLD LV_STATE_USER_1
#define ETX_STATE_NAME_FONT_SMALL LV_STATE_USER_1

static void publishModelOutputsChanged()
{
  UiEventHub::publish(UiTopic::ModelOutputsChanged);
}

class OutputLineButton : public ListLineButton
{
 public:
  OutputLineButton(Window* parent, uint8_t channel) :
      ListLineButton(parent, channel, LineDependencies::LiveValues)
  {
    setHeight(CH_LINE_H);
    padAll(PAD_ZERO);

    refreshMsg.subscribe(Messaging::REFRESH, [=](uint32_t param) { requestLineUpdate(); });
  }

  void onLineLoaded() override
  {
    curve =
        new StaticIcon(this, CRV_X, CRV_Y, ICON_TEXTLINE_CURVE, COLOR_THEME_SECONDARY1_INDEX);

    new OutputChannelBar(this, rect_t{BAR_X, PAD_MEDIUM, CH_BAR_WIDTH, CH_BAR_HEIGHT},
                               index, false, false);

    refreshCachedText();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

  void describeLine(LineView& view) const override
  {
    view.text(SRC_X, SRC_Y, SRC_W, SRC_H, sourceText.data(),
#if !NARROW_LAYOUT
              sourceSmall ? FONT(XS) : 0, LV_TEXT_ALIGN_LEFT,
              LineView::Color::Default, sourceSmall ? -PAD_THREE : 0);
#else
              sourceSmall ? FONT(XS) : 0);
#endif
    view.text(MIN_X, MIN_Y, MIN_W, EdgeTxStyles::STD_FONT_HEIGHT,
              minText.data(), minBold ? FONT(BOLD) : 0,
              LV_TEXT_ALIGN_RIGHT);
    view.text(MAX_X, MAX_Y, MAX_W, EdgeTxStyles::STD_FONT_HEIGHT,
              maxText.data(), maxBold ? FONT(BOLD) : 0,
              LV_TEXT_ALIGN_RIGHT);
    view.text(OFF_X, OFF_Y, OFF_W, EdgeTxStyles::STD_FONT_HEIGHT,
              offsetText.data(), 0, LV_TEXT_ALIGN_RIGHT);
    view.text(CTR_X, CTR_Y, CTR_W, EdgeTxStyles::STD_FONT_HEIGHT,
              centerText.data(), 0, LV_TEXT_ALIGN_RIGHT);
    if (revertVisible) {
      view.text(REV_X, REV_Y, REV_W, EdgeTxStyles::STD_FONT_HEIGHT,
                LV_SYMBOL_SHUFFLE, 0, LV_TEXT_ALIGN_CENTER);
    }
  }

  void onRefresh() override
  {
    refreshCachedText();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

  static LAYOUT_SIZE_SCALED(CH_LINE_H, 32, 50)
  static LAYOUT_VAL_SCALED(CH_BAR_WIDTH, 100)
  static LAYOUT_VAL_SCALED(CH_BAR_HEIGHT, 16)
  static LAYOUT_VAL_SCALED(BAR_XO, 17)
  static constexpr coord_t BAR_X = LCD_W - CH_BAR_WIDTH - BAR_XO;

  static constexpr coord_t SRC_X = PAD_TINY;
  static constexpr coord_t SRC_Y = 1;
  static LAYOUT_VAL_SCALED(SRC_W, 80)
  static constexpr coord_t SRC_H = CH_LINE_H - PAD_MEDIUM;
  static constexpr coord_t MIN_X = SRC_X + SRC_W + PAD_TINY;
  static LAYOUT_SIZE_SCALED(MIN_Y, 4, 2)
  static LAYOUT_VAL_SCALED(MIN_W, 52)
  static constexpr coord_t MAX_X = MIN_X + MIN_W + PAD_TINY;
  static constexpr coord_t MAX_Y = MIN_Y;
  static LAYOUT_SIZE_SCALED(MAX_W, 52, 60)
  static LAYOUT_SIZE(OFF_X, MAX_X + MAX_W + PAD_TINY, SRC_X + SRC_W + PAD_TINY)
  static LAYOUT_SIZE_SCALED(OFF_Y, 4, 24)
  static LAYOUT_SIZE_SCALED(OFF_W, 44, 52)
  static constexpr coord_t CTR_X = OFF_X + OFF_W + PAD_TINY;
  static constexpr coord_t CTR_Y = OFF_Y;
  static LAYOUT_VAL_SCALED(CTR_W, 60)
  static constexpr coord_t REV_X = CTR_X + CTR_W + PAD_TINY;
  static constexpr coord_t REV_Y = CTR_Y;
  static LAYOUT_VAL_SCALED(REV_W, 16)
  static constexpr coord_t CRV_X = REV_X + REV_W + PAD_TINY;
  static constexpr coord_t CRV_Y = REV_Y + 1;

 protected:
  template <size_t N>
  void setText(std::array<char, N>& dest, const char* text)
  {
    dest[0] = '\0';
    strAppend(dest.data(), text ? text : "", N - 1);
  }

  bool isActive() const override { return false; }

  void onLinePresentationSync(LiveWindow& live) override
  {
    int newValue = getChannelOutput(index);
    if (value != newValue) {
      value = newValue;

      int chanVal = calcRESXto100(getRawChannelOutput(index));
      bool newMinBold = chanVal < -DEADBAND;
      bool newMaxBold = chanVal > DEADBAND;
      if (minBold != newMinBold || maxBold != newMaxBold) {
        minBold = newMinBold;
        maxBold = newMaxBold;
        updateAutomationText();
        lv_obj_invalidate(live.lvobj());
      }
    }
  }

  void refreshCachedText()
  {
    const LimitData* output = limitAddress(index);
    sourceSmall = g_model.limitData[index].name[0] != '\0';
    if (sourceSmall) {
      char chanStr[LEN_CHANNEL_NAME + 16] = {};
      char* s = strAppend(chanStr, getSourceString(MIXSRC_FIRST_CH + index));
      s = strAppend(s, "\n");
      s = strAppend(s, STR_CH);
      strAppendUnsigned(s, index + 1);
      setText(sourceText, chanStr);
    } else {
      setText(sourceText, getSourceString(MIXSRC_FIRST_CH + index));
    }
    setAutomationText(sourceText.data());

    revertVisible = output->revert;

    getValueOrGVarString(minText.data(), minText.size(), output->min, PREC1,
                         nullptr, -LIMITS_MIN_MAX_OFFSET, true);
    getValueOrGVarString(maxText.data(), maxText.size(), output->max, PREC1,
                         nullptr, +LIMITS_MIN_MAX_OFFSET, true);
    getValueOrGVarString(offsetText.data(), offsetText.size(), output->offset,
                         PREC1, nullptr, 0, true);
    snprintf(centerText.data(), centerText.size(), "%d%s",
             PPM_CENTER + output->ppmCenter,
             output->symetrical ? " =" : CHAR_DELTA);

    curveValue = output->curve;
    if (curve) curve->show(curveValue);
    updateAutomationText();
  }

  void updateAutomationText()
  {
#if defined(SIMU)
    char text[224];
    snprintf(text, sizeof(text),
             "%s | min=%s%s | max=%s%s | offset=%s | center=%s | revert=%u | curve=%d",
             sourceText.data(), minText.data(), minBold ? "*" : "",
             maxText.data(), maxBold ? "*" : "", offsetText.data(),
             centerText.data(), unsigned(revertVisible), int(curveValue));
    setAutomationText(text);
#endif
  }

  StaticIcon* curve = nullptr;
  int value = -10000;
  bool minBold = false;
  bool maxBold = false;
  bool sourceSmall = false;
  bool revertVisible = false;
  int8_t curveValue = 0;
  std::array<char, LEN_CHANNEL_NAME + 16> sourceText = {};
  std::array<char, 32> minText = {};
  std::array<char, 32> maxText = {};
  std::array<char, 32> offsetText = {};
  std::array<char, 16> centerText = {};
  Messaging refreshMsg;
};

ModelOutputsPage::ModelOutputsPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

bool ModelOutputsPage::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_OUTPUT_EDIT) return false;

  uint8_t ch = static_cast<uint8_t>(r.params[depth]);
  if (ch >= MAX_OUTPUT_CHANNELS) return false;

  editOutput(ch, nullptr);
  return true;
}

void ModelOutputsPage::build(Window* window)
{
  window->padAll(PAD_ZERO);
  window->padBottom(PAD_LARGE);

  new TextButton(window, {ADD_TRIMS_X, ADD_TRIMS_Y, ADD_TRIMS_W, ADD_TRIMS_H}, STR_ADD_ALL_TRIMS_TO_SUBTRIMS, [=]() {
    new ConfirmDialog(
        STR_TRIMS2OFFSETS, STR_ADD_ALL_TRIMS_TO_SUBTRIMS,
        [=] {
          moveTrimsToOffsets();
          Messaging::send(Messaging::REFRESH);
          publishModelOutputsChanged();
        });
    return 0;
  });

  new StaticText(window, {EXLIM_X, EXLIM_Y, EXLIM_W, EdgeTxStyles::STD_FONT_HEIGHT}, STR_ELIMITS, COLOR_THEME_PRIMARY1_INDEX, RIGHT);
  new ToggleSwitch(window, {EXLIMCB_X, EXLIMCB_Y, EXLIMCB_W, EXLIMCB_H}, GET_SET_DEFAULT(g_model.extendedLimits));

  for (uint8_t ch = 0; ch < MAX_OUTPUT_CHANNELS; ch++) {
    // Channel settings
    auto btn = new OutputLineButton(window, ch);
    btn->setPos(TRIMB_X,
                TRIMB_Y + (ch * (OutputLineButton::CH_LINE_H + PAD_TINY)));
    btn->setWidth(TRIMB_W);

    LimitData* output = limitAddress(ch);
    btn->setPressHandler([=]() -> uint8_t {
      editOutput(ch, btn);
      return 0;
    });
    btn->setLongPressHandler([=]() -> uint8_t {
      Menu* menu = new Menu();
      menu->addLine(STR_EDIT, [=]() { editOutput(ch, btn); });
      menu->addLine(STR_RESET, [=]() {
        output->min = 0;
        output->max = 0;
        output->offset = 0;
        output->ppmCenter = 0;
        output->revert = false;
        output->curve = 0;
        output->symetrical = 0;
        storageDirty(EE_MODEL);
        btn->requestLineUpdate();
        publishModelOutputsChanged();
      });
      menu->addLine(STR_COPY_STICKS_TO_OFS, [=]() {
        copySticksToOffset(ch);
        storageDirty(EE_MODEL);
        btn->requestLineUpdate();
        publishModelOutputsChanged();
      });
      menu->addLine(STR_COPY_TRIMS_TO_OFS, [=]() {
        copyTrimsToOffset(ch);
        storageDirty(EE_MODEL);
        btn->requestLineUpdate();
        publishModelOutputsChanged();
      });
      menu->addLine(STR_COPY_MIN_MAX_TO_OUTPUTS, [=]() {
        copyMinMaxToOutputs(ch);
        storageDirty(EE_MODEL);
        btn->requestLineUpdate();
        publishModelOutputsChanged();
      });
      return 0;
    });
  }
}

void ModelOutputsPage::editOutput(uint8_t channel, OutputLineButton* btn)
{
  (new OutputEditWindow(channel,
      route().appended(RP_OUTPUT_EDIT, static_cast<int16_t>(channel))))
      ->setCloseHandler([=]() {
    btn->requestLineUpdate();
    publishModelOutputsChanged();
  });
}
