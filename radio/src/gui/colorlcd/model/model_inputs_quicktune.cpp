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

#include "model_inputs.h"

#include <algorithm>
#include <vector>

#include "button.h"
#include "choice.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "hal/rotary_encoder.h"
#include "menu.h"
#include "output_edit.h"
#include "slider.h"
#include "static.h"
#include "switchchoice.h"
#include "tasks/mixer_task.h"
#include "ui_events.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

static int inputLineNumber(uint8_t index)
{
  ExpoData* expo = expoAddress(index);
  if (!EXPO_VALID(expo)) return -1;

  int number = 0;
  for (uint8_t i = 0; i <= index && i < MAX_EXPOS; i += 1) {
    ExpoData* current = expoAddress(i);
    if (!EXPO_VALID(current)) break;
    if (current->chn == expo->chn) number += 1;
  }
  return number;
}

static bool sourceNumIsSource(uint16_t rawValue)
{
  SourceNumVal v;
  v.rawValue = rawValue;
  return v.isSource;
}

static int16_t sourceNumValue(uint16_t rawValue)
{
  SourceNumVal v;
  v.rawValue = rawValue;
  return v.value;
}

static std::string sourceNumString(uint16_t rawValue, const char* suffix)
{
  char text[32] = {};
  getValueOrSrcVarString(text, sizeof(text), rawValue, 0, suffix);
  return text;
}

static bool isQuickExpoEditable(const ExpoData* input)
{
  return input->curve.type == CURVE_REF_EXPO &&
         !sourceNumIsSource(input->curve.value);
}

static std::string quickCurveString(const ExpoData* input)
{
  char text[32] = {};
  getCurveRefString(text, sizeof(text), input->curve);
  return text[0] ? std::string(text) : std::string("0%");
}

static bool outputChannelHasMix(uint8_t channel)
{
  for (uint8_t i = 0; i < MAX_MIXERS; i++) {
    MixData* mix = &g_model.mixData[i];
    if (is_memclear(mix, sizeof(MixData))) break;
    if (mix->destCh == channel) return true;
  }
  return false;
}

static bool outputChannelHasCustomLimits(uint8_t channel)
{
  const LimitData* output = limitAddress(channel);
  return output->name[0] != '\0' || output->min != 0 || output->max != 0 ||
         output->offset != 0 || output->ppmCenter != 0 || output->revert ||
         output->curve != 0 || output->symetrical != 0;
}

static bool outputChannelShouldShow(uint8_t channel)
{
  return outputChannelHasMix(channel) || outputChannelHasCustomLimits(channel);
}

static std::string outputLimitString(uint8_t channel, bool minimum)
{
  const LimitData* output = limitAddress(channel);
  char text[32] = {};
  getValueOrGVarString(text, sizeof(text), minimum ? output->min : output->max,
                       PREC1, nullptr,
                       minimum ? -LIMITS_MIN_MAX_OFFSET
                               : +LIMITS_MIN_MAX_OFFSET,
                       true);
  return text;
}

static bool outputLimitIsQuickEditable(uint8_t channel, bool minimum)
{
  const LimitData* output = limitAddress(channel);
  return !GV_IS_GV_VALUE(minimum ? output->min : output->max);
}

static void setOutputDirty()
{
  storageDirty(EE_MODEL);
  UiEventHub::publish(UiTopic::ModelOutputsChanged);
}

class QuickAdjustCell : public TextButton
{
 public:
  QuickAdjustCell(Window* parent, int vmin, int vmax,
                  std::function<int()> getValue,
                  std::function<void(int)> setValue,
                  std::string suffix = {},
                  std::function<std::string(int)> displayValue = nullptr) :
      TextButton(parent, rect_t{}, ""),
      vmin(vmin),
      vmax(vmax),
      getValue(std::move(getValue)),
      setValue(std::move(setValue)),
      suffix(std::move(suffix)),
      displayValue(std::move(displayValue))
  {
    withLive([&](LiveWindow& live) {
      lv_obj_add_event_cb(live.lvobj(), QuickAdjustCell::quickAdjustEventCb,
                          LV_EVENT_KEY, this);
    });
    update();
  }

