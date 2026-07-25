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

#include "model_usbjoystick.h"

#include "button_matrix.h"
#include "channel_bar.h"
#include "choice.h"
#include "dialog.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "list_line_button.h"
#include "menu.h"
#include "toggleswitch.h"
#include "usb_joystick.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

#if !NARROW_LAYOUT

static const lv_coord_t line_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_TEMPLATE_LAST};

#else

static const lv_coord_t line_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_TEMPLATE_LAST};

#endif

#define USBCH_BTNMX_COL 8

static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class USBChannelEditStatusBar : public Window
{
 public:
  USBChannelEditStatusBar(Window* parent, const rect_t& rect, int8_t channel) :
      Window(parent, rect), _channel(channel)
  {
    channelBar = new ComboChannelBar(
        this,
        {USBCH_EDIT_STATUS_BAR_MARGIN, 0,
         rect.w - (USBCH_EDIT_STATUS_BAR_MARGIN * 2), rect.h},
        channel, true);
  }

  static LAYOUT_SIZE_SCALED(USBCH_EDIT_STATUS_BAR_MARGIN, 3, 0)  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.

 protected:
  ComboChannelBar* channelBar;
  int8_t _channel;
};

class USBChannelButtonSel : public ButtonMatrix
{
 public:
  USBChannelButtonSel(Window* parent, const rect_t& rect, uint8_t channel,
                      std::function<void(int)> _setValue) :
      ButtonMatrix(parent, rect),
      m_channel(channel),
      m_setValue(std::move(_setValue))
  {
    padAll(PAD_OUTLINE);  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.

    bg_color[0] = makeLvColor(COLOR_THEME_PRIMARY2);  // Unused
    fg_color[0] = makeLvColor(COLOR_THEME_SECONDARY1);
    bg_color[1] = makeLvColor(COLOR_THEME_DISABLED);  // Used by other channel_bar
    fg_color[1] = makeLvColor(COLOR_THEME_PRIMARY1);
    bg_color[2] = makeLvColor(COLOR_THEME_ACTIVE);  // Used by this channel
    fg_color[2] = makeLvColor(COLOR_THEME_PRIMARY1);
    bg_color[3] = makeLvColor(COLOR_THEME_WARNING);  // Collision
    fg_color[3] = makeLvColor(COLOR_THEME_PRIMARY2);

    initBtnMap(USBCH_BTNMX_COL, USBJ_BUTTON_SIZE);
    char snum[5];
    for (uint8_t btn = 0; btn < USBJ_BUTTON_SIZE; btn++) {
      snprintf(snum, 5, "%u", btn);
      setText(btn, snum);
    }
    update();

    addStyle(styles->circle, LV_PART_ITEMS);


    memset(m_btns, 0, USBJ_BUTTON_SIZE);
    for (uint8_t ch = 0; ch < USBJ_MAX_JOYSTICK_CHANNELS; ch++) {
      if (ch != m_channel) {
        USBJoystickChData* cch = usbJChAddress(ch);

        if (cch->mode == USBJOYS_CH_BUTTON) {
          uint8_t last = cch->lastBtnNum();
          for (uint8_t b = cch->btn_num; b <= last; b++) {
            m_btns[b] = 1;
          }
        }
      }
    }
    updateState();
  }

  void onPress(uint8_t btn_id) override
  {
    if (m_setValue) {
      m_setValue(btn_id);
    }
    updateState();
  }

  bool isActive(uint8_t btn_id) override { return false; }

  uint8_t getBtnState(uint8_t id)
  {
    if (id < USBJ_BUTTON_SIZE) return m_btns[id];
    return 0;
  }

  void updateState()
  {
    USBJoystickChData* cch = usbJChAddress(m_channel);
    uint8_t last = cch->lastBtnNum();
    uint8_t i;

    for (i = 0; i < USBJ_BUTTON_SIZE; i++) m_btns[i] &= 1;

    for (i = cch->btn_num; i <= last; i++) m_btns[i] |= 2;
  }


 protected:
  uint8_t m_channel = 0;
  std::function<void(int)> m_setValue;
  uint8_t m_btns[USBJ_BUTTON_SIZE];
  lv_color_t bg_color[4];
  lv_color_t fg_color[4];
};



#define USBCH_LINES 3
#define SET_VALUE_WUPDATE(value) \
  [=](int32_t newValue) {        \
    value = newValue;            \
    SET_DIRTY();                 \
    this->update();              \
  }

class USBChannelEditWindow : public Page
{
 public:
  USBChannelEditWindow(uint8_t channel, Route route) : Page(ICON_MODEL_USB, route, PAD_TINY), channel(channel)  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.
  {
    body->padLeft(PAD_MEDIUM);  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.
    body->padRight(PAD_MEDIUM);  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.

    buildHeader(header);
    buildBody(body);
  }

