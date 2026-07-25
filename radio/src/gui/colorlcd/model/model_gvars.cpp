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

#include "model_gvars.h"

#include "choice.h"
#include "dialog.h"
#include "ds_core.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "menu.h"
#include "numberedit.h"
#include "page.h"
#include "route.h"
#include "textedit.h"
#include "toggleswitch.h"
#include "ui_events.h"

#define SET_DIRTY()             \
  do {                          \
    storageDirty(EE_MODEL);     \
    publishModelGVarsChanged(); \
  } while (0)

static void publishModelGVarsChanged()
{
  UiEventHub::publish(UiTopic::ModelGVarsChanged);
}

void getFMExtName(char* dest, int8_t idx)
{
  getFlightModeString(dest, idx);

  FlightModeData* fmData = &g_model.flightModeData[idx - 1];
  int userNameLen = zlen(fmData->name, LEN_FLIGHT_MODE_NAME);

  if (userNameLen > 0) {
    char* s = strAppend(dest + strlen(dest), ":", 1);
    strAppend(s, fmData->name, LEN_FLIGHT_MODE_NAME);
  }
}

// ---------------------------------------------------------------------------
// GVars list — a ds::Grid: a frozen FM0..FM8 header row aligned above every
// GV row's per-flight-mode value columns, with the GV-name column frozen on
// the left. Both the header and the rows realize into the SAME shared column
// template owned by ds::Grid, which is what guarantees the FM header always
// sits directly above its value columns (the class of alignment bug this
// component exists to make structurally impossible). The whole row is the
// touch target: tap = edit (opens GVarEditWindow), long-press = menu.
// ---------------------------------------------------------------------------

static int gvarNumFlightModes()
{
  return modelFMEnabled() ? MAX_FLIGHT_MODES : 1;
}

// Column layout: a frozen GV-name label, then one value column per flight
// mode. Value columns Fill the remaining width equally (never below the touch
// floor), exactly the way the original hand-computed GVAR_VAL_W to fit.
static std::vector<ds::Grid::Column> gvarColumns()
{
  std::vector<ds::Grid::Column> cols;
  cols.push_back(ds::Grid::Column::Fixed(44, ds::CellAlign::Start,
                                         /*frozen=*/true));
  for (int fm = 0; fm < gvarNumFlightModes(); fm++)
    cols.push_back(ds::Grid::Column::Fill(40, ds::CellAlign::Center));
  return cols;
}

static constexpr int GVAR_NAME_COL = 0;
static inline int gvarValueCol(int flightMode) { return flightMode + 1; }

// The sticky FM0..FM8 caption row. Owns only the current-flight-mode
// highlight, which follows getFlightMode() live.
class GVarHeaderRow : public ds::Grid::Row
{
 public:
  explicit GVarHeaderRow(ds::Grid* grid) : ds::Grid::Row(grid, /*header=*/true)
  {
    char label[16];
    for (int fm = 0; fm < MAX_FLIGHT_MODES; fm++) {
      getFlightModeString(label, fm + 1);
      setCell(gvarValueCol(fm), label);
    }
    currentFlightMode = getFlightMode();
    highlightCell(gvarValueCol(currentFlightMode), true);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Button::onLiveCheckEvents(live);
    uint8_t newFM = getFlightMode();
    if (newFM != currentFlightMode) {
      highlightCell(gvarValueCol(currentFlightMode), false);
      highlightCell(gvarValueCol(newFM), true);
      currentFlightMode = newFM;
    }
  }

 protected:
  uint8_t currentFlightMode = 0;
};

// One GV row: the frozen name cell + a value cell per flight mode. Refreshes
// its value cells and the current-flight-mode highlight live (LiveValues).
class GVarRow : public ds::Grid::Row
{
 public:
  GVarRow(ds::Grid* grid, uint8_t gvar, std::function<uint8_t()> onPress,
          std::function<uint8_t()> onLongPress) :
      ds::Grid::Row(grid, /*header=*/false, std::move(onPress),
                    std::move(onLongPress)),
      index(gvar)
  {
    nameText[0] = '\0';
    strAppend(nameText, getGVarString(index), sizeof(nameText) - 1);
    setCell(GVAR_NAME_COL, nameText);
    for (int fm = 0; fm < gvarNumFlightModes(); fm++) updateValueCell(fm);
    currentFlightMode = getFlightMode();
    if (modelFMEnabled()) highlightCell(gvarValueCol(currentFlightMode), true);
    updateAutomationText();
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Button::onLiveCheckEvents(live);
    bool changed = false;
    if (modelFMEnabled()) {
      uint8_t newFM = getFlightMode();
      if (newFM != currentFlightMode) {
        highlightCell(gvarValueCol(currentFlightMode), false);
        highlightCell(gvarValueCol(newFM), true);
        currentFlightMode = newFM;
        changed = true;
      }
    }
    for (int fm = 0; fm < gvarNumFlightModes(); fm++) {
      if (values[fm] != g_model.flightModeData[fm].gvars[index]) {
        updateValueCell(fm);
        changed = true;
      }
    }
    if (changed) updateAutomationText();
  }

