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

#include "model_logical_switches.h"

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "list_line_button.h"
#include "menu.h"
#include "numberedit.h"
#include "page.h"
#include "route.h"
#include "sourcechoice.h"
#include "switchchoice.h"
#include "switches.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

#define ETX_STATE_LS_ACTIVE LV_STATE_USER_1
#define ETX_STATE_V1_SMALL_FONT LV_STATE_USER_2

static const lv_coord_t col_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(3),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t col_dsc2[] = {LV_GRID_FR(4), LV_GRID_FR(3),
                                      LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class LogicalSwitchEditPage : public Page
{
 public:
  explicit LogicalSwitchEditPage(uint8_t index, Route route) :
      Page(ICON_MODEL_LOGICAL_SWITCHES, route, PAD_ZERO), index(index)
  {
    buildHeader(header);
    buildBody(body);
  }

 protected:
  uint8_t index;
  bool active = false;
  Window* logicalSwitchOneWindow = nullptr;
  StaticText* headerSwitchName = nullptr;
  NumberEdit* v2Edit = nullptr;

  bool isActive() const
  {
    return getSwitch(SWSRC_FIRST_LOGICAL_SWITCH + index);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Page::onLiveCheckEvents(live);
    if (active != isActive()) {
      if (isActive()) {
        headerSwitchName->addState(ETX_STATE_LS_ACTIVE);
      } else {
        headerSwitchName->clearState(ETX_STATE_LS_ACTIVE);
      }
      active = isActive();
    }
  }

  void buildHeader(Window* window)
  {
    header->setTitle(STR_MENULOGICALSWITCHES);
    headerSwitchName = header->setTitle2(
        getSwitchPositionName(SWSRC_FIRST_LOGICAL_SWITCH + index));

    headerSwitchName->textColor(COLOR_THEME_ACTIVE_INDEX, ETX_STATE_LS_ACTIVE);
    headerSwitchName->font(FONT_BOLD_INDEX, ETX_STATE_LS_ACTIVE);
  }

  void updateLogicalSwitchOneWindow()
  {
    SwitchChoice* choice;
    NumberEdit* timer;

    logicalSwitchOneWindow->clear();
    logicalSwitchOneWindow->setFlexLayout();
    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
    FlexGridLayout grid2(col_dsc2, row_dsc, PAD_TINY);

    LogicalSwitchData* cs = lswAddress(index);
    uint8_t cstate = lswFamily(cs->func);

    // V1
    auto line = logicalSwitchOneWindow->newLine(grid);
    new StaticText(line, rect_t{}, STR_V1);
    switch (cstate) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
      case LS_FAMILY_EDGE:
        choice = new SwitchChoice(
            line, rect_t{}, SWSRC_FIRST_IN_LOGICAL_SWITCHES,
            SWSRC_LAST_IN_LOGICAL_SWITCHES, GET_SET_DEFAULT(cs->v1));
        choice->setAvailableHandler(isSwitchAvailableInLogicalSwitches);
        break;
      case LS_FAMILY_COMP:
        new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_TELEM,
                         GET_SET_DEFAULT(cs->v1), true);
        break;
      case LS_FAMILY_TIMER:
        timer =
            new NumberEdit(line, rect_t{}, -128, 122, GET_SET_DEFAULT(cs->v1));
        timer->setDisplayHandler([](int32_t value) {
          return formatNumberAsString(lswTimerValue(value), PREC1, 0, nullptr,
                                      "s");
        });
        break;
      default:
        new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_TELEM,
                         GET_DEFAULT(cs->v1), [=](int32_t newValue) {
                           cs->v1 = newValue;
                           if (v2Edit != nullptr) {
                             int16_t v2_min = 0, v2_max = 0;
                             validateLSV2Range(cs, v2_min, v2_max, nullptr);
                             v2Edit->setMin(v2_min);
                             v2Edit->setMax(v2_max);
                             v2Edit->setValue(cs->v2);
                           }
                           SET_DIRTY();
                         }, true);
        break;
    }

    // V2
    if (cstate == LS_FAMILY_EDGE) {
      line = logicalSwitchOneWindow->newLine(grid2);
    } else {
      line = logicalSwitchOneWindow->newLine(grid);
    }
    new StaticText(line, rect_t{}, STR_V2);
    switch (cstate) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
        choice = new SwitchChoice(
            line, rect_t{}, SWSRC_FIRST_IN_LOGICAL_SWITCHES,
            SWSRC_LAST_IN_LOGICAL_SWITCHES, GET_SET_DEFAULT(cs->v2));
        choice->setAvailableHandler(isSwitchAvailableInLogicalSwitches);
        break;
      case LS_FAMILY_EDGE: {
        auto edit1 =
            new NumberEdit(line, rect_t{}, -129, 122, GET_DEFAULT(cs->v2));
        auto edit2 = new NumberEdit(line, rect_t{}, -1, 222 - cs->v2,
                                    GET_SET_DEFAULT(cs->v3));
        edit1->setSetValueHandler([=](int32_t newValue) {
          cs->v2 = newValue;
          SET_DIRTY();
          edit2->setMax(222 - cs->v2);
          edit2->setValue(cs->v3);
        });
        edit1->setDisplayHandler([](int32_t value) {
          return formatNumberAsString(lswTimerValue(value), PREC1, 0, nullptr,
                                      "s");
        });
        edit2->setDisplayHandler([cs](int32_t value) {
          if (value < 0)
            return std::string("<<");
          else if (value == 0)
            return std::string("--");
          else {
            return formatNumberAsString(lswTimerValue(cs->v2 + value), PREC1, 0,
                                        nullptr, "s");
          }
        });
      } break;
      case LS_FAMILY_COMP:
        new SourceChoice(line, rect_t{}, 0, MIXSRC_LAST_TELEM,
                         GET_SET_DEFAULT(cs->v2), true);
        break;
      case LS_FAMILY_TIMER:
        timer =
            new NumberEdit(line, rect_t{}, -128, 122, GET_SET_DEFAULT(cs->v2));
        timer->setDisplayHandler([](int32_t value) {
          return formatNumberAsString(lswTimerValue(value), PREC1, 0, nullptr,
                                      "s");
        });
        break;
      default:
        int16_t v2_min = 0, v2_max = 0;
        if (validateLSV2Range(cs, v2_min, v2_max, nullptr)) SET_DIRTY();
        v2Edit = new NumberEdit(line, rect_t{}, v2_min, v2_max,
                                GET_SET_DEFAULT(cs->v2));

        v2Edit->setDisplayHandler([=](int value) -> std::string {
          if (abs(cs->v1) <= MIXSRC_LAST_CH) value = calc100toRESX(value);
          std::string txt = getSourceCustomValueString(cs->v1, value, 0);
          return txt;
        });
        break;
    }

    // AND switch
    line = logicalSwitchOneWindow->newLine(grid);
    new StaticText(line, rect_t{}, STR_AND_SWITCH);
    choice = new SwitchChoice(line, rect_t{}, -MAX_LS_ANDSW, MAX_LS_ANDSW,
                              GET_SET_DEFAULT(cs->andsw));
    choice->setAvailableHandler(isSwitchAvailableInLogicalSwitches);

    // Duration
    line = logicalSwitchOneWindow->newLine(grid);
    new StaticText(line, rect_t{}, STR_DURATION);
    auto edit = new NumberEdit(line, rect_t{}, 0, MAX_LS_DURATION,
                               GET_SET_DEFAULT(cs->duration), PREC1);
    edit->setZeroText("---");
    edit->setDisplayHandler([](int32_t value) {
      return formatNumberAsString(value, PREC1, 0, nullptr, "s");
    });

    // Delay
    line = logicalSwitchOneWindow->newLine(grid);
    new StaticText(line, rect_t{}, STR_DELAY);
    if (cstate == LS_FAMILY_EDGE) {
      new StaticText(line, rect_t{}, STR_NA);
    } else {
      auto edit = new NumberEdit(line, rect_t{}, 0, MAX_LS_DELAY,
                                 GET_SET_DEFAULT(cs->delay), PREC1);
      edit->setDisplayHandler([](int32_t value) {
        if (value == 0) return std::string("---");
        return formatNumberAsString(value, PREC1, 0, nullptr, "s");
      });
    }

    // Sticky persist
    if (cstate == LS_FAMILY_STICKY) {
      line = logicalSwitchOneWindow->newLine(grid);
      new StaticText(line, rect_t{}, STR_PERSISTENT);
      new ToggleSwitch(line, rect_t{}, GET_SET_DEFAULT(cs->lsPersist));
    }
  }

  void buildBody(Window* window)
  {
    window->setFlexLayout();
    window->padLeft(PAD_SMALL);
    window->padRight(PAD_SMALL);

    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);

    LogicalSwitchData* cs = lswAddress(index);

    // LS Func
    auto line = window->newLine(grid);
    new StaticText(line, rect_t{}, STR_FUNC);
    auto functionChoice = new Choice(line, rect_t{}, STR_VCSWFUNC, 0,
                                     LS_FUNC_MAX, GET_DEFAULT(cs->func));
    functionChoice->setSetValueHandler([=](int32_t newValue) {
      cs->func = newValue;
      if (lswFamily(cs->func) == LS_FAMILY_TIMER) {
        cs->v1 = cs->v2 = 0;
      } else if (lswFamily(cs->func) == LS_FAMILY_EDGE) {
        cs->v1 = 0;
        cs->v2 = -129;
        cs->v3 = 0;
      } else {
        cs->v1 = cs->v2 = 0;
      }
      SET_DIRTY();
      updateLogicalSwitchOneWindow();
    });

    logicalSwitchOneWindow = new Window(window, rect_t{});
    updateLogicalSwitchOneWindow();
  }
};