  static LAYOUT_SIZE_SCALED(USBCH_EDIT_STATUS_BAR_WIDTH, 250, 160)  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.
  static LAYOUT_SIZE(USBCH_EDIT_RIGHT_MARGIN, 0, 3)  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.

 protected:
  uint8_t channel;
  USBChannelEditStatusBar* statusBar = nullptr;
  Window* m_btnModeFrame = nullptr;
  Window* m_axisModeLine = nullptr;
  Window* m_simModeLine = nullptr;
  USBChannelButtonSel* _BtnNumSel = nullptr;
  StaticText* collisionText = nullptr;
  Choice* m_btnPosChoice = nullptr;

  void update()
  {
    USBJoystickChData* cch = usbJChAddress(channel);

    m_btnModeFrame->show(cch->mode == USBJOYS_CH_BUTTON);
    m_axisModeLine->show(cch->mode == USBJOYS_CH_AXIS);
    m_simModeLine->show(cch->mode == USBJOYS_CH_SIM);

    if (m_btnPosChoice)
      m_btnPosChoice->enable((cch->param != USBJOYS_BTN_MODE_SW_EMU) &&
                             (cch->param != USBJOYS_BTN_MODE_DELTA));

    if (collisionText) {
      collisionText->setText("");
      collisionText->hide();
      if (cch->mode == USBJOYS_CH_BUTTON) {
        if (isUSBBtnNumCollision(channel)) {
          collisionText->setText(STR_USBJOYSTICK_BTN_COLLISION);
          collisionText->show();
        }
      } else if (cch->mode == USBJOYS_CH_AXIS) {
        if (isUSBAxisCollision(channel)) {
          collisionText->setText(STR_USBJOYSTICK_AXIS_COLLISION);
          collisionText->show();
        }
      } else if (cch->mode == USBJOYS_CH_SIM) {
        if (isUSBSimCollision(channel)) {
          collisionText->setText(STR_USBJOYSTICK_AXIS_COLLISION);
          collisionText->show();
        }
      }
    }

    _BtnNumSel->updateState();
  }

  void buildHeader(Window* window)
  {
    header->setTitle(STR_USBJOYSTICK_LABEL);
    header->setTitle2(getSourceString(MIXSRC_FIRST_CH + channel));

    statusBar = new USBChannelEditStatusBar(
        window,
        {window->getRect().w - USBCH_EDIT_STATUS_BAR_WIDTH -
             USBCH_EDIT_RIGHT_MARGIN,
         0, USBCH_EDIT_STATUS_BAR_WIDTH, EdgeTxStyles::MENU_HEADER_HEIGHT},
        channel);
  }