  void setFastStep(int value) { fastStep = value; }
  void setAccelFactor(int value) { accelFactor = value; }

  void update()
  {
    int value = currentValue();
    std::string text = display(value);
    setText(isEditMode() ? ("‹" + text + "›") : text);
#if defined(SIMU)
    setAutomationText(text + (isEditMode() ? " adjusting" : ""));
#endif
  }

 protected:
  int vmin;
  int vmax;
  int fastStep = 5;
  int accelFactor = 8;
  std::function<int()> getValue;
  std::function<void(int)> setValue;
  std::string suffix;
  std::function<std::string(int)> displayValue;
  rotenc_t lastRotaryValue = 0;

  int currentValue() const { return getValue ? getValue() : 0; }

  std::string display(int value) const
  {
    if (displayValue) return displayValue(value);
    return std::to_string(value) + suffix;
  }

  void setAdjustedValue(int value)
  {
    value = limit(value, vmin, vmax);
    if (setValue) setValue(value);
    update();
  }

  void adjust(int delta) { setAdjustedValue(currentValue() + delta); }

  void setAdjusting(bool adjusting)
  {
    if (adjusting) lastRotaryValue = rotaryEncoderGetValue();
    setEditMode(adjusting);
    check(adjusting);
    update();
  }

  void onLiveClicked(LiveWindow&) override { setAdjusting(!isEditMode()); }

  void onCancel() override
  {
    if (isEditMode()) {
      setAdjusting(false);
    } else {
      TextButton::onCancel();
    }
  }

  void onLiveEvent(LiveWindow& live, event_t event) override
  {
    if (isEditMode()) {
      switch (event) {
#if defined(HARDWARE_KEYS)
        case EVT_ROTARY_RIGHT: {
          int step = 1 + (rotaryEncoderGetAccel() * accelFactor) / 8;
#if defined(USE_HATS_AS_KEYS)
          adjust(-step);
#else
          adjust(step);
#endif
          lastRotaryValue = rotaryEncoderGetValue();
          return;
        }

        case EVT_ROTARY_LEFT: {
          int step = 1 + (rotaryEncoderGetAccel() * accelFactor) / 8;
#if defined(USE_HATS_AS_KEYS)
          adjust(step);
#else
          adjust(-step);
#endif
          lastRotaryValue = rotaryEncoderGetValue();
          return;
        }

        case EVT_KEY_BREAK(KEY_PLUS):
        case EVT_KEY_LONG(KEY_PLUS):
          adjust(fastStep);
          return;

        case EVT_KEY_BREAK(KEY_MINUS):
        case EVT_KEY_LONG(KEY_MINUS):
          adjust(-fastStep);
          return;
#endif
      }
    }

    TextButton::onLiveEvent(live, event);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (isEditMode()) {
      rotenc_t rotaryValue = rotaryEncoderGetValue();
      rotenc_t delta = rotaryValue - lastRotaryValue;
      if (delta != 0) {
        adjust(delta);
        lastRotaryValue = rotaryValue;
      }
    }
    TextButton::onLiveCheckEvents(live);
    update();
  }

  static void quickAdjustEventCb(lv_event_t* e)
  {
    auto cell = static_cast<QuickAdjustCell*>(lv_event_get_user_data(e));
    if (!cell || !cell->isAvailable()) return;

    switch (lv_event_get_key(e)) {
      case LV_KEY_LEFT:
        cell->onEvent(EVT_ROTARY_LEFT);
        break;
      case LV_KEY_RIGHT:
        cell->onEvent(EVT_ROTARY_RIGHT);
        break;
    }
  }
};

static void resetOutputLimits(uint8_t channel)
{
  LimitData* output = limitAddress(channel);
  output->min = 0;
  output->max = 0;
  output->offset = 0;
  output->ppmCenter = 0;
  output->revert = false;
  output->curve = 0;
  output->symetrical = 0;
  setOutputDirty();
}

