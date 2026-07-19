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
#include <memory>

#include "dialog.h"
#include "ds_core.h"
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
    // DESIGN SYSTEM: the ds:: layer owns row height, padding and slot layout.
    // This is a Compact (D1) dense one-line row on the 40 px floor.
    setWidth(LV_PCT(100));
    setHeight(ds::rowHeight(ds::RowSize::Compact));

    refreshMsg.subscribe(Messaging::REFRESH,
                         [=](uint32_t param) { requestLineUpdate(); });
  }

  void onLineLoaded() override
  {
    if (!withLive([&](LiveWindow& live) {
          dsRow = std::make_unique<ds::RowContent>(this, ds::RowSize::Compact);

          // DESIGN SYSTEM: leading slot = at-a-glance reverse indicator.
          // Reserved on every row (not just reversed ones) so the glyph
          // always lands in the SAME column -- a reversed channel among up to
          // 32 catches the eye by appearing in a predictable spot, the same
          // "audit at a glance" reason the USB-joystick inversion marker uses
          // this slot. Hidden/shown per-channel in refreshCachedText().
          Window* slot = dsRow->leadingSlot();
          if (!slot) return false;
          revertIcon = new StaticText(
              slot, rect_t{0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT},
              LV_SYMBOL_SHUFFLE);
          if (!revertIcon || !revertIcon->isAvailable()) return false;

          refreshCachedText();
          lv_obj_invalidate(live.lvobj());
          return true;
        }))
      return;
  }

  void onRefresh() override
  {
    refreshCachedText();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

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

    // Title: channel/source name. getSourceString already yields the custom
    // channel name when one is set, otherwise the default "CHnn".
    setText(sourceText, getSourceString(MIXSRC_FIRST_CH + index));

    revertVisible = output->revert;
    if (revertIcon) revertIcon->show(revertVisible);

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

    // DESIGN SYSTEM row content: leading = reverse indicator (safety-relevant,
    // kept at a glance -- see onLineLoaded); title = channel/source name;
    // trailing = the concise output travel range (min…max). Offset, center,
    // the live channel bar and the curve icon are intentionally relegated to
    // the output editor (OutputEditWindow, opened by editOutput /
    // RP_OUTPUT_EDIT) so more channels fit on this dense list.
    snprintf(trailingText, sizeof(trailingText), "%s  %s", minText.data(),
             maxText.data());
    if (dsRow) {
      dsRow->setTitle(sourceText.data());
      dsRow->setTrailing(trailingText, ds::TextRole::Muted);
    }

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

  int value = -10000;
  bool minBold = false;
  bool maxBold = false;
  bool revertVisible = false;
  int8_t curveValue = 0;
  std::array<char, LEN_CHANNEL_NAME + 16> sourceText = {};
  std::array<char, 32> minText = {};
  std::array<char, 32> maxText = {};
  std::array<char, 32> offsetText = {};
  std::array<char, 16> centerText = {};
  // Sized to the worst case ("<min>  <max>") so the snprintf that fills it can
  // never truncate: both operands plus the two-space separator and the NUL.
  char trailingText[sizeof(minText) + sizeof(maxText) + 3] = {};
  std::unique_ptr<ds::RowContent> dsRow;
  StaticText* revertIcon = nullptr;  // leading-slot reverse (LV_SYMBOL_SHUFFLE) glyph
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
  // DESIGN SYSTEM: the ds::List owns page side-margins and inter-row gaps; the
  // page window is only a flex-column host. The header controls and the
  // channel rows are all children of the (Compact-density) list so spacing
  // stays DS-owned.
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN);
  Window* list = new ds::List(window, ds::Density::Compact);
  if (!list) return;

  // Page-header action: add all trims to subtrims (full-width DS row).
  new TextButton(
      list, rect_t{0, 0, LV_PCT(100), ds::rowHeight(ds::RowSize::OneLine)},
      STR_ADD_ALL_TRIMS_TO_SUBTRIMS, [=]() {
        new ConfirmDialog(STR_TRIMS2OFFSETS, STR_ADD_ALL_TRIMS_TO_SUBTRIMS,
                          [=] {
                            moveTrimsToOffsets();
                            Messaging::send(Messaging::REFRESH);
                            publishModelOutputsChanged();
                          });
        return 0;
      });

  // Page-header setting: extended limits toggle (DS label + control row).
  new ds::FormRow(list, STR_ELIMITS, [&](Window* slot) {
    new ToggleSwitch(slot, rect_t{}, GET_SET_DEFAULT(g_model.extendedLimits));
  });

  for (uint8_t ch = 0; ch < MAX_OUTPUT_CHANNELS; ch++) {
    // Channel row — width/height/position are DS-owned (set in the row ctor
    // and by the ds::List column); no absolute coordinates here.
    auto btn = new OutputLineButton(list, ch);

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