void getsEdgeDelayParam(char* s, LogicalSwitchData* ls)
{
  sprintf(s, "[%s:%s]",
          formatNumberAsString(lswTimerValue(ls->v2), PREC1, 0, nullptr, "s")
              .c_str(),
          (ls->v3 < 0)    ? "<<"
          : (ls->v3 == 0) ? "--"
                          : formatNumberAsString(lswTimerValue(ls->v2 + ls->v3),
                                                 PREC1, 0, nullptr, "s")
                                .c_str());
}

class LogicalSwitchButton : public ListLineButton
{
 public:
  LogicalSwitchButton(Window* parent, int lsIndex) :
      ListLineButton(parent, lsIndex, LineDependencies::LiveValues)
  {
    setHeight(LS_BUTTON_H);
    padAll(PAD_ZERO);

    check(isActive());
  }

  void onLineLoaded() override
  {
    onRefresh();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

  void describeLine(LineView& view) const override
  {
    view.text(NM_X, NM_Y, NM_W, EdgeTxStyles::STD_FONT_HEIGHT, lsNameText);
    view.text(FN_X, FN_Y, FN_W, EdgeTxStyles::STD_FONT_HEIGHT, lsFuncText,
              funcBold ? FONT(BOLD) : 0);
    view.text(V1_X, V1_Y, V1_W, EdgeTxStyles::STD_FONT_HEIGHT, lsV1Text,
              v1Bold ? FONT(BOLD) : (v1Small ? FONT(XS) : 0),
              LV_TEXT_ALIGN_CENTER);
    view.text(V2_X, V2_Y, V2_W, EdgeTxStyles::STD_FONT_HEIGHT, lsV2Text,
              v2Bold ? FONT(BOLD) : 0, LV_TEXT_ALIGN_CENTER);
    view.text(AND_X, AND_Y, AND_W, EdgeTxStyles::STD_FONT_HEIGHT, lsAndText,
              andBold ? FONT(BOLD) : 0, LV_TEXT_ALIGN_CENTER);
    view.text(DUR_X, DUR_Y, DUR_W, EdgeTxStyles::STD_FONT_HEIGHT,
              lsDurationText, 0, LV_TEXT_ALIGN_CENTER);
    view.text(DEL_X, DEL_Y, DEL_W, EdgeTxStyles::STD_FONT_HEIGHT,
              lsDelayText, 0, LV_TEXT_ALIGN_CENTER);
  }

  bool isActive() const override
  {
    return getSwitch(SWSRC_FIRST_LOGICAL_SWITCH + index);
  }

  void onLinePresentationSync(LiveWindow& live) override
  {
    LogicalSwitchData* ls = lswAddress(index);
    uint8_t lsFamily = lswFamily(ls->func);

    bool newFuncBold = (lsFamily == LS_FAMILY_STICKY && getLSStickyState(index));
    bool newV1Bold = ((lsFamily == LS_FAMILY_BOOL || lsFamily == LS_FAMILY_EDGE || lsFamily == LS_FAMILY_STICKY) && getSwitch(ls->v1));
    bool newV2Bold = ((lsFamily == LS_FAMILY_BOOL || lsFamily == LS_FAMILY_STICKY) && getSwitch(ls->v2));
    bool newAndBold = getSwitch(ls->andsw);
    bool active = isActive();

    if (newFuncBold != funcBold || newV1Bold != v1Bold ||
        newV2Bold != v2Bold || newAndBold != andBold || active != lastActive) {
      funcBold = newFuncBold;
      v1Bold = newV1Bold;
      v2Bold = newV2Bold;
      andBold = newAndBold;
      lastActive = active;
      check(active);
      updateAutomationText();
      lv_obj_invalidate(live.lvobj());
    }
  }

  void onRefresh() override
  {
    char s[20] = "";

    LogicalSwitchData* ls = lswAddress(index);
    uint8_t lsFamily = lswFamily(ls->func);

    setText(lsNameText, getSwitchPositionName(SWSRC_FIRST_LOGICAL_SWITCH + index));
    setText(lsFuncText, STR_VCSWFUNC[ls->func]);

    // CSW params - V1
    switch (lsFamily) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
      case LS_FAMILY_EDGE:
        setText(lsV1Text, getSwitchPositionName(ls->v1));
        v1Small = false;
        break;
      case LS_FAMILY_TIMER:
        setText(lsV1Text, formatNumberAsString(lswTimerValue(ls->v1), PREC1,
                                               0, nullptr, "s").c_str());
        v1Small = false;
        break;
      default: {
        char* text = getSourceString(ls->v1);
        v1Small = getTextWidth(text, 0, FONT(STD)) > 88;
        setText(lsV1Text, text);
      } break;
    }

    // CSW params - V2
    switch (lsFamily) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
        setText(lsV2Text, getSwitchPositionName(ls->v2));
        break;
      case LS_FAMILY_EDGE:
        getsEdgeDelayParam(s, ls);
        setText(lsV2Text, s);
        break;
      case LS_FAMILY_TIMER:
        setText(lsV2Text, formatNumberAsString(lswTimerValue(ls->v2), PREC1,
                                               0, nullptr, "s").c_str());
        break;
      case LS_FAMILY_COMP:
        setText(lsV2Text, getSourceString(ls->v2));
        break;
      default:
        setText(lsV2Text,
                getSourceCustomValueString(
                    ls->v1,
                    (ls->v1 <= MIXSRC_LAST_CH ? calc100toRESX(ls->v2) : ls->v2),
                    0));
        break;
    }