enum class QuickTunePage : uint8_t { Expo, Rate, Min, Max };

static QuickTunePage s_quickTunePage = QuickTunePage::Expo;

static const char* quickTunePageLabel(QuickTunePage page)
{
  switch (page) {
    case QuickTunePage::Expo:
      return "Expo";
    case QuickTunePage::Rate:
      return "Rate";
    case QuickTunePage::Min:
      return STR_MIN;
    case QuickTunePage::Max:
      return STR_MAX;
  }
  return "Expo";
}

static bool quickTunePageIsInput(QuickTunePage page)
{
  return page == QuickTunePage::Expo || page == QuickTunePage::Rate;
}

static swsrc_t s_quickTuneSelectedSwitch = 0;
static swsrc_t s_quickTuneLastActiveSwitch = 0;

static std::vector<swsrc_t> quickTuneSwitchProfiles()
{
  std::vector<swsrc_t> profiles;
  for (uint8_t index = 0; index < MAX_EXPOS; index++) {
    ExpoData* input = expoAddress(index);
    if (!EXPO_VALID(input)) break;
    if (!input->swtch) continue;
    if (std::find(profiles.begin(), profiles.end(), input->swtch) ==
        profiles.end()) {
      profiles.push_back(input->swtch);
    }
  }
  return profiles;
}

static swsrc_t quickTuneActiveSwitch(const std::vector<swsrc_t>& profiles)
{
  for (swsrc_t swtch : profiles) {
    if (getSwitch(swtch)) return swtch;
  }
  return 0;
}

static bool quickTuneHasSwitchProfiles(const std::vector<swsrc_t>& profiles)
{
  return profiles.size() > 1;
}

static void syncQuickTuneSelectedProfile(const std::vector<swsrc_t>& profiles)
{
  if (!quickTuneHasSwitchProfiles(profiles)) {
    s_quickTuneSelectedSwitch = 0;
    s_quickTuneLastActiveSwitch = 0;
    return;
  }

  swsrc_t active = quickTuneActiveSwitch(profiles);
  if (active && active != s_quickTuneLastActiveSwitch) {
    s_quickTuneSelectedSwitch = active;
  } else if (s_quickTuneSelectedSwitch == 0) {
    s_quickTuneSelectedSwitch = active ? active : profiles.front();
  }

  if (std::find(profiles.begin(), profiles.end(), s_quickTuneSelectedSwitch) ==
      profiles.end()) {
    s_quickTuneSelectedSwitch = active ? active : profiles.front();
  }

  s_quickTuneLastActiveSwitch = active;
}

struct QuickTuneValueConfig {
  int vmin = 0;
  int vmax = 0;
  int fineStep = 1;
  int fastStep = 5;
  int accelFactor = 8;
  std::function<int()> getValue;
  std::function<void(int)> setValue;
  std::function<std::string(int)> displayValue;
};

#if LANDSCAPE
static const lv_coord_t quickTuneInputCols[] = {
    LV_GRID_FR(4), LV_GRID_FR(4), 34, LV_GRID_FR(10), 34, 76,
    LV_GRID_TEMPLATE_LAST};
