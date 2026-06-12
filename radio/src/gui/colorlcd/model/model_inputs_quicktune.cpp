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
#include <functional>
#include <memory>
#include <vector>

#include "button.h"
#include "choice.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "hal/rotary_encoder.h"
#include "menu.h"
#include "numberedit.h"
#include "output_edit.h"
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

static bool isQuickExpoEditable(const ExpoData* input)
{
  return input->curve.type == CURVE_REF_EXPO &&
         !sourceNumIsSource(input->curve.value);
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

static const MixData* firstOutputMix(uint8_t channel)
{
  for (uint8_t i = 0; i < MAX_MIXERS; i++) {
    MixData* mix = &g_model.mixData[i];
    if (is_memclear(mix, sizeof(MixData))) break;
    if (mix->destCh == channel) return mix;
  }
  return nullptr;
}

static std::string quickSourceName(mixsrc_t source)
{
  if (source >= MIXSRC_FIRST_INPUT && source <= MIXSRC_LAST_INPUT) {
    uint8_t input = source - MIXSRC_FIRST_INPUT;
    if (input < MAX_INPUTS && g_model.inputNames[input][0] != '\0') {
      return std::string(g_model.inputNames[input],
                         strnlen(g_model.inputNames[input], LEN_INPUT_NAME));
    }

    const char* label = getMainControlLabel(input);
    if (label && label[0] != '\0') return label;

    return "I" + std::to_string(input + 1);
  }

  return getSourceString(source);
}

static std::string quickOutputLabel(uint8_t channel)
{
  std::string label(quickSourceName(MIXSRC_FIRST_CH + channel));
  const MixData* mix = firstOutputMix(channel);
  if (mix) {
    std::string source(quickSourceName(mix->srcRaw));
    if (!source.empty() && source != label) {
      label += " ";
      label += source;
    }
  }
  return label;
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

enum class QuickTunePage : uint8_t { Expo, Rate, Limits };

static QuickTunePage s_quickTunePage = QuickTunePage::Expo;

static const char* quickTunePageLabel(QuickTunePage page)
{
  switch (page) {
    case QuickTunePage::Expo:
      return "Expo";
    case QuickTunePage::Rate:
      return "Rate";
    case QuickTunePage::Limits:
      return "Limits";
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
  int vdefault = 0;
  bool hasDefault = false;
};

#if LANDSCAPE
#else
#endif
class QuickTuneActivityDot : public Window
{
 public:
  QuickTuneActivityDot(Window* parent, std::function<int()> getValue,
                       int threshold = 5) :
      Window(parent, rect_t{0, 0, 10, 10}),
      getValue(std::move(getValue)), threshold(threshold)
  {
    setWindowFlag(NO_SCROLL);
    withLive([&](LiveWindow& live) {
      lv_obj_clear_flag(live.lvobj(), LV_OBJ_FLAG_CLICKABLE);
      auto dot = lv_obj_create(live.lvobj());
      lv_obj_remove_style_all(dot);
      lv_obj_clear_flag(dot, static_cast<lv_obj_flag_t>(
                                LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_size(dot, 8, 8);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(dot, LV_OPA_0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(dot, lv_color_hex(0x58A6FF), LV_PART_MAIN);
      dotObj = dot;
    });
  }

 protected:
  std::function<int()> getValue;
  int threshold;
  lv_obj_t* dotObj = nullptr;
  int lastValue = 0;

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (!getValue || !dotObj) return;
    int v = getValue();
    if (v == lastValue) return;
    lastValue = v;
    bool active = std::abs(v) > threshold;
    if (active) {
      lv_obj_set_style_bg_opa(dotObj, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_opa(dotObj, LV_OPA_0, LV_PART_MAIN);
    }
  }
};

// Fill bar: dark pill with live stick-position indicator
class QuickTuneFillBar : public Window
{
 public:
  QuickTuneFillBar(Window* parent, uint8_t channel)
      : Window(parent, rect_t{}), channel(channel)
  {
    setWindowFlag(NO_SCROLL);
    setFlexGrow(1);
    withLive([this](LiveWindow& live) {
      auto* o = live.lvobj();
      etx_bg_color(o, COLOR_THEME_PRIMARY3_INDEX, LV_PART_MAIN);
      lv_obj_set_style_radius(o, 4, 0);
      lv_obj_set_style_pad_all(o, 0, 0);
      lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_border_width(o, 0, 0);

      fillBar = lv_obj_create(o);
      lv_obj_remove_style_all(fillBar);
      lv_obj_set_size(fillBar, 1, lv_obj_get_height(o));
      lv_obj_set_style_radius(fillBar, 3, 0);
      etx_bg_color(fillBar, COLOR_THEME_ACTIVE_INDEX, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(fillBar, LV_OPA_40, 0);
      lv_obj_align(fillBar, LV_ALIGN_LEFT_MID, 0, 0);
    });
    setFlexLayout(LV_FLEX_FLOW_ROW, 0);
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (!fillBar) return;
    int stick = calcRESXto100(getRawChannelOutput(channel));
    if (stick == lastFill) return;
    lastFill = stick;
    lv_coord_t cw = lv_obj_get_width(live.lvobj());
    if (cw <= 0) return;
    lv_coord_t fw = (stick + 100) * cw / 200;
    fw = std::max<lv_coord_t>(1, std::min(fw, cw));
    lv_obj_set_width(fillBar, fw);
  }

 private:
  uint8_t channel;
  lv_obj_t* fillBar = nullptr;
  int lastFill = -999;
};

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
          percentString,
          100, true};
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
          percentString,
          0, true};
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

struct LimitsWheelLiveContext {
  QuickTuneValueConfig minCfg;
  QuickTuneValueConfig maxCfg;
  int originalMin = 0;
  int originalMax = 0;
  lv_obj_t* minRoller = nullptr;
  lv_obj_t* maxRoller = nullptr;
  lv_obj_t* minLabel = nullptr;
  lv_obj_t* maxLabel = nullptr;
  bool suppressLiveApply = false;
};

static int limitsWheelValue(const QuickTuneValueConfig& cfg, lv_obj_t* roller)
{
  return cfg.vmin + lv_roller_get_selected(roller) * cfg.fineStep;
}

static void updateLimitsWheelLabels(LimitsWheelLiveContext* ctx)
{
  if (!ctx) return;
  if (ctx->minLabel && ctx->minRoller) {
    auto text = std::string("Min ") + ctx->minCfg.displayValue(limitsWheelValue(ctx->minCfg, ctx->minRoller));
    lv_label_set_text(ctx->minLabel, text.c_str());
  }
  if (ctx->maxLabel && ctx->maxRoller) {
    auto text = std::string("Max ") + ctx->maxCfg.displayValue(limitsWheelValue(ctx->maxCfg, ctx->maxRoller));
    lv_label_set_text(ctx->maxLabel, text.c_str());
  }
}

static void applyLimitsWheelLiveValue(LimitsWheelLiveContext* ctx)
{
  if (!ctx || ctx->suppressLiveApply || !ctx->minRoller || !ctx->maxRoller) return;
  ctx->minCfg.setValue(limitsWheelValue(ctx->minCfg, ctx->minRoller));
  ctx->maxCfg.setValue(limitsWheelValue(ctx->maxCfg, ctx->maxRoller));
  updateLimitsWheelLabels(ctx);
}

static void onLimitsWheelRollerChanged(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  applyLimitsWheelLiveValue(static_cast<LimitsWheelLiveContext*>(lv_event_get_user_data(e)));
}

static void onLimitsWheelDeleted(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  delete static_cast<LimitsWheelLiveContext*>(lv_event_get_user_data(e));
}

void ModelInputsPage::openLimitsWheel(uint8_t channel)
{
  auto cfg_min = outputLimitConfig(channel, true);
  auto cfg_max = outputLimitConfig(channel, false);
  auto* ctx = new LimitsWheelLiveContext{cfg_min, cfg_max,
                                         cfg_min.getValue(), cfg_max.getValue()};

  auto* wheel = new ModalWindow(true);
  wheel->withLive([ctx](Window::LiveWindow& live) {
    lv_obj_set_style_bg_color(live.lvobj(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(live.lvobj(), LV_OPA_80, 0);
    lv_obj_add_event_cb(live.lvobj(), onLimitsWheelDeleted, LV_EVENT_DELETE, ctx);
  });

  // Card
  lv_obj_t* cardObj = nullptr;
  auto* card = new Window(wheel, rect_t{
    (LV_HOR_RES - 340) / 2, (LV_VER_RES - 240) / 2, 340, 240
  });
  card->withLive([&](Window::LiveWindow& live) {
    cardObj = live.lvobj();
    etx_solid_bg(cardObj, COLOR_THEME_SECONDARY3_INDEX);
    lv_obj_set_style_radius(cardObj, 14, 0);
    etx_padding(cardObj, PAD_MEDIUM);
    lv_obj_clear_flag(cardObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(cardObj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cardObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cardObj, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cardObj, PAD_SMALL, 0);
  });

  // Title
  new StaticText(card, rect_t{}, quickOutputLabel(channel),
                 COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD) | CENTERED);

  // Build roller options
  std::string minOpts, maxOpts;
  for (int i = cfg_min.vmin; i <= cfg_min.vmax; i += cfg_min.fineStep) {
    if (i > cfg_min.vmin) minOpts += '\n';
    minOpts += limitDisplayString(i);
  }
  for (int i = cfg_max.vmin; i <= cfg_max.vmax; i += cfg_max.fineStep) {
    if (i > cfg_max.vmin) maxOpts += '\n';
    maxOpts += limitDisplayString(i);
  }
  int minIdx = (cfg_min.getValue() - cfg_min.vmin) / cfg_min.fineStep;
  int maxIdx = (cfg_max.getValue() - cfg_max.vmin) / cfg_max.fineStep;

  // Roller container: two columns, label above each roller, rollers flex-grow
  lv_obj_t* minRoller = nullptr;
  lv_obj_t* maxRoller = nullptr;
  auto* rollerContainer = new Window(card, rect_t{});
  rollerContainer->withLive([&, minOpts, maxOpts, minIdx, maxIdx, ctx](Window::LiveWindow& live) {
    auto* cont = live.lvobj();
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, PAD_LARGE, 0);
    lv_obj_set_width(cont, lv_pct(100));
    lv_obj_set_flex_grow(cont, 1);
    lv_obj_set_style_min_height(cont, 112, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Left column: Min label + roller
    auto* leftCol = lv_obj_create(cont);
    lv_obj_set_layout(leftCol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(leftCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftCol, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(leftCol, PAD_TINY, 0);
    lv_obj_set_flex_grow(leftCol, 1);
    lv_obj_set_width(leftCol, lv_pct(48));
    lv_obj_set_height(leftCol, lv_pct(100));
    lv_obj_clear_flag(leftCol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(leftCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftCol, 0, 0);
    lv_obj_set_style_pad_all(leftCol, 0, 0);

    auto* minLabel = lv_label_create(leftCol);
    ctx->minLabel = minLabel;
    lv_label_set_text(minLabel, "Min");
    etx_txt_color(minLabel, COLOR_THEME_SECONDARY1_INDEX, 0);
    etx_font(minLabel, FONT_XS_INDEX, 0);
    lv_obj_set_width(minLabel, lv_pct(100));
    lv_obj_set_style_text_align(minLabel, LV_TEXT_ALIGN_CENTER, 0);

    minRoller = lv_roller_create(leftCol);
    etx_font(minRoller, FONT_STD_INDEX, 0);
    lv_roller_set_options(minRoller, minOpts.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(minRoller, 5);
    lv_obj_set_style_text_align(minRoller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(minRoller, lv_pct(100));
    lv_obj_set_height(minRoller, lv_pct(100));
    lv_obj_set_flex_grow(minRoller, 1);
    lv_obj_set_style_text_color(minRoller, makeLvColor(COLOR_THEME_SECONDARY1), LV_PART_MAIN);
    lv_obj_set_style_text_color(minRoller, makeLvColor(COLOR_THEME_PRIMARY1), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(minRoller, makeLvColor(COLOR_THEME_ACTIVE), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(minRoller, LV_OPA_40, LV_PART_SELECTED);
    lv_obj_set_style_radius(minRoller, 8, LV_PART_SELECTED);
    if (minIdx >= 0) lv_roller_set_selected(minRoller, minIdx, LV_ANIM_OFF);
    ctx->minRoller = minRoller;
    lv_obj_add_event_cb(minRoller, onLimitsWheelRollerChanged,
                        LV_EVENT_VALUE_CHANGED, ctx);

    // Right column: Max label + roller
    auto* rightCol = lv_obj_create(cont);
    lv_obj_set_layout(rightCol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rightCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightCol, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(rightCol, PAD_TINY, 0);
    lv_obj_set_flex_grow(rightCol, 1);
    lv_obj_set_width(rightCol, lv_pct(48));
    lv_obj_set_height(rightCol, lv_pct(100));
    lv_obj_clear_flag(rightCol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(rightCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rightCol, 0, 0);
    lv_obj_set_style_pad_all(rightCol, 0, 0);

    auto* maxLabel = lv_label_create(rightCol);
    ctx->maxLabel = maxLabel;
    lv_label_set_text(maxLabel, "Max");
    etx_txt_color(maxLabel, COLOR_THEME_SECONDARY1_INDEX, 0);
    etx_font(maxLabel, FONT_XS_INDEX, 0);
    lv_obj_set_width(maxLabel, lv_pct(100));
    lv_obj_set_style_text_align(maxLabel, LV_TEXT_ALIGN_CENTER, 0);

    maxRoller = lv_roller_create(rightCol);
    etx_font(maxRoller, FONT_STD_INDEX, 0);
    lv_roller_set_options(maxRoller, maxOpts.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(maxRoller, 5);
    lv_obj_set_style_text_align(maxRoller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(maxRoller, lv_pct(100));
    lv_obj_set_height(maxRoller, lv_pct(100));
    lv_obj_set_flex_grow(maxRoller, 1);
    lv_obj_set_style_text_color(maxRoller, makeLvColor(COLOR_THEME_SECONDARY1), LV_PART_MAIN);
    lv_obj_set_style_text_color(maxRoller, makeLvColor(COLOR_THEME_PRIMARY1), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(maxRoller, makeLvColor(COLOR_THEME_ACTIVE), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(maxRoller, LV_OPA_40, LV_PART_SELECTED);
    lv_obj_set_style_radius(maxRoller, 8, LV_PART_SELECTED);
    if (maxIdx >= 0) lv_roller_set_selected(maxRoller, maxIdx, LV_ANIM_OFF);
    ctx->maxRoller = maxRoller;
    lv_obj_add_event_cb(maxRoller, onLimitsWheelRollerChanged,
                        LV_EVENT_VALUE_CHANGED, ctx);
    updateLimitsWheelLabels(ctx);

    // Group for rotary events
    lv_group_t* g = lv_group_create();
    lv_group_set_editing(g, true);
    lv_group_add_obj(g, minRoller);
    lv_group_add_obj(g, maxRoller);
    lv_group_focus_obj(minRoller);
    wheel->assignLvGroup(g, true);
  });

  // Button row
  auto* btnRow = new Window(card, rect_t{});
  btnRow->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  btnRow->withLive([](Window::LiveWindow& live) {
    lv_obj_set_layout(live.lvobj(), LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(live.lvobj(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(live.lvobj(), lv_pct(100));
    lv_obj_clear_flag(live.lvobj(), LV_OBJ_FLAG_SCROLLABLE);
  });
  auto scheduleRebuild = [this]() {
    auto* cb = new std::function<void()>([this]() { rebuildFromModel(); });
    lv_async_call([](void* data) {
      auto* fn = static_cast<std::function<void()>*>(data);
      (*fn)();
      delete fn;
    }, cb);
  };

  new TextButton(btnRow, rect_t{}, "Cancel", [=]() {
    ctx->suppressLiveApply = true;
    ctx->minCfg.setValue(ctx->originalMin);
    ctx->maxCfg.setValue(ctx->originalMax);
    wheel->deleteLater();
    scheduleRebuild();
    return 0;
  });
  new TextButton(btnRow, rect_t{}, "OK", [=]() {
    applyLimitsWheelLiveValue(ctx);
    wheel->deleteLater();
    scheduleRebuild();
    return 0;
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
  addTab(QuickTunePage::Limits);

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
  auto profiles = quickTuneSwitchProfiles();
  syncQuickTuneSelectedProfile(profiles);
  const bool profileMode = inputPage && quickTuneHasSwitchProfiles(profiles);

  // Responsive flex-wrap grid of tall cards
  auto* grid = new Window(window, rect_t{});
  grid->withLive([](Window::LiveWindow& live) {
    auto* o = live.lvobj();
    lv_obj_set_size(o, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(o, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(o, PAD_SMALL, 0);
    lv_obj_set_style_pad_column(o, PAD_SMALL, 0);
    lv_obj_set_style_pad_all(o, PAD_SMALL, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  });

  static const lv_coord_t cardH = EdgeTxStyles::UI_ELEMENT_HEIGHT * 2 + PAD_SMALL;

  // Shared card styling lambda
  auto styleCard = [](lv_obj_t* o) {
    etx_solid_bg(o, COLOR_THEME_SECONDARY3_INDEX, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 8, 0);
    etx_padding(o, PAD_SMALL, LV_PART_MAIN);
    etx_border_color(o, COLOR_THEME_SECONDARY2_INDEX, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_50, 0);
    lv_obj_set_layout(o, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(o, PAD_TINY, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  };

  // --- Expo / Rate cards ---
  auto addInputCard = [&](const std::string& label,
                         const QuickTuneValueConfig& cfg,
                         bool editable = true) {
    auto* card = new Window(grid, rect_t{});
    card->setWindowFlag(NO_SCROLL);
    card->withLive([styleCard](Window::LiveWindow& live) {
      auto* o = live.lvobj();
      lv_obj_set_size(o, lv_pct(23), cardH);
      lv_obj_set_style_min_width(o, 90, 0);
      styleCard(o);
    });

    // Bold title
    new StaticText(card, rect_t{}, label,
                   COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD) | CENTERED);

    if (!editable) {
      new StaticText(card, rect_t{},
                     cfg.displayValue ? cfg.displayValue(cfg.getValue())
                                      : std::to_string(cfg.getValue()),
                     COLOR_THEME_SECONDARY1_INDEX, CENTERED);
      return;
    }

    auto* val = new NumberEdit(card, rect_t{}, cfg.vmin, cfg.vmax,
                               cfg.getValue, cfg.setValue, CENTERED);
    val->setDisplayHandler(cfg.displayValue);
    val->setStep(cfg.fineStep);
    val->setFastStep(cfg.fastStep);
    val->setDirectKeyboard(false);
    if (cfg.hasDefault) val->setDefault(cfg.vdefault);
    val->setEditTitle(label);
    val->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  };

  // --- Limits card: the whole card opens the dual-roller ---
  auto addLimitsCard = [&](uint8_t channel) {
    auto cfg_min = outputLimitConfig(channel, true);
    auto cfg_max = outputLimitConfig(channel, false);
    auto* card = new TextButton(grid, rect_t{}, "", [=]() {
      openLimitsWheel(channel);
      return 0;
    });
    card->setWindowFlag(NO_SCROLL);
    card->withLive([styleCard](Window::LiveWindow& live) {
      auto* o = live.lvobj();
      lv_obj_set_size(o, lv_pct(23), 86);
      lv_obj_set_style_min_width(o, 90, 0);
      styleCard(o);
    });

    new StaticText(card, rect_t{}, quickOutputLabel(channel),
                   COLOR_THEME_PRIMARY1_INDEX, FONT(BOLD) | CENTERED);

    new StaticText(card, rect_t{},
                   std::string("Min ") + limitDisplayString(cfg_min.getValue()),
                   COLOR_THEME_SECONDARY1_INDEX, FONT(XS) | CENTERED);
    new StaticText(card, rect_t{},
                   std::string("Max ") + limitDisplayString(cfg_max.getValue()),
                   COLOR_THEME_SECONDARY1_INDEX, FONT(XS) | CENTERED);
  };

  // --- Populate ---
  if (inputPage) {
    for (uint8_t input = 0; input < MAX_INPUTS; input++) {
      bool exactProfile = false;
      int index = quickInputLineForProfile(input, s_quickTuneSelectedSwitch,
                                           exactProfile);
      if (index < 0) continue;
      ExpoData* lineData = expoAddress(index);

      std::string label = profileMode
                              ? quickInputProfileLabel(input)
                              : quickInputLabel(lineData, index);

      if (s_quickTunePage == QuickTunePage::Rate) {
        addInputCard(label, inputRateConfig(index),
                     !sourceNumIsSource(lineData->weight));
      } else {
        addInputCard(label, inputExpoConfig(index),
                     isQuickExpoEditable(lineData));
      }
    }
  } else {
    // Limits: one card per channel
    for (uint8_t channel = 0; channel < MAX_OUTPUT_CHANNELS; channel++) {
      if (!outputChannelShouldShow(channel)) continue;
      addLimitsCard(channel);
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