 protected:
  uint8_t index;
  uint8_t currentFlightMode = 0;
  gvar_t values[MAX_FLIGHT_MODES] = {};
  char nameText[16] = {};
  char valueTexts[MAX_FLIGHT_MODES][16] = {};

  void updateValueCell(int flightMode)
  {
    gvar_t value = g_model.flightModeData[flightMode].gvars[index];
    values[flightMode] = value;
    char* text = valueTexts[flightMode];
    text[0] = '\0';
    bool small = false;

    if (value > GVAR_MAX) {
      uint8_t fm = value - GVAR_MAX - 1;
      if (fm >= flightMode) fm += 1;
      getFlightModeString(text, fm + 1);
    } else {
      uint8_t unit = g_model.gvars[index].unit;
      const char* suffix = (unit == 1) ? "%" : "";
      uint8_t prec = g_model.gvars[index].prec;
      if (prec)
        snprintf(text, 16, "%d.%01u%s", value / 10,
                 (value < 0) ? (-value) % 10 : value % 10, suffix);
      else
        snprintf(text, 16, "%d%s", value, suffix);
      if (unit)
        small = value <= -1000 || value >= 1000 || (prec && (value <= -100));
    }
    setCellSmall(gvarValueCol(flightMode), text, small);
    if (modelFMEnabled() && flightMode == currentFlightMode)
      highlightCell(gvarValueCol(flightMode), true);
  }

  void updateAutomationText()
  {
#if defined(SIMU)
    char text[192];
    int written = snprintf(text, sizeof(text), "%s | fm=%u", nameText,
                           unsigned(currentFlightMode + 1));
    for (int fm = 0; fm < gvarNumFlightModes() && written >= 0 &&
                     size_t(written) < sizeof(text);
         fm++) {
      written += snprintf(text + written, sizeof(text) - written, " | %u:%s",
                          unsigned(fm + 1), valueTexts[fm]);
    }
    setAutomationText(text);
#endif
  }
};

class GVarEditWindow : public Page
{
 public:
  explicit GVarEditWindow(uint8_t gvarIndex, Route route) :
      Page(ICON_MODEL_GVARS, route), index(gvarIndex)
  {
    buildHeader(header);
    buildBody(body);
  }

 protected:
  uint8_t index;
  gvar_t lastGVar = 0;
  bool refreshTitle = true;
  uint8_t lastFlightMode = 255;  // Force initial setting of header title
  NumberEdit* min = nullptr;
  NumberEdit* max = nullptr;
  NumberEdit* values[MAX_FLIGHT_MODES] = {};
  StaticText* gVarInHeader = nullptr;

  int numFlightModes() { return modelFMEnabled() ? MAX_FLIGHT_MODES : 1; }

  void buildHeader(Window* window)
  {
    header->setTitle(STR_MENU_GLOBAL_VARS);
    gVarInHeader = header->setTitle2("");
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Page::onLiveCheckEvents(live);

    auto curFM = getFlightMode();
    auto fmData = &g_model.flightModeData[curFM];

    if (gVarInHeader && ((lastFlightMode != curFM) ||
                         (lastGVar != fmData->gvars[index]) || refreshTitle)) {
      char label[32];
      refreshTitle = false;
      lastFlightMode = curFM;
      lastGVar = fmData->gvars[index];
      sprintf(label, "%s=", getSourceString(index + MIXSRC_FIRST_GVAR));
      if (lastGVar > GVAR_MAX) {
        uint8_t fm = lastGVar - GVAR_MAX - 1;
        if (fm >= curFM) fm++;
        getFMExtName(label + strlen(label), fm + 1);
      } else {
        strcat(label, getGVarValue(index, lastGVar, 0).c_str());
      }
      gVarInHeader->setText(label);
    }
  }