  void buildBody(Window* form)
  {
    form->setFlexLayout();

    USBJoystickChData* cch = usbJChAddress(channel);

    // DESIGN SYSTEM (see DESIGN_SYSTEM.md): each settings line packs its
    // field(s) side-by-side, so it's a ds::List + ds::Card of ds::FieldRow /
    // ds::FormRow lines. The button/axis/sim sub-panels are mutually
    // exclusive (see update()): the button panel is a whole Card that
    // shows/hides as a unit (its two lines never toggle independently), while
    // the axis/sim lines are each a bare ds::FormRow — itself a Window — so
    // it can show/hide on its own without disturbing its sibling.
    Window* list = new ds::List(form);
    auto* card = new ds::Card(list);

    // Mode | Inversion — both always visible, independent fields.
    new ds::FieldRow(
        card,
        {{STR_USBJOYSTICK_CH_MODE,
          [=](Window* slot) {
            new Choice(slot, rect_t{}, STR_VUSBJOYSTICK_CH_MODE, 0,
                       USBJOYS_CH_LAST, GET_DEFAULT(cch->mode),
                       SET_VALUE_WUPDATE(cch->mode));
          }},
         {STR_USBJOYSTICK_CH_INVERSION, [=](Window* slot) {
            new ToggleSwitch(slot, rect_t{}, GET_SET_DEFAULT(cch->inversion));
          }}});

    // Button-mode panel — visible only while cch->mode == USBJOYS_CH_BUTTON.
    auto* btnModeCard = new ds::Card(list);
    m_btnModeFrame = btnModeCard;

    new ds::FieldRow(
        btnModeCard,
        {{STR_USBJOYSTICK_CH_BTNMODE,
          [=](Window* slot) {
            new Choice(slot, rect_t{}, STR_VUSBJOYSTICK_CH_BTNMODE, 0,
                       USBJOYS_BTN_MODE_LAST, GET_DEFAULT(cch->param),
                       [=](int32_t newValue) {
                         cch->param = newValue;
                         if (cch->param == USBJOYS_BTN_MODE_SW_EMU)
                           m_btnPosChoice->setValue(0);
                         else if (cch->param == USBJOYS_BTN_MODE_DELTA)
                           m_btnPosChoice->setValue(1);
                         SET_DIRTY();
                         this->update();
                       });
          }},
         {STR_USBJOYSTICK_CH_SWPOS, [=](Window* slot) {
            m_btnPosChoice =
                new Choice(slot, rect_t{}, STR_VUSBJOYSTICK_CH_SWPOS, 0, 7,
                          GET_DEFAULT(cch->switch_npos),
                          SET_VALUE_WUPDATE(cch->switch_npos));
          }}});

    new ds::FormRow(btnModeCard, STR_USBJOYSTICK_CH_BTNNUM, [=](Window* slot) {
      _BtnNumSel = new USBChannelButtonSel(slot, rect_t{}, channel,
                                           SET_VALUE_WUPDATE(cch->btn_num));
    });

    // Axis / simulation lines — mutually exclusive with the button panel and
    // each other (exactly one is visible per cch->mode; see update()).
    auto* card2 = new ds::Card(list);

    m_axisModeLine =
        new ds::FormRow(card2, STR_USBJOYSTICK_CH_AXIS, [=](Window* slot) {
          new Choice(slot, rect_t{}, STR_VUSBJOYSTICK_CH_AXIS, 0,
                     USBJOYS_AXIS_LAST, GET_DEFAULT(cch->param),
                     SET_VALUE_WUPDATE(cch->param));
        });

    m_simModeLine =
        new ds::FormRow(card2, STR_USBJOYSTICK_CH_SIM, [=](Window* slot) {
          new Choice(slot, rect_t{}, STR_VUSBJOYSTICK_CH_SIM, 0,
                     USBJOYS_SIM_LAST, GET_DEFAULT(cch->param),
                     SET_VALUE_WUPDATE(cch->param));
        });

    // Collision banner — full-width status text, not a label:control field.
    collisionText = new StaticText(  // ds-allow: USB-joystick channel edit - full-width collision status banner, not a label:control form field.
        card2, {0, 0, LV_PCT(100), EdgeTxStyles::STD_FONT_HEIGHT}, "",
        COLOR_THEME_PRIMARY2_INDEX, FONT(BOLD) | CENTERED);
    collisionText->bgColor(COLOR_THEME_WARNING_INDEX);

    update();
  }
};

class USBChannelLineButton : public ListLineButton
{
 public:
  USBChannelLineButton(Window* parent, uint8_t index) :
      ListLineButton(parent, index, LineDependencies::LiveValues)
  {
    // DESIGN SYSTEM: ds:: owns row height, padding and slot layout.
    setWidth(LV_PCT(100));
    setHeight(ds::rowHeight(ds::RowSize::TwoLine));
  }

  void onLineLoaded() override
  {
    if (!withLive([&](LiveWindow& live) {
          // DESIGN SYSTEM row content: leading slot = inversion marker,
          // title = channel, subtitle = mode (+ switch/range detail in
          // Button mode), trailing = mode-specific setting (Warning role
          // when it collides with another channel's assignment).
          dsRow = std::make_unique<ds::RowContent>(this, ds::RowSize::TwoLine);
          Window* slot = dsRow->leadingSlot();
          if (!slot) return false;

          m_inverse = new StaticIcon(slot, 0, 0, ICON_CHAN_MONITOR_INVERTED,
                                     COLOR_THEME_SECONDARY1_INDEX);
          if (!m_inverse || !m_inverse->isAvailable()) {
            failClosed();
            return false;
          }

          dsRow->setTitle(getSourceString(MIXSRC_FIRST_CH + index));
          return true;
        }))
      return;

    onRefresh();
  }

  void onRefresh() override
  {
    if (!dsRow) return;

    USBJoystickChData* cch = usbJChAddress(index);

    m_inverse->show(cch->inversion);

    const char* mode = STR_VUSBJOYSTICK_CH_MODE[cch->mode];
    const char* param = "";
    bool hasCollision = false;

    if (cch->mode == USBJOYS_CH_BUTTON) {
      param = STR_VUSBJOYSTICK_CH_BTNMODE[cch->param];
      hasCollision = isUSBBtnNumCollision(index);

      uint8_t last = cch->lastBtnNum();
      char range[20];
      if (last > cch->btn_num)
        snprintf(range, sizeof(range), "%u..%u", cch->btn_num, last);
      else
        snprintf(range, sizeof(range), "%u", cch->btn_num);

      snprintf(subtitleText, sizeof(subtitleText), "%s  %s  %s", mode,
               STR_VUSBJOYSTICK_CH_SWPOS[cch->switch_npos], range);
    } else {
      if (cch->mode == USBJOYS_CH_AXIS) {
        param = STR_VUSBJOYSTICK_CH_AXIS[cch->param];
        hasCollision = isUSBAxisCollision(index);
      } else if (cch->mode == USBJOYS_CH_SIM) {
        param = STR_VUSBJOYSTICK_CH_SIM[cch->param];
        hasCollision = isUSBSimCollision(index);
      }
      snprintf(subtitleText, sizeof(subtitleText), "%s", mode);
    }

    dsRow->setSubtitle(subtitleText);
    dsRow->setTrailing(param, hasCollision ? ds::TextRole::Warning
                                           : ds::TextRole::Muted);
  }