static const lv_coord_t quickTuneOutputCols[] = {
    LV_GRID_FR(5), 34, LV_GRID_FR(14), 34, 76, LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t quickTuneInputCols[] = {LV_GRID_FR(4), 30,
                                                LV_GRID_FR(8), 30, 68,
                                                LV_GRID_TEMPLATE_LAST};
static const lv_coord_t* quickTuneOutputCols = quickTuneInputCols;
#endif
static const lv_coord_t quickTuneRows[] = {LV_GRID_CONTENT,
                                           LV_GRID_TEMPLATE_LAST};

static std::string percentString(int value)
{
  return std::to_string(value) + "%";
}

static std::string limitDisplayString(int value)
{
  if (g_eeGeneral.ppmunit == PPM_US) value = value * 128 / 25;
  return formatNumberAsString(value, PREC1);
}

static std::string quickInputLabel(const ExpoData* input, uint8_t index)
{
  std::string label(getSourceString(MIXSRC_FIRST_INPUT + input->chn));
  int lineNumber = inputLineNumber(index);
  if (lineNumber > 1) {
    label += " #";
    label += std::to_string(lineNumber);
  }
  return label;
}

static std::string quickInputProfileLabel(uint8_t input)
{
  return getSourceString(MIXSRC_FIRST_INPUT + input);
}

static int quickInputLineForProfile(uint8_t input, swsrc_t profileSwitch,
                                    bool& exactProfile)
{
  exactProfile = false;
  int fallback = -1;
  int always = -1;
  int active = -1;

  for (uint8_t index = 0; index < MAX_EXPOS; index++) {
    ExpoData* line = expoAddress(index);
    if (!EXPO_VALID(line)) break;
    if (line->chn != input) continue;

    if (profileSwitch && line->swtch == profileSwitch) {
      exactProfile = true;
      return index;
    }
    if (!profileSwitch && isExpoActive(index)) active = index;
    if (!line->swtch && always < 0) always = index;
    if (fallback < 0) fallback = index;
  }

  if (active >= 0) return active;
  if (always >= 0) return always;
  return fallback;
}

static QuickTuneValueConfig inputRateConfig(uint8_t index)
{
  return {-100,
          100,
          1,
          5,
          8,
          [=]() -> int { return sourceNumValue(expoAddress(index)->weight); },
          [=](int newValue) {
            MixerTaskLockGuard lock;
            expoAddress(index)->weight = makeSourceNumVal(newValue);
            SET_DIRTY();
          },
          percentString};
}

static QuickTuneValueConfig inputExpoConfig(uint8_t index)
{
  return {-100,
          100,
          1,
          5,
          8,
          [=]() -> int {
            return sourceNumValue(expoAddress(index)->curve.value);
          },
          [=](int newValue) {
            MixerTaskLockGuard lock;
            expoAddress(index)->curve.type = CURVE_REF_EXPO;
            expoAddress(index)->curve.value = makeSourceNumVal(newValue);
            SET_DIRTY();
          },
          percentString};
}

static QuickTuneValueConfig outputLimitConfig(uint8_t channel, bool minimum)
{
  int limit = (g_model.extendedLimits ? LIMIT_EXT_MAX : LIMIT_STD_MAX);
  return {minimum ? -limit : 0,
          minimum ? 0 : limit,
          5,
          20,
          16,
          [=]() -> int {
            LimitData* output = limitAddress(channel);
            return minimum ? output->min - LIMITS_MIN_MAX_OFFSET
                           : output->max + LIMITS_MIN_MAX_OFFSET;
          },
          [=](int newValue) {
            LimitData* output = limitAddress(channel);
            if (minimum)
              output->min = newValue + LIMITS_MIN_MAX_OFFSET;
            else
              output->max = newValue - LIMITS_MIN_MAX_OFFSET;
            setOutputDirty();
          },
          limitDisplayString};
}

static void applyQuickTuneDelta(const QuickTuneValueConfig& config, int delta)
{
  if (!config.getValue || !config.setValue) return;
  config.setValue(limit(config.vmin, config.getValue() + delta, config.vmax));
}

static QuickAdjustCell* newQuickTuneValueControls(
    Window* line, const QuickTuneValueConfig& config, bool focus = false)
{
  auto minus = new TextButton(line, rect_t{}, LV_SYMBOL_MINUS, [=]() {
    applyQuickTuneDelta(config, -config.fineStep);
    return 0;
  });
  minus->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  minus->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  minus->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);

  auto slider = new Slider(line, 100, config.vmin, config.vmax,
                           config.getValue, config.setValue);
  slider->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  slider->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  slider->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);

  auto plus = new TextButton(line, rect_t{}, LV_SYMBOL_PLUS, [=]() {
    applyQuickTuneDelta(config, config.fineStep);
    return 0;
  });
  plus->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  plus->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  plus->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);

  auto value = new QuickAdjustCell(line, config.vmin, config.vmax,
                                   config.getValue, config.setValue, "",
                                   config.displayValue);
  value->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  value->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  value->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  value->setFastStep(config.fastStep);
  value->setAccelFactor(config.accelFactor);
  if (focus) value->focus();

  return value;
}