    // AND switch
    setText(lsAndText, getSwitchPositionName(ls->andsw));

    // CSW duration
    if (ls->duration > 0) {
      setText(lsDurationText,
              formatNumberAsString(ls->duration, PREC1, 0, nullptr, "s").c_str());
    } else {
      lsDurationText[0] = '\0';
    }

    // CSW delay
    if (lsFamily != LS_FAMILY_EDGE && ls->delay > 0) {
      setText(lsDelayText,
              formatNumberAsString(ls->delay, PREC1, 0, nullptr, "s").c_str());
    } else {
      lsDelayText[0] = '\0';
    }
    updateAutomationText();
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

  static LAYOUT_SIZE_SCALED(LS_BUTTON_H, 32, 44)

  static constexpr coord_t NM_X = PAD_TINY;
  static LAYOUT_SIZE_SCALED(NM_Y, 4, 10)
  static LAYOUT_SIZE_SCALED(NM_W, 30, 36)
  static constexpr coord_t FN_X = NM_X + NM_W + PAD_TINY;
  static constexpr coord_t FN_Y = NM_Y;
  static LAYOUT_SIZE_SCALED(FN_W, 50, 58)
  static constexpr coord_t V1_X = FN_X + FN_W + PAD_TINY;
  static LAYOUT_SIZE(V1_Y, NM_Y, 0)
  static LAYOUT_VAL_SCALED(V1_W, 88)
  static constexpr coord_t V2_X = V1_X + V1_W + PAD_TINY;
  static constexpr coord_t V2_Y = V1_Y;
  static constexpr coord_t AND_W = V1_W;
  static LAYOUT_SIZE_SCALED(DUR_W, 40, 54)
  static constexpr coord_t DEL_W = DUR_W;
  static constexpr coord_t AND_X = ListLineButton::GRP_W - PAD_BORDER * 2 - AND_W - DUR_W - DEL_W - PAD_TINY * 3;
  static LAYOUT_SIZE_SCALED(AND_Y, 4, 20)
  static constexpr coord_t V2_W = AND_X - V2_X - PAD_TINY;
  static constexpr coord_t DUR_X = ListLineButton::GRP_W - PAD_BORDER * 2 - DUR_W - DEL_W - PAD_TINY * 2;
  static constexpr coord_t DUR_Y = AND_Y;
  static constexpr coord_t DEL_X = ListLineButton::GRP_W - PAD_BORDER * 2 - DEL_W - PAD_TINY;
  static constexpr coord_t DEL_Y = AND_Y;