  bool isActive() const override { return false; }

 protected:
  StaticIcon* m_inverse = nullptr;
  char subtitleText[48] = {};
  std::unique_ptr<ds::RowContent> dsRow;
};

ModelUSBJoystickPage::ModelUSBJoystickPage(Route route) : Page(ICON_MODEL_USB, route, PAD_BORDER)  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.
{
  header->setTitle(STR_MAIN_MENU_MODEL_SETTINGS);
  header->setTitle2(STR_USBJOYSTICK_LABEL);

  body->setFlexLayout();
  FlexGridLayout grid(line_col_dsc, row_dsc, PAD_TINY);  // ds-allow: USB-joystick - channel table plus a fixed-width status bar; multi-column channel grid, not a plain DS form.

  // Extended mode
  auto line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_USBJOYSTICK_EXTMODE);
  new Choice(line, rect_t{}, STR_VUSBJOYSTICK_EXTMODE, 0, 1,
             GET_DEFAULT(g_model.usbJoystickExtMode),
             SET_VALUE_WUPDATE(g_model.usbJoystickExtMode));

#if NARROW_LAYOUT
  line = body->newLine(grid);
#endif

  _IfModeLabel = new StaticText(line, rect_t{}, STR_USBJOYSTICK_IF_MODE);
  _IfMode = new Choice(line, rect_t{}, STR_VUSBJOYSTICK_IF_MODE, 0,
                       USBJOYS_LAST, GET_DEFAULT(g_model.usbJoystickIfMode),
                       SET_VALUE_WUPDATE(g_model.usbJoystickIfMode));

  line = body->newLine(grid);

  _CircCoutoutLabel = new StaticText(
      line, rect_t{}, STR_USBJOYSTICK_CIRC_COUTOUT);
  _CircCoutout =
      new Choice(line, rect_t{}, STR_VUSBJOYSTICK_CIRC_COUTOUT, 0, USBJOYS_LAST,
                 GET_DEFAULT(g_model.usbJoystickCircularCut),
                 SET_VALUE_WUPDATE(g_model.usbJoystickCircularCut));

#if NARROW_LAYOUT
  line = body->newLine(grid);
#endif

  _ApplyBtn =
      new TextButton(line, rect_t{}, STR_USBJOYSTICK_APPLY_CHANGES, [=]() {
        onUSBJoystickModelChanged();
        this->update();
        return 0;
      });

  // DESIGN SYSTEM: the list container owns margins and inter-row gaps.
  auto btngrp = new ds::List(body);
  _ChannelsGroup = btngrp;
  for (uint8_t ch = 0; ch < USBJ_MAX_JOYSTICK_CHANNELS; ch++) {
    // Channel settings
    auto btn = new USBChannelLineButton(btngrp, ch);

    USBJoystickChData* cch = usbJChAddress(ch);
    btn->setPressHandler([=]() -> uint8_t {
      editChannel(ch, btn);
      return 0;
    });
    btn->setLongPressHandler([=]() -> uint8_t {
      if (cch->mode == USBJOYS_CH_NONE) {
        editChannel(ch, btn);
      } else {
        Menu* menu = new Menu();
        menu->addLine(STR_EDIT, [=]() { editChannel(ch, btn); });
        menu->addLine(STR_CLEAR, [=]() {
          std::string s(getSourceString(MIXSRC_FIRST_CH + ch));
          confirmDestructive(STR_CLEAR, s.c_str(), [=]() {
            memset(cch, 0, sizeof(USBJoystickChData));
            SET_DIRTY();
          });
        });
      }
      return 0;
    });
  }

  update();
}

void ModelUSBJoystickPage::update()
{
  const uint8_t usbj_ctrls = 6;
  Window* ctrls[usbj_ctrls] = {_IfModeLabel, _IfMode,   _CircCoutoutLabel,
                               _CircCoutout, _ApplyBtn, _ChannelsGroup};

  for (uint8_t i = 0; i < usbj_ctrls; i++) {
    ctrls[i]->show(usbJoystickExtMode());
  }

  _ApplyBtn->enable(usbJoystickSettingsChanged());
}

void ModelUSBJoystickPage::editChannel(uint8_t channel,
                                       USBChannelLineButton* btn)
{
  auto chedit = new USBChannelEditWindow(channel, Route{});
  chedit->setCloseHandler([=]() { this->update(); btn->requestLineUpdate(); });
}