static void newQuickTuneFallbackControls(
    Window* line, const std::string& value,
    std::function<uint8_t(void)> pressHandler)
{
  auto empty1 = new StaticText(line, rect_t{}, "");
  empty1->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  auto button = new TextButton(line, rect_t{}, value, std::move(pressHandler));
  button->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
  button->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  button->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  auto empty2 = new StaticText(line, rect_t{}, "");
  empty2->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
  auto editHint = new StaticText(line, rect_t{}, STR_EDIT,
                                 COLOR_THEME_SECONDARY1_INDEX);
  editHint->setStyleGridCellYAlign(LV_GRID_ALIGN_CENTER, 0);
}

void ModelInputsPage::openInputQuickMenu(uint8_t input, uint8_t index)
{
  Menu* menu = new Menu();
  menu->addLine(STR_EDIT, [=]() { editInput(input, index); });
  if (!reachExposLimit()) {
    menu->addLine(STR_INSERT_AFTER, [=]() { insertInput(input, index + 1); });
  }
  menu->addLine(STR_DELETE, [=]() { deleteInput(index); });
}

void ModelInputsPage::openOutputQuickMenu(uint8_t channel)
{
  Menu* menu = new Menu();
  menu->addLine(STR_EDIT, [=]() {
    auto edit = new OutputEditWindow(channel);
    edit->setCloseHandler([=]() { rebuildFromModel(); });
  });
  menu->addLine(STR_RESET, [=]() {
    resetOutputLimits(channel);
    rebuildFromModel();
  });
}

void ModelInputsPage::buildQuickTuneTabs(Window* window)
{
  auto row = new Window(window, rect_t{});
  row->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_TINY);
  row->setWidth(lv_pct(100));

  auto tabs = new Window(row, rect_t{});
  tabs->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_TINY);
  tabs->setFlexGrow(1);

  auto addTab = [&](QuickTunePage page) {
    auto button = new TextButton(tabs, rect_t{}, quickTunePageLabel(page), [=]() {
      s_quickTunePage = page;
      rebuildFromModel();
      return 0;
    });
    button->setFlexGrow(1);
    button->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
    button->check(s_quickTunePage == page);
  };

  addTab(QuickTunePage::Expo);
  addTab(QuickTunePage::Rate);
  addTab(QuickTunePage::Min);
  addTab(QuickTunePage::Max);

  auto profiles = quickTuneSwitchProfiles();
  syncQuickTuneSelectedProfile(profiles);
  if (quickTunePageIsInput(s_quickTunePage) &&
      quickTuneHasSwitchProfiles(profiles)) {
    std::vector<std::string> profileNames;
    profileNames.reserve(profiles.size());
    for (swsrc_t swtch : profiles) {
      profileNames.emplace_back(getSwitchPositionName(swtch));
    }

    auto selectedProfileIndex = [profiles]() -> int {
      auto it = std::find(profiles.begin(), profiles.end(),
                          s_quickTuneSelectedSwitch);
      return it == profiles.end() ? 0 : static_cast<int>(it - profiles.begin());
    };

    auto profileChoice = new Choice(
        row, rect_t{}, std::move(profileNames), 0,
        static_cast<int>(profiles.size() - 1),
        selectedProfileIndex, [this, profiles](int index) {
          if (index >= 0 && static_cast<size_t>(index) < profiles.size()) {
            s_quickTuneSelectedSwitch = profiles[index];
            rebuildFromModel();
          }
        });
    profileChoice->setWidth(72);
    profileChoice->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  }
}

