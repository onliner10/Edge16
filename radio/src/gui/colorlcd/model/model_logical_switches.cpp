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

#include "dialog.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
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

static const lv_coord_t col_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(3),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t col_dsc2[] = {LV_GRID_FR(4), LV_GRID_FR(3),
                                      LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class LogicalSwitchEditPage : public Page
{
 public:
  explicit LogicalSwitchEditPage(uint8_t index, Route route) :
      Page(ICON_MODEL_LOGICAL_SWITCHES, route, PAD_ZERO), index(index)  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.
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
    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.
    FlexGridLayout grid2(col_dsc2, row_dsc, PAD_TINY);  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.

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
            return std::string("---");
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
    window->padLeft(PAD_SMALL);  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.
    window->padRight(PAD_SMALL);  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.

    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);  // ds-allow: LS edit sub-page - V1/V2 rows switch between single- and dual-control layouts (e.g. EDGE's two number fields via a second grid); a single DS FormRow control column can't express it.

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

// ---------------------------------------------------------------------------
// Logical-switch list — a ds::Grid restoring the 7-column tabular scan:
//   name | func | v1 | v2 | AND | duration | delay
// Every row realizes into the same shared column template, so the columns
// stay aligned down the list. The whole row is the touch target: tap = edit,
// long-press = menu. Active operands render bold and the active row is
// checked, both live -- exactly the original per-field feedback, kept because
// the columnar scan of every switch's condition IS the feature here.
// ---------------------------------------------------------------------------

enum {
  LSCOL_NAME = 0,
  LSCOL_FUNC,
  LSCOL_V1,
  LSCOL_V2,
  LSCOL_AND,
  LSCOL_DUR,
  LSCOL_DEL,
  LSCOL_COUNT
};

static std::vector<ds::Grid::Column> logicalSwitchColumns()
{
  return {
      ds::Grid::Column::Fixed(34, ds::CellAlign::Start),   // name  (L01)
      ds::Grid::Column::Fixed(54, ds::CellAlign::Start),   // func  (AND/a<x)
      ds::Grid::Column::Fixed(84, ds::CellAlign::Center),  // v1
      ds::Grid::Column::Fill(56, ds::CellAlign::Center),   // v2 (fills)
      ds::Grid::Column::Fixed(84, ds::CellAlign::Center),  // AND switch
      ds::Grid::Column::Fixed(50, ds::CellAlign::Center),  // duration
      ds::Grid::Column::Fixed(50, ds::CellAlign::Center),  // delay
  };
}

class LogicalSwitchRow : public ds::Grid::Row
{
 public:
  LogicalSwitchRow(ds::Grid* grid, int lsIndex,
                   std::function<uint8_t()> onPress,
                   std::function<uint8_t()> onLongPress) :
      ds::Grid::Row(grid, /*header=*/false, std::move(onPress),
                    std::move(onLongPress)),
      index(lsIndex)
  {
    computeFlags();
    refreshCells();
    check(lastActive);
    updateAutomationText();
  }

  bool isActive() const
  {
    return getSwitch(SWSRC_FIRST_LOGICAL_SWITCH + index);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Button::onLiveCheckEvents(live);
    if (computeFlags()) {
      check(lastActive);
      refreshCells();
      updateAutomationText();
    }
  }

 protected:
  int index;
  bool lastActive = false;
  bool funcBold = false;
  bool v1Bold = false;
  bool v2Bold = false;
  bool andBold = false;
  bool v1Small = false;
  char lsNameText[16] = {};
  char lsFuncText[32] = {};
  char lsV1Text[32] = {};
  char lsV2Text[32] = {};
  char lsAndText[32] = {};
  char lsDurationText[16] = {};
  char lsDelayText[16] = {};

  template <size_t N>
  static void setStr(char (&dest)[N], const char* text)
  {
    dest[0] = '\0';
    strAppend(dest, text ? text : "", N - 1);
  }

  // Recompute the live "bold"/active flags. Returns true if anything changed.
  bool computeFlags()
  {
    LogicalSwitchData* ls = lswAddress(index);
    uint8_t fam = lswFamily(ls->func);
    bool nFuncBold = (fam == LS_FAMILY_STICKY && getLSStickyState(index));
    bool nV1Bold = ((fam == LS_FAMILY_BOOL || fam == LS_FAMILY_EDGE ||
                     fam == LS_FAMILY_STICKY) &&
                    getSwitch(ls->v1));
    bool nV2Bold =
        ((fam == LS_FAMILY_BOOL || fam == LS_FAMILY_STICKY) && getSwitch(ls->v2));
    bool nAndBold = getSwitch(ls->andsw);
    bool active = isActive();
    if (nFuncBold == funcBold && nV1Bold == v1Bold && nV2Bold == v2Bold &&
        nAndBold == andBold && active == lastActive)
      return false;
    funcBold = nFuncBold;
    v1Bold = nV1Bold;
    v2Bold = nV2Bold;
    andBold = nAndBold;
    lastActive = active;
    return true;
  }

  void refreshCells()
  {
    char s[20] = "";
    LogicalSwitchData* ls = lswAddress(index);
    uint8_t fam = lswFamily(ls->func);

    setStr(lsNameText, getSwitchPositionName(SWSRC_FIRST_LOGICAL_SWITCH + index));
    setStr(lsFuncText, STR_VCSWFUNC[ls->func]);

    // V1
    switch (fam) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
      case LS_FAMILY_EDGE:
        setStr(lsV1Text, getSwitchPositionName(ls->v1));
        v1Small = false;
        break;
      case LS_FAMILY_TIMER:
        setStr(lsV1Text, formatNumberAsString(lswTimerValue(ls->v1), PREC1, 0,
                                              nullptr, "s")
                             .c_str());
        v1Small = false;
        break;
      default: {
        char* text = getSourceString(ls->v1);
        v1Small = getTextWidth(text, 0, FONT(STD)) > 88;
        setStr(lsV1Text, text);
      } break;
    }

    // V2
    switch (fam) {
      case LS_FAMILY_BOOL:
      case LS_FAMILY_STICKY:
        setStr(lsV2Text, getSwitchPositionName(ls->v2));
        break;
      case LS_FAMILY_EDGE:
        getsEdgeDelayParam(s, ls);
        setStr(lsV2Text, s);
        break;
      case LS_FAMILY_TIMER:
        setStr(lsV2Text, formatNumberAsString(lswTimerValue(ls->v2), PREC1, 0,
                                              nullptr, "s")
                             .c_str());
        break;
      case LS_FAMILY_COMP:
        setStr(lsV2Text, getSourceString(ls->v2));
        break;
      default:
        setStr(lsV2Text,
               getSourceCustomValueString(
                   ls->v1,
                   (ls->v1 <= MIXSRC_LAST_CH ? calc100toRESX(ls->v2) : ls->v2),
                   0));
        break;
    }

    // AND switch
    setStr(lsAndText, getSwitchPositionName(ls->andsw));

    // Duration
    if (ls->duration > 0)
      setStr(lsDurationText, formatNumberAsString(ls->duration, PREC1, 0,
                                                  nullptr, "s")
                                 .c_str());
    else
      lsDurationText[0] = '\0';

    // Delay
    if (fam != LS_FAMILY_EDGE && ls->delay > 0)
      setStr(lsDelayText,
             formatNumberAsString(ls->delay, PREC1, 0, nullptr, "s").c_str());
    else
      lsDelayText[0] = '\0';

    setCell(LSCOL_NAME, lsNameText);
    setCell(LSCOL_FUNC, lsFuncText, funcBold ? ds::TextRole::Strong
                                             : ds::TextRole::Body);
    setCellSmall(LSCOL_V1, lsV1Text, v1Small,
                 v1Bold ? ds::TextRole::Strong : ds::TextRole::Body);
    setCell(LSCOL_V2, lsV2Text,
            v2Bold ? ds::TextRole::Strong : ds::TextRole::Body);
    setCell(LSCOL_AND, lsAndText,
            andBold ? ds::TextRole::Strong : ds::TextRole::Body);
    setCell(LSCOL_DUR, lsDurationText);
    setCell(LSCOL_DEL, lsDelayText);
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

  bool hasAnySwitch = false;
  for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++) {
    if (lswAddress(i)->func != LS_FUNC_NONE) {
      hasAnySwitch = true;
      break;
    }
  }

  if (!hasAnySwitch) {
    // Nothing configured yet: replace the bare "+" spanning cell with a real
    // empty state (icon + what-this-is + a primary "New" action) so the
    // screen explains itself instead of showing a lone plus. See
    // special_functions.cpp / DESIGN_SYSTEM.md.
    window->setFlexLayout(LV_FLEX_FLOW_COLUMN);
    addButton = nullptr;
    new ds::EmptyState(window, ICON_MODEL_LOGICAL_SWITCHES,
                       STR_MENULOGICALSWITCHES, nullptr, STR_NEW,
                       [=]() -> uint8_t {
                         plusPopup(window);
                         return 0;
                       });
    return;
  }

  // DESIGN SYSTEM: ds::Grid restores the 7-column tabular scan (name / func /
  // v1 / v2 / AND / duration / delay) with all columns aligned down the list,
  // the 40 px touch floor and all spacing. tap = edit, long-press = menu.
  auto* grid = new ds::Grid(window, logicalSwitchColumns());

  bool hasEmptySwitch = false;

  // Reset focusIndex after switching tabs
  if (!isRebuilding) focusIndex = prevFocusIndex;

  for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; i++) {
    LogicalSwitchData* ls = lswAddress(i);

    if (ls->func != LS_FUNC_NONE) {
      auto onEdit = [=]() -> uint8_t {
        Window* lsWindow = new LogicalSwitchEditPage(
            i, route().appended(RP_LOGICAL_SWITCH_EDIT, static_cast<int16_t>(i)));
        lsWindow->setCloseHandler([=]() {
          if (ls->func == LS_FUNC_NONE) rebuild(window);
        });
        return 0;
      };
      auto onMenu = [=]() -> uint8_t {
        Menu* menu = new Menu();
        menu->addLine(STR_EDIT, [=]() { onEdit(); });
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
          // Clearing a logical switch zeroes its whole struct (func ->
          // LS_FUNC_NONE), i.e. it DELETES the switch -- any other logical
          // switch/special function referencing it (by SWSRC index) silently
          // breaks. Name it with its "L05"-style label plus its current
          // function summary so the pilot can see exactly what disappears.
          std::string s(getSwitchPositionName(SWSRC_FIRST_LOGICAL_SWITCH + i));
          s += " - ";
          s += STR_VCSWFUNC[ls->func];
          confirmDestructive(STR_CLEAR, s.c_str(), [=]() {
            memset(ls, 0, sizeof(LogicalSwitchData));
            storageDirty(EE_MODEL);
            rebuild(window);
          });
        });
        return 0;
      };

      auto* row = new LogicalSwitchRow(grid, i, onEdit, onMenu);

      if (focusIndex == i) row->focus();

      row->setFocusHandler([=](bool hasFocus) {
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
    auto* add = grid->addRow(
        [=]() -> uint8_t {
          plusPopup(window);
          return 0;
        },
        [=]() -> uint8_t {
          plusPopup(window);
          return 0;
        });
    if (add) {
      add->setSpanningCell(LV_SYMBOL_PLUS, ds::TextRole::Active);
      add->setFocusHandler([=](bool hasFocus) {
        if (hasFocus && !isRebuilding) prevFocusIndex = focusIndex;
      });
    }
    addButton = add;
  } else {
    addButton = nullptr;
  }
}