 protected:
  template <size_t N>
  void setText(char (&dest)[N], const char* text)
  {
    dest[0] = '\0';
    strAppend(dest, text ? text : "", N - 1);
  }

  void updateAutomationText()
  {
#if defined(SIMU)
    char text[224];
    snprintf(text, sizeof(text),
             "%s | %s | v1=%s | v2=%s | and=%s | dur=%s | delay=%s | active=%u",
             lsNameText, lsFuncText, lsV1Text, lsV2Text, lsAndText,
             lsDurationText, lsDelayText, unsigned(lastActive));
    setAutomationText(text);
#endif
  }

  char lsNameText[16] = {};
  char lsFuncText[32] = {};
  char lsV1Text[32] = {};
  char lsV2Text[32] = {};
  char lsAndText[32] = {};
  char lsDurationText[16] = {};
  char lsDelayText[16] = {};
  bool lastActive = false;
  bool funcBold = false;
  bool v1Bold = false;
  bool v2Bold = false;
  bool andBold = false;
  bool v1Small = false;
};

ModelLogicalSwitchesPage::ModelLogicalSwitchesPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

bool ModelLogicalSwitchesPage::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_LOGICAL_SWITCH_EDIT) return false;

  uint8_t index = static_cast<uint8_t>(r.params[depth]);
  if (index >= MAX_LOGICAL_SWITCHES) return false;

  LogicalSwitchData* ls = lswAddress(index);
  if (ls->func == LS_FUNC_NONE) return false;

  auto* lsWindow = new LogicalSwitchEditPage(index, r);
  lsWindow->setCloseHandler([=]() { rebuild(pageWindow); });
  return true;
}