void ModelInputsPage::buildQuickTuneRows(Window* window)
{
  const bool inputPage = quickTunePageIsInput(s_quickTunePage);
  const bool minimum = s_quickTunePage == QuickTunePage::Min;
  auto profiles = quickTuneSwitchProfiles();
  syncQuickTuneSelectedProfile(profiles);
  const bool profileMode = inputPage && quickTuneHasSwitchProfiles(profiles);
  FlexGridLayout grid(inputPage && !profileMode ? quickTuneInputCols
                                                : quickTuneOutputCols,
                      quickTuneRows, PAD_TINY);

  auto header = window->newLine(grid);
  new StaticText(header, rect_t{}, inputPage ? STR_MENUINPUTS : STR_MENULIMITS,
                 COLOR_THEME_SECONDARY1_INDEX, FONT(BOLD));
#if LANDSCAPE
  if (inputPage && !profileMode) {
    new StaticText(header, rect_t{}, STR_SWITCH, COLOR_THEME_SECONDARY1_INDEX,
                   FONT(BOLD));
  }
#endif
  new StaticText(header, rect_t{}, "", COLOR_THEME_SECONDARY1_INDEX,
                 FONT(BOLD));
  new StaticText(header, rect_t{}, "Slider", COLOR_THEME_SECONDARY1_INDEX,
                 FONT(BOLD));
  new StaticText(header, rect_t{}, "", COLOR_THEME_SECONDARY1_INDEX,
                 FONT(BOLD));
  new StaticText(header, rect_t{}, quickTunePageLabel(s_quickTunePage),
                 COLOR_THEME_SECONDARY1_INDEX, FONT(BOLD));

  bool focusSet = false;
  if (inputPage) {
    bool anyInput = false;
    for (uint8_t input = 0; input < MAX_INPUTS; input++) {
      bool exactProfile = false;
      int index = quickInputLineForProfile(input, s_quickTuneSelectedSwitch,
                                           exactProfile);
      if (index < 0) continue;
      anyInput = true;
      ExpoData* lineData = expoAddress(index);

      auto line = window->newLine(grid);
      new StaticText(line, rect_t{}, profileMode
                                      ? quickInputProfileLabel(input)
                                      : quickInputLabel(lineData, index));
#if LANDSCAPE
      if (!profileMode) {
        if (modelFMEnabled() && lineData->flightModes && !lineData->swtch) {
          new StaticText(line, rect_t{}, STR_FLMODE,
                         COLOR_THEME_SECONDARY1_INDEX);
        } else {
          auto sw = new SwitchChoice(
              line, rect_t{}, SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES,
              [=]() -> int16_t { return expoAddress(index)->swtch; },
              [=](int16_t newValue) {
                MixerTaskLockGuard lock;
                expoAddress(index)->swtch = newValue;
                SET_DIRTY();
              });
          sw->setStyleGridCellXAlign(LV_GRID_ALIGN_STRETCH, 0);
          sw->setTextHandler([=](int value) -> std::string {
            return value ? std::string(getSwitchPositionName(value))
                         : std::string("Always");
          });
        }
      }
#endif

      bool focus = shouldFocusLine(index, focusSet);
      if (profileMode && !exactProfile) {
        std::string inherited = "Inherited ";
        inherited += s_quickTunePage == QuickTunePage::Rate
                         ? sourceNumString(lineData->weight, "%")
                         : quickCurveString(lineData);
        newQuickTuneFallbackControls(line, inherited, [=]() {
          editInput(lineData->chn, index);
          return 0;
        });
      } else if (s_quickTunePage == QuickTunePage::Rate) {
        if (sourceNumIsSource(lineData->weight)) {
          newQuickTuneFallbackControls(line,
                                       sourceNumString(lineData->weight, "%"),
                                       [=]() {
                                         editInput(lineData->chn, index);
                                         return 0;
                                       });
        } else {
          newQuickTuneValueControls(line, inputRateConfig(index), focus);
        }
      } else {
        if (isQuickExpoEditable(lineData)) {
          newQuickTuneValueControls(line, inputExpoConfig(index), focus);
        } else {
          newQuickTuneFallbackControls(line, quickCurveString(lineData), [=]() {
            editInput(lineData->chn, index);
            return 0;
          });
        }
      }
    }

    if (!anyInput) {
      auto empty = window->newLine(grid);
      new StaticText(empty, rect_t{}, "No inputs configured",
                     COLOR_THEME_SECONDARY1_INDEX);
#if LANDSCAPE
      if (!profileMode) new StaticText(empty, rect_t{}, "");
#endif
      new StaticText(empty, rect_t{}, "");
      new StaticText(empty, rect_t{}, "");
      new StaticText(empty, rect_t{}, "");
      new StaticText(empty, rect_t{}, "");
    }
  } else {
    uint8_t rows = 0;
    for (uint8_t channel = 0; channel < MAX_OUTPUT_CHANNELS; channel++) {
      if (!outputChannelShouldShow(channel)) continue;
      rows++;

      auto line = window->newLine(grid);
      new StaticText(line, rect_t{}, getSourceString(MIXSRC_FIRST_CH + channel));

      if (outputLimitIsQuickEditable(channel, minimum)) {
        newQuickTuneValueControls(line, outputLimitConfig(channel, minimum),
                                  !focusSet);
        focusSet = true;
      } else {
        newQuickTuneFallbackControls(line, outputLimitString(channel, minimum),
                                     [=]() {
                                       openOutputQuickMenu(channel);
                                       return 0;
                                     });
      }
    }

    if (rows == 0) {
      const uint8_t fallbackRows = std::min<uint8_t>(4, MAX_OUTPUT_CHANNELS);
      for (uint8_t channel = 0; channel < fallbackRows; channel++) {
        auto line = window->newLine(grid);
        new StaticText(line, rect_t{},
                       getSourceString(MIXSRC_FIRST_CH + channel));
        newQuickTuneValueControls(line, outputLimitConfig(channel, minimum),
                                  channel == 0);
      }
    }
  }
}