  void setProperties(int onlyForFlightMode = -1)
  {
    GVarData* gvar = &g_model.gvars[index];
    int32_t minValue = GVAR_MIN + gvar->min;
    int32_t maxValue = GVAR_MAX - gvar->max;
    const char* suffix = gvar->unit ? "%" : "";

    if (min && max) {
      min->setMax(maxValue);
      max->setMin(minValue);

      min->setSuffix(suffix);
      max->setSuffix(suffix);

      if (gvar->prec) {
        min->setTextFlag(PREC1);
        max->setTextFlag(PREC1);
      } else {
        min->clearTextFlag(PREC1);
        max->clearTextFlag(PREC1);
      }

      min->update();
      max->update();
    }
    FlightModeData* fmData;
    for (int fm = 0; fm < numFlightModes(); fm++) {
      if (values[fm] == nullptr)  // KLK: the order of calls has changed and
                                  // this might not be initialized yet.
        continue;

      if (onlyForFlightMode >= 0 && fm != onlyForFlightMode) continue;
      fmData = &g_model.flightModeData[fm];

      // custom value
      if (fmData->gvars[index] <= GVAR_MAX || fm == 0) {
        values[fm]->setMin(GVAR_MIN + gvar->min);
        values[fm]->setMax(GVAR_MAX - gvar->max);
        // Update value if outside min/max range
        values[fm]->setValue(values[fm]->getValue());

        if (gvar->prec)
          values[fm]->setTextFlag(PREC1);
        else
          values[fm]->clearTextFlag(PREC1);

        values[fm]->setDisplayHandler(nullptr);
      } else {
        values[fm]->setMin(GVAR_MAX + 1);
        values[fm]->setMax(GVAR_MAX + MAX_FLIGHT_MODES - 1);
        values[fm]->setDisplayHandler([=](int32_t value) {
          uint8_t targetFlightMode = value - GVAR_MAX - 1;
          if (targetFlightMode >= fm) targetFlightMode++;
          char label[16];
          getFlightModeString(label, targetFlightMode + 1);
          return std::string(label);
        });
      }

      values[fm]->setSuffix(suffix);
    }
  }

  void buildBody(Window* window)
  {
    static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                         LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                         LV_GRID_TEMPLATE_LAST};

    window->setFlexLayout();
    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);  // ds-allow: GVar edit sub-page - per-flight-mode rows pack an enable toggle plus value field (dual control); ds::FormRow's single 40/60 control column can't express it.

    auto line = window->newLine(grid);

    GVarData* gvar = &g_model.gvars[index];

    new StaticText(line, rect_t{}, STR_NAME);
    grid.nextCell();
    new ModelTextEdit(line, rect_t{}, gvar->name, LEN_GVAR_NAME, [=]() { refreshTitle = true; });

    line = window->newLine(grid);

    static const char* const strUnits[] = { "-", "%" };
    new StaticText(line, rect_t{}, STR_UNIT);
    grid.nextCell();
    new Choice(line, rect_t{}, strUnits, 0, 1, GET_DEFAULT(gvar->unit),
               [=](int16_t newValue) {
                 refreshTitle = (gvar->unit != newValue);
                 gvar->unit = newValue;
                 SET_DIRTY();
                 setProperties();
               });

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_PRECISION);
    grid.nextCell();
    new Choice(line, rect_t{}, STR_VPREC, 0, 1, GET_DEFAULT(gvar->prec),
               [=](int16_t newValue) {
                 refreshTitle = (gvar->prec != newValue);
                 gvar->prec = newValue;
                 SET_DIRTY();
                 setProperties();
               });

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_MIN);
    grid.nextCell();
    min = new NumberEdit(
        line, rect_t{}, GVAR_MIN, GVAR_MAX - gvar->max,
        [=] { return gvar->min + GVAR_MIN; },
        [=](int32_t newValue) {
          gvar->min = newValue - GVAR_MIN;
          SET_DIRTY();
          setProperties();
        });
    min->setAccelFactor(16);

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_MAX);
    grid.nextCell();
    max = new NumberEdit(
        line, rect_t{}, GVAR_MIN + gvar->min, GVAR_MAX,
        [=] { return GVAR_MAX - gvar->max; },
        [=](int32_t newValue) {
          gvar->max = GVAR_MAX - newValue;
          SET_DIRTY();
          setProperties();
        });
    max->setAccelFactor(16);

    line = window->newLine(grid);
    new StaticText(line, rect_t{}, STR_POPUP);
    grid.nextCell();
    new ToggleSwitch(line, rect_t{}, GET_SET_DEFAULT(gvar->popup));

    line = window->newLine(grid);
    char flightModeName[16];
    FlightModeData* fmData;

    for (int flightMode = 0; flightMode < numFlightModes(); flightMode++) {
      fmData = &g_model.flightModeData[flightMode];

      if (modelFMEnabled()) {
        getFMExtName(flightModeName, flightMode + 1);
        new StaticText(line, rect_t{}, flightModeName);
      } else {
        new StaticText(line, rect_t{}, STR_VALUE);
      }

      if (flightMode > 0) {
        auto cb = new ToggleSwitch(
            line, rect_t{}, [=] { return fmData->gvars[index] <= GVAR_MAX; },
            [=](uint8_t checked) {
              fmData->gvars[index] = checked ? 0 : GVAR_MAX + 1;
              setProperties(flightMode);
              publishModelGVarsChanged();
            });
        cb->setStyleGridCellXAlign(LV_GRID_ALIGN_END, 0);
        cb->invalidate();
      } else {
        grid.nextCell();
      }

      values[flightMode] = new NumberEdit(
          line, rect_t{}, GVAR_MIN + gvar->min, GVAR_MAX + MAX_FLIGHT_MODES - 1,
          GET_SET_DEFAULT(fmData->gvars[index]));
      values[flightMode]->setAccelFactor(16);
      line = window->newLine(grid);
    }

    setProperties();
    window->setHeight(LCD_H - header->height());
    setHeight(LCD_H);
  }
};