void ModelLogicalSwitchesPage::rebuild(Window* window)
{
  // When window.clear() is called the last button on screen is given focus
  // (???) This causes the page to jump to the end when rebuilt. Set flag to
  // bypass the button focus handler and reset focusIndex when rebuilding
  isRebuilding = true;
  window->clear();
  build(window);
  isRebuilding = false;
}

void ModelLogicalSwitchesPage::newLS(Window* window, bool pasteLS)
{
  Menu* menu = new Menu();
  menu->setTitle(STR_MENU_LOGICAL_SWITCHES);

  // search for unused switches
  for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++) {
    LogicalSwitchData* ls = lswAddress(i);
    if (ls->func == LS_FUNC_NONE) {
      std::string ch_name(
          getSwitchPositionName(SWSRC_FIRST_LOGICAL_SWITCH + i));
      menu->addLineBuffered(ch_name.c_str(), [=]() {
        if (pasteLS) {
          *ls = clipboard.data.csw;
          storageDirty(EE_MODEL);
          focusIndex = i;
          rebuild(window);
        } else {
          Window* lsWindow = new LogicalSwitchEditPage(i,
              route().appended(RP_LOGICAL_SWITCH_EDIT, static_cast<int16_t>(i)));
          lsWindow->setCloseHandler([=]() {
            if (ls->func != LS_FUNC_NONE) {
              focusIndex = i;
              rebuild(window);
            }
          });
        }
      });
    }
  }
  menu->updateLines();
}