void ModelInputsPage::checkEvents()
{
  if (quickTunePageIsInput(s_quickTunePage) && pageWindow) {
    auto profiles = quickTuneSwitchProfiles();
    if (quickTuneHasSwitchProfiles(profiles)) {
      swsrc_t active = quickTuneActiveSwitch(profiles);
      if (active && active != s_quickTuneLastActiveSwitch) {
        s_quickTuneSelectedSwitch = active;
        s_quickTuneLastActiveSwitch = active;
        rebuildFromModel();
        return;
      }
    }
  }
}

static const PageDef classicInputsPageDef = {
    ICON_MODEL_INPUTS, STR_DEF(STR_QM_INPUTS), STR_DEF(STR_MENUINPUTS),
    PAGE_CREATE, QM_MODEL_INPUTS, nullptr};

class ClassicInputsWindow : public Page
{
 public:
  ClassicInputsWindow() : Page(ICON_MODEL_INPUTS), classicPage(classicInputsPageDef)
  {
    header->setTitle(STR_MENUINPUTS);
    header->setTitle2("Advanced");
    classicPage.buildClassic(body);
  }

 protected:
  ModelInputsPage classicPage;
};

void ModelInputsPage::build(Window* window)
{
  bindPageWindow(window);

  // reset clipboard
  _copyMode = 0;
  _copySrc = nullptr;
  groups.clear();
  lines.clear();
  form = window;

  window->scrollbar();
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_TINY);
  window->setAutomationId("model.inputs.quick");

  buildQuickTuneTabs(window);
  buildQuickTuneRows(window);

  if (quickTunePageIsInput(s_quickTunePage)) {
    auto addButton = new TextButton(window, rect_t{}, LV_SYMBOL_PLUS " Add input",
                                    [=]() {
                                      newInput();
                                      return 0;
                                    });
    addButton->setWidth(lv_pct(100));
  }

  auto advancedButton = new TextButton(window, rect_t{}, "Advanced input lines",
                                       [=]() {
                                         new ClassicInputsWindow();
                                         return 0;
                                       });
  advancedButton->setWidth(lv_pct(100));
}