ModelGVarsPage::ModelGVarsPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

bool ModelGVarsPage::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != RP_GVAR_EDIT) return false;

  uint8_t index = static_cast<uint8_t>(r.params[depth]);
  if (index >= MAX_GVARS) return false;

  auto* editWindow = new GVarEditWindow(index, r);
  editWindow->setCloseHandler([=]() { rebuild(pageWindow); });
  return true;
}

void ModelGVarsPage::cleanup()
{
  // The sticky header now lives inside the ds::Grid (cleared with the body).
  hdr = nullptr;
}

void ModelGVarsPage::rebuild(Window* window)
{
  window->clear();
  cleanup();
  build(window);
}

void ModelGVarsPage::build(Window* window)
{
  pageWindow = window;

  // DESIGN SYSTEM: ds::Grid owns the columnar layout — the frozen FM0..FM8
  // header, the frozen GV-name column, per-row value columns, 40 px touch
  // floor and all spacing. The header and every row share one column
  // template, so the header cannot drift out of alignment with the columns.
  auto* grid = new ds::Grid(window, gvarColumns());

  if (modelFMEnabled()) new GVarHeaderRow(grid);

  for (uint8_t index = 0; index < MAX_GVARS; index++) {
    auto onEdit = [=]() -> uint8_t {
      Window* editWindow = new GVarEditWindow(
          index, route().appended(RP_GVAR_EDIT, static_cast<int16_t>(index)));
      editWindow->setCloseHandler([=]() {
        rebuild(window);
        publishModelGVarsChanged();
      });
      return 0;
    };
    auto onMenu = [=]() -> uint8_t {
      Menu* menu = new Menu();
      menu->addLine(STR_EDIT, [=]() { onEdit(); });
      menu->addLine(STR_CLEAR, [=]() {
        // Clear zeroes this GVar in EVERY flight mode at once, not just the
        // one currently shown/selected -- the object string must say so, or
        // a pilot clearing what they think is one flight mode's value loses
        // all MAX_FLIGHT_MODES of them.
        std::string s(getGVarString(index));
        s += " - ";
        s += STR_ALL_FLIGHT_MODES;
        confirmDestructive(STR_CLEAR, s.c_str(), [=]() {
          for (auto& flightMode : g_model.flightModeData) {
            flightMode.gvars[index] = 0;
          }
          storageDirty(EE_MODEL);
          publishModelGVarsChanged();
        });
      });
      return 0;
    };
    new GVarRow(grid, index, onEdit, onMenu);
  }
}