void ModelLogicalSwitchesPage::plusPopup(Window* window)
{
  if (clipboard.type == CLIPBOARD_TYPE_CUSTOM_SWITCH) {
    Menu* menu = new Menu();
    menu->addLine(STR_NEW, [=]() { newLS(window, false); });
    menu->addLine(STR_PASTE, [=]() { newLS(window, true); });
  } else {
    newLS(window, false);
  }
}

void ModelLogicalSwitchesPage::build(Window* window)
{
  pageWindow = window;
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_OUTLINE);

  bool hasEmptySwitch = false;

  // Reset focusIndex after switching tabs
  if (!isRebuilding) focusIndex = prevFocusIndex;

  for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++) {
    LogicalSwitchData* ls = lswAddress(i);

    bool isActive = (ls->func != LS_FUNC_NONE);

    if (isActive) {
      auto button = new LogicalSwitchButton(window, i);

      button->setPressHandler([=]() {
        Window* lsWindow = new LogicalSwitchEditPage(i,
            route().appended(RP_LOGICAL_SWITCH_EDIT, static_cast<int16_t>(i)));
        lsWindow->setCloseHandler([=]() {
          if (ls->func == LS_FUNC_NONE)
            rebuild(window);
          else
            button->requestLineUpdate();
        });
        return 0;
      });

      button->setLongPressHandler([=]() -> uint8_t {
        Menu* menu = new Menu();
        menu->addLine(STR_EDIT, [=]() {
          Window* lsWindow = new LogicalSwitchEditPage(i,
              route().appended(RP_LOGICAL_SWITCH_EDIT, static_cast<int16_t>(i)));
          lsWindow->setCloseHandler([=]() {
            if (ls->func == LS_FUNC_NONE)
              rebuild(window);
            else
              button->requestLineUpdate();
          });
        });
        menu->addLine(STR_COPY, [=]() {
          clipboard.type = CLIPBOARD_TYPE_CUSTOM_SWITCH;
          clipboard.data.csw = *ls;
        });
        if (clipboard.type == CLIPBOARD_TYPE_CUSTOM_SWITCH)
          menu->addLine(STR_PASTE, [=]() {
            *ls = clipboard.data.csw;
            storageDirty(EE_MODEL);
            rebuild(window);
          });
        menu->addLine(STR_CLEAR, [=]() {
          memset(ls, 0, sizeof(LogicalSwitchData));
          storageDirty(EE_MODEL);
          rebuild(window);
        });
        return 0;
      });

      if (focusIndex == i) {
        button->focus();
      }

      button->setFocusHandler([=](bool hasFocus) {
        if (hasFocus && !isRebuilding) {
          prevFocusIndex = focusIndex;
          focusIndex = i;
        }
      });
    } else {
      hasEmptySwitch = true;
    }
  }

  if (hasEmptySwitch) {
    addButton =
        new TextButton(window, rect_t{0, 0, window->width() - PAD_SMALL * 2, LogicalSwitchButton::LS_BUTTON_H},
                       LV_SYMBOL_PLUS, [=]() {
                         plusPopup(window);
                         return 0;
                       });

    addButton->setLongPressHandler([=]() -> uint8_t {
      plusPopup(window);
      return 0;
    });

    addButton->setFocusHandler([=](bool hasFocus) {
      if (hasFocus && !isRebuilding) {
        prevFocusIndex = focusIndex;
      }
    });
  } else {
    addButton = nullptr;
  }
}
