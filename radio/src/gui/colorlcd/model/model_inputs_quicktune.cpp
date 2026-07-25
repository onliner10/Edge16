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
#include "dialog.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "hal/rotary_encoder.h"
#include "menu.h"
#include "menus.h"
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
static uint8_t s_quickTuneLastFlightMode = 0;

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
      lv_obj_set_style_pad_all(o, 0, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
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

// A limit min/max may hold a GVAR reference instead of a plain value; the
// quick card must never decode (or overwrite) those as percentages.
static bool outputLimitsGVarBound(uint8_t channel)
{
  const LimitData* output = limitAddress(channel);
  return GV_IS_GV_VALUE(output->min) || GV_IS_GV_VALUE(output->max);
}

// rawValue: the stored LimitData min/max; decodedValue: the same value with
// the min/max offset already removed (only meaningful when not GVAR-bound).
static std::string limitCellString(int16_t rawValue, int decodedValue)
{
  if (GV_IS_GV_VALUE(rawValue)) {
    char buf[16];
    getGVarString(buf, GV_INDEX_FROM_VALUE(rawValue));
    return buf;
  }
  return limitDisplayString(decodedValue);
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

// A resolved input line plus how well it matches the current flight mode and
// the selected switch profile.  A non-exact dimension means an edit must
// materialize a dedicated line first (copy-on-write) so the edit does not
// bleed into other flight modes / profiles.
struct QuickTuneLineRef {
  int index = -1;
  bool fmExact = false;
  bool profileExact = false;
};

static uint16_t quickTuneAllFlightModes()
{
  return (1 << MAX_FLIGHT_MODES) - 1;
}

// True when flight modes can actually change at runtime: the flight-modes
// feature must be enabled for the model AND another mode must have an
// activation switch assigned (FM0 is the only reachable mode otherwise,
// mirroring getFlightMode()).  Requiring modelFMEnabled() too keeps stale
// flightModeData[].swtch left behind by a disabled feature from creating FM
// splits/masks the Advanced editor does not even display.
static bool quickTuneFlightModesInUse()
{
  if (!modelFMEnabled()) return false;
  for (uint8_t i = 1; i < MAX_FLIGHT_MODES; i++) {
    if (g_model.flightModeData[i].swtch != SWSRC_NONE) return true;
  }
  return false;
}

// ExpoData::flightModes is a DISABLE bitmask: bit set = line disabled in that
// flight mode.
static bool quickTuneLineFMEnabled(const ExpoData* line, uint8_t flightMode)
{
  return (line->flightModes & (1 << flightMode)) == 0;
}

static bool quickTuneLineFMExact(const ExpoData* line, uint8_t flightMode)
{
  if (!quickTuneFlightModesInUse()) return true;
  return line->flightModes ==
         (quickTuneAllFlightModes() & ~(1 << flightMode));
}

static QuickTuneLineRef quickResolveInputLine(uint8_t input,
                                              swsrc_t profileSwitch,
                                              uint8_t flightMode)
{
  const bool fmAware = quickTuneFlightModesInUse();

  int found = -1;
  int profileFallback = -1;
  int fmActive = -1;
  int fmAlways = -1;
  int fmFallback = -1;
  int active = -1;
  int always = -1;
  int fallback = -1;

  for (uint8_t index = 0; index < MAX_EXPOS; index++) {
    ExpoData* line = expoAddress(index);
    if (!EXPO_VALID(line)) break;
    if (line->chn != input) continue;

    bool fmEnabled = !fmAware || quickTuneLineFMEnabled(line, flightMode);

    if (profileSwitch && line->swtch == profileSwitch) {
      // Exact profile match: the mixer picks the first line enabled in the
      // current flight mode (switch state intentionally ignored here).
      if (fmEnabled) {
        found = index;
        break;
      }
      if (profileFallback < 0) profileFallback = index;
      continue;
    }

    if (fmEnabled) {
      if (!profileSwitch && isExpoActive(index)) fmActive = index;
      if (!line->swtch && fmAlways < 0) fmAlways = index;
      if (fmFallback < 0) fmFallback = index;
    }
    if (!profileSwitch && isExpoActive(index)) active = index;
    if (!line->swtch && always < 0) always = index;
    if (fallback < 0) fallback = index;
  }

  if (found < 0) {
    // A profile-matching line wins even when it is FM-disabled in the
    // current flight mode: the card must keep tracking the selected profile,
    // and an edit still lands correctly because the FM mismatch forces a
    // copy-on-write split.  Intentional -- do not "fix" by preferring the
    // fm* candidates below.
    if (profileFallback >= 0)
      found = profileFallback;
    else if (fmActive >= 0)
      found = fmActive;
    else if (fmAlways >= 0)
      found = fmAlways;
    else if (fmFallback >= 0)
      found = fmFallback;
    else if (active >= 0)
      found = active;
    else if (always >= 0)
      found = always;
    else
      found = fallback;
  }

  QuickTuneLineRef ref;
  ref.index = found;
  if (found >= 0) {
    ExpoData* line = expoAddress(found);
    ref.fmExact = quickTuneLineFMExact(line, flightMode);
    ref.profileExact = profileSwitch != 0 && line->swtch == profileSwitch;
  }
  return ref;
}

// Validate a line index pinned when the card was built and refresh its
// exactness flags from the live line.  The pin keeps a whole edit session
// (wheel rotations, cancel restore) writing to the SAME line even when
// switch/stick movement would make quickResolveInputLine() pick a different
// one mid-session.  Re-deriving fmExact/profileExact (instead of reusing the
// build-time flags) is what limits a session to a single copy-on-write
// split: after quickTuneMaterializeLine() the materialized line sits at the
// pinned index and now IS exact, so later writes land in place.
//
// If the pinned line vanished (deleted underneath us -- normally impossible
// because the page rebuilds on overlay close and on FM/profile changes),
// fall back to a fresh resolve; callers must then re-check editability
// before writing.
static QuickTuneLineRef quickTuneSessionRef(int pinnedIndex, uint8_t input,
                                            swsrc_t profileSwitch,
                                            uint8_t flightMode)
{
  if (pinnedIndex >= 0 && pinnedIndex < MAX_EXPOS) {
    ExpoData* line = expoAddress(pinnedIndex);
    if (EXPO_VALID(line) && line->chn == input) {
      QuickTuneLineRef ref;
      ref.index = pinnedIndex;
      ref.fmExact = quickTuneLineFMExact(line, flightMode);
      ref.profileExact = profileSwitch != 0 && line->swtch == profileSwitch;
      return ref;
    }
  }
  return quickResolveInputLine(input, profileSwitch, flightMode);
}

// Copy-on-write bookkeeping: when an edit splits off a dedicated line, we
// remember it so the split can be undone if the edit session ends with the
// value unchanged (e.g. the wheel dialog was cancelled).
struct QuickTunePendingSplit {
  bool active = false;
  int newIndex = -1;
  uint8_t chn = 0;
  bool fmSplit = false;
  uint8_t flightMode = 0;
  uint16_t originFlightModes = 0;
  bool profileSplit = false;
  swsrc_t profileSwitch = 0;
};

static QuickTunePendingSplit s_quickTunePendingSplit;

// Materialize a dedicated line for the current flight mode / profile before
// writing to a shared line.  Returns the index the edit must be applied to.
static int quickTuneMaterializeLine(const QuickTuneLineRef& ref,
                                    swsrc_t profileSwitch, uint8_t flightMode)
{
  bool needsFMSplit = quickTuneFlightModesInUse() && !ref.fmExact;
  bool needsProfileSplit = profileSwitch != 0 && !ref.profileExact;
  if (!needsFMSplit && !needsProfileSplit) return ref.index;

  // Table full: fall back to editing the shown line in place.
  if (getExposCount() >= MAX_EXPOS) return ref.index;

  // A previous split can no longer be finalized once a new one starts.
  s_quickTunePendingSplit = {};

  uint16_t originFlightModes = expoAddress(ref.index)->flightModes;
  uint8_t chn = expoAddress(ref.index)->chn;

  // Insert a copy of the resolved line immediately before it, so the mixer
  // picks the new line first.  copyExpo() handles the mixer task stop/start
  // and storage dirty itself.
  ::copyExpo(ref.index, ref.index, chn);

  {
    MixerTaskLockGuard lock;
    ExpoData* line = expoAddress(ref.index);
    ExpoData* origin = expoAddress(ref.index + 1);
    if (needsFMSplit) {
      // New line: enabled only in the current flight mode.
      line->flightModes = quickTuneAllFlightModes() & ~(1 << flightMode);
      // Original line no longer applies in the current flight mode.
      origin->flightModes |= (1 << flightMode);
    }
    if (needsProfileSplit) line->swtch = profileSwitch;
    SET_DIRTY();
  }

  s_quickTunePendingSplit.active = true;
  s_quickTunePendingSplit.newIndex = ref.index;
  s_quickTunePendingSplit.chn = chn;
  s_quickTunePendingSplit.fmSplit = needsFMSplit;
  s_quickTunePendingSplit.flightMode = flightMode;
  s_quickTunePendingSplit.originFlightModes = originFlightModes;
  s_quickTunePendingSplit.profileSplit = needsProfileSplit;
  s_quickTunePendingSplit.profileSwitch = profileSwitch;

  return ref.index;
}

// Equality of everything a quick-tune edit can touch, ignoring the fields the
// split itself changed (swtch, flightModes).
static bool quickTuneLinesEquivalent(const ExpoData* a, const ExpoData* b)
{
  ExpoData ca, cb;
  memcpy(&ca, a, sizeof(ExpoData));
  memcpy(&cb, b, sizeof(ExpoData));
  ca.swtch = 0;
  ca.flightModes = 0;
  cb.swtch = 0;
  cb.flightModes = 0;
  return memcmp(&ca, &cb, sizeof(ExpoData)) == 0;
}

// If the last split ended up holding the same value as the line it was split
// from (edit cancelled or committed unchanged), remove the redundant line and
// restore the origin line's flight modes.  Returns true if a pending split
// was processed (caller should rebuild).
static bool quickTuneFinalizePendingSplit()
{
  if (!s_quickTunePendingSplit.active) return false;
  QuickTunePendingSplit split = s_quickTunePendingSplit;
  s_quickTunePendingSplit = {};

  // Validate that the model still looks exactly like we left it; if anything
  // moved underneath us, keep the materialized line as-is.
  if (split.newIndex < 0 || split.newIndex + 1 >= MAX_EXPOS) return true;
  ExpoData* line = expoAddress(split.newIndex);
  ExpoData* origin = expoAddress(split.newIndex + 1);
  if (!EXPO_VALID(line) || !EXPO_VALID(origin)) return true;
  if (line->chn != split.chn || origin->chn != split.chn) return true;
  if (split.profileSplit && line->swtch != split.profileSwitch) return true;
  if (split.fmSplit) {
    if (line->flightModes !=
        (quickTuneAllFlightModes() & ~(1 << split.flightMode)))
      return true;
    if (origin->flightModes !=
        (split.originFlightModes | (1 << split.flightMode)))
      return true;
  }
  if (!quickTuneLinesEquivalent(line, origin)) return true;

  ::deleteExpo(split.newIndex);
  if (split.fmSplit) {
    MixerTaskLockGuard lock;
    expoAddress(split.newIndex)->flightModes = split.originFlightModes;
    SET_DIRTY();
  }
  return true;
}

// pinnedIndex is the line resolved when the card was built; the closures pin
// the whole edit session to it via quickTuneSessionRef() instead of
// re-resolving against live mixer state on every get/set.
static QuickTuneValueConfig inputRateConfig(int pinnedIndex, uint8_t input,
                                            swsrc_t profileSwitch,
                                            uint8_t flightMode)
{
  return {-100,
          100,
          1,
          5,
          8,
          [=]() -> int {
            auto ref = quickTuneSessionRef(pinnedIndex, input, profileSwitch,
                                           flightMode);
            if (ref.index < 0) return 0;
            if (sourceNumIsSource(expoAddress(ref.index)->weight)) return 0;
            return sourceNumValue(expoAddress(ref.index)->weight);
          },
          [=](int newValue) {
            auto ref = quickTuneSessionRef(pinnedIndex, input, profileSwitch,
                                           flightMode);
            if (ref.index < 0) return;
            // The card's editable rule: never overwrite a source-bound
            // weight with a plain value (matters on the fallback path, where
            // the resolved line may differ from the one the card showed).
            if (sourceNumIsSource(expoAddress(ref.index)->weight)) return;
            int index = quickTuneMaterializeLine(ref, profileSwitch, flightMode);
            MixerTaskLockGuard lock;
            expoAddress(index)->weight = makeSourceNumVal(newValue);
            SET_DIRTY();
          },
          percentString,
          100, true};
}

static QuickTuneValueConfig inputExpoConfig(int pinnedIndex, uint8_t input,
                                            swsrc_t profileSwitch,
                                            uint8_t flightMode)
{
  return {-100,
          100,
          1,
          5,
          8,
          [=]() -> int {
            auto ref = quickTuneSessionRef(pinnedIndex, input, profileSwitch,
                                           flightMode);
            if (ref.index < 0) return 0;
            if (!isQuickExpoEditable(expoAddress(ref.index))) return 0;
            return sourceNumValue(expoAddress(ref.index)->curve.value);
          },
          [=](int newValue) {
            auto ref = quickTuneSessionRef(pinnedIndex, input, profileSwitch,
                                           flightMode);
            if (ref.index < 0) return;
            // The card's editable rule: never stomp a custom/function curve
            // or a source-driven expo (matters on the fallback path, where
            // the resolved line may differ from the one the card showed).
            if (!isQuickExpoEditable(expoAddress(ref.index))) return;
            int index = quickTuneMaterializeLine(ref, profileSwitch, flightMode);
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

void ModelInputsPage::openOutputQuickMenu(uint8_t channel)
{
  Menu* menu = new Menu();
  menu->addLine(STR_EDIT, [=]() {
    auto edit = new OutputEditWindow(channel, Route{});
    edit->setCloseHandler([=]() { rebuildFromModel(); });
  });
  menu->addLine(STR_RESET, [=]() {
    // Wipes this channel's output min/max/offset/centre/curve in one tap --
    // name the channel so the confirm reads as a continuation of the tap
    // rather than a generic "Reset".
    confirmDestructive(STR_RESET, quickOutputLabel(channel).c_str(), [=]() {
      resetOutputLimits(channel);
      rebuildFromModel();
    });
  });
}

struct LimitsWheelLiveContext {
  QuickTuneValueConfig minCfg;
  QuickTuneValueConfig maxCfg;
  int originalMin = 0;
  int originalMax = 0;
  // Roller index each roller started on (original value rounded to the
  // roller grid and clamped to its range).  A roller only writes to the
  // model once the user moves it off this index; an untouched roller keeps
  // the exact original raw value (which may be off-grid or out of range).
  int initialMinIdx = 0;
  int initialMaxIdx = 0;
  bool minTouched = false;
  bool maxTouched = false;
  lv_obj_t* minRoller = nullptr;
  lv_obj_t* maxRoller = nullptr;
  lv_obj_t* minLabel = nullptr;
  lv_obj_t* maxLabel = nullptr;
  lv_obj_t* cancelBtn = nullptr;
  lv_obj_t* okBtn = nullptr;
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
    // Untouched roller: show the exact original value (may be off-grid).
    int value = ((int)lv_roller_get_selected(ctx->minRoller) ==
                 ctx->initialMinIdx)
                    ? ctx->originalMin
                    : limitsWheelValue(ctx->minCfg, ctx->minRoller);
    auto text = std::string("Min ") + ctx->minCfg.displayValue(value);
    lv_label_set_text(ctx->minLabel, text.c_str());
  }
  if (ctx->maxLabel && ctx->maxRoller) {
    int value = ((int)lv_roller_get_selected(ctx->maxRoller) ==
                 ctx->initialMaxIdx)
                    ? ctx->originalMax
                    : limitsWheelValue(ctx->maxCfg, ctx->maxRoller);
    auto text = std::string("Max ") + ctx->maxCfg.displayValue(value);
    lv_label_set_text(ctx->maxLabel, text.c_str());
  }
}

static void applyLimitsWheelLiveValue(LimitsWheelLiveContext* ctx)
{
  if (!ctx || ctx->suppressLiveApply || !ctx->minRoller || !ctx->maxRoller) return;
  // Only write the roller(s) the user actually moved; an untouched roller
  // must keep its exact original raw value (the roller grid quantizes to
  // fineStep and clamps out-of-range values).
  if ((int)lv_roller_get_selected(ctx->minRoller) != ctx->initialMinIdx) {
    ctx->minTouched = true;
    ctx->minCfg.setValue(limitsWheelValue(ctx->minCfg, ctx->minRoller));
  } else if (ctx->minTouched) {
    // Moved away and back: restore the exact original value.
    ctx->minTouched = false;
    ctx->minCfg.setValue(ctx->originalMin);
  }
  if ((int)lv_roller_get_selected(ctx->maxRoller) != ctx->initialMaxIdx) {
    ctx->maxTouched = true;
    ctx->maxCfg.setValue(limitsWheelValue(ctx->maxCfg, ctx->maxRoller));
  } else if (ctx->maxTouched) {
    ctx->maxTouched = false;
    ctx->maxCfg.setValue(ctx->originalMax);
  }
  updateLimitsWheelLabels(ctx);
}

static void onLimitsWheelRollerChanged(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  // lv_roller's own VALUE_CHANGED handler clears the group's edit mode after
  // every encoder-driven change; re-assert it so the next detent keeps
  // adjusting the roller instead of cycling group focus (same fix as
  // NumberWheel::onRollerChanged).
  lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
  lv_group_t* g = lv_obj_get_group(roller);
  if (g) lv_group_set_editing(g, true);
  applyLimitsWheelLiveValue(static_cast<LimitsWheelLiveContext*>(lv_event_get_user_data(e)));
}

// Move group focus to `next` from inside a keypad ENTER-press handler.
// wait_release stops the ENTER release from click-activating the newly
// focused object; reset stops the in-flight press from leaving the current
// object in pressed state.  Same mechanics as NumberWheel::onRollerKey.
static void limitsWheelFocusObj(lv_obj_t* next)
{
  lv_group_t* g = next ? lv_obj_get_group(next) : nullptr;
  if (!g) return;
  lv_group_focus_obj(next);
  lv_group_set_editing(g, true);
  lv_indev_t* indev = lv_indev_active();
  if (indev) {
    lv_indev_wait_release(indev);
    lv_indev_reset(indev, nullptr);
  }
}

static void onLimitsWheelRollerKey(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  auto* ctx = static_cast<LimitsWheelLiveContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  uint32_t key = lv_event_get_key(e);
  lv_obj_t* roller = static_cast<lv_obj_t*>(lv_event_get_target(e));

  if (key == LV_KEY_ESC) {
    if (ctx->cancelBtn) lv_obj_send_event(ctx->cancelBtn, LV_EVENT_CLICKED, nullptr);
  } else if (key == LV_KEY_ENTER) {
    // Tab order: min roller -> max roller -> OK button.
    limitsWheelFocusObj(roller == ctx->minRoller ? ctx->maxRoller : ctx->okBtn);
  } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT ||
             key == LV_KEY_UP || key == LV_KEY_DOWN) {
    // The roller class handler already moved the selection by 1; add
    // accelerated extra steps to match inline-edit feel.
    int dir = (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) ? 1 : -1;
    const QuickTuneValueConfig& cfg =
        (roller == ctx->minRoller) ? ctx->minCfg : ctx->maxCfg;
    int extra = (rotaryEncoderGetAccel() * cfg.accelFactor) / 8;
    if (extra > 0) {
      int cur = (int)lv_roller_get_selected(roller);
      int cnt = (int)lv_roller_get_option_count(roller);
      int next = LV_CLAMP(0, cur + dir * extra, cnt - 1);
      if (next != cur) lv_roller_set_selected(roller, next, LV_ANIM_OFF);
    }
    applyLimitsWheelLiveValue(ctx);
  }
}

static void onLimitsWheelButtonKey(lv_event_t* e)
{
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  auto* ctx = static_cast<LimitsWheelLiveContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  uint32_t key = lv_event_get_key(e);
  lv_obj_t* cur = static_cast<lv_obj_t*>(lv_event_get_target(e));

  if (key == LV_KEY_ESC) {
    if (ctx->cancelBtn) lv_obj_send_event(ctx->cancelBtn, LV_EVENT_CLICKED, nullptr);
  } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT ||
             key == LV_KEY_UP || key == LV_KEY_DOWN) {
    // Toggle focus between Cancel and OK; re-assert editing so the next
    // detent keeps cycling buttons instead of moving group focus.
    lv_obj_t* other = (cur == ctx->okBtn) ? ctx->cancelBtn : ctx->okBtn;
    lv_group_t* g = other ? lv_obj_get_group(other) : nullptr;
    if (g) {
      lv_group_focus_obj(other);
      lv_group_set_editing(g, true);
    }
  }
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
    etx_padding(cardObj, PAD_MEDIUM);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_clear_flag(cardObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(cardObj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cardObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cardObj, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cardObj, PAD_SMALL, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
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
  // Nearest roller entry, clamped to the roller range.  Off-grid or
  // out-of-range originals are only displayed here; they stay untouched in
  // storage until the user actually moves the roller.
  auto rollerIndex = [](const QuickTuneValueConfig& cfg) -> int {
    int count = (cfg.vmax - cfg.vmin) / cfg.fineStep + 1;
    int idx = (cfg.getValue() - cfg.vmin + cfg.fineStep / 2) / cfg.fineStep;
    return limit(0, idx, count - 1);
  };
  int minIdx = rollerIndex(cfg_min);
  int maxIdx = rollerIndex(cfg_max);
  ctx->initialMinIdx = minIdx;
  ctx->initialMaxIdx = maxIdx;

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
    lv_obj_set_style_pad_column(cont, PAD_LARGE, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
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
    lv_obj_set_style_pad_row(leftCol, PAD_TINY, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_set_flex_grow(leftCol, 1);
    lv_obj_set_width(leftCol, lv_pct(48));
    lv_obj_set_height(leftCol, lv_pct(100));
    lv_obj_clear_flag(leftCol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(leftCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftCol, 0, 0);
    lv_obj_set_style_pad_all(leftCol, 0, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.

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
    lv_obj_add_event_cb(minRoller, onLimitsWheelRollerKey, LV_EVENT_KEY, ctx);

    // Right column: Max label + roller
    auto* rightCol = lv_obj_create(cont);
    lv_obj_set_layout(rightCol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rightCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightCol, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(rightCol, PAD_TINY, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_set_flex_grow(rightCol, 1);
    lv_obj_set_width(rightCol, lv_pct(48));
    lv_obj_set_height(rightCol, lv_pct(100));
    lv_obj_clear_flag(rightCol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(rightCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rightCol, 0, 0);
    lv_obj_set_style_pad_all(rightCol, 0, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.

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
    lv_obj_add_event_cb(maxRoller, onLimitsWheelRollerKey, LV_EVENT_KEY, ctx);
    updateLimitsWheelLabels(ctx);

    // Group for rotary events.  set_editing must come AFTER focus_obj:
    // lv_group_focus_obj always clears edit mode, which would leave the
    // encoder cycling focus instead of adjusting the focused roller.
    lv_group_t* g = lv_group_create();
    lv_group_add_obj(g, minRoller);
    lv_group_add_obj(g, maxRoller);
    lv_group_focus_obj(minRoller);
    lv_group_set_editing(g, true);
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

  auto* cancelBtn = new TextButton(btnRow, rect_t{}, "Cancel", [=]() {
    ctx->suppressLiveApply = true;
    // Untouched rollers never wrote to the model; only undo live-applied
    // changes.
    if (ctx->minTouched) ctx->minCfg.setValue(ctx->originalMin);
    if (ctx->maxTouched) ctx->maxCfg.setValue(ctx->originalMax);
    lv_indev_reset(nullptr, nullptr);
    wheel->deleteLater();
    scheduleRebuild();
    return 0;
  });
  auto* okBtn = new TextButton(btnRow, rect_t{}, "OK", [=]() {
    applyLimitsWheelLiveValue(ctx);
    lv_indev_reset(nullptr, nullptr);
    wheel->deleteLater();
    scheduleRebuild();
    return 0;
  });

  // Add buttons to the rotary group so the encoder click chain works:
  // min roller -> max roller -> OK; rotation then toggles Cancel/OK and the
  // next click activates the focused button (keypad indev clicks natively).
  auto attachBtn = [ctx](TextButton* btn, lv_obj_t** slot) {
    if (!btn) return;
    btn->withLive([ctx, slot](Window::LiveWindow& live) {
      *slot = live.lvobj();
      lv_group_t* g =
          ctx->minRoller ? lv_obj_get_group(ctx->minRoller) : nullptr;
      if (g) lv_group_add_obj(g, live.lvobj());
      lv_obj_add_event_cb(live.lvobj(), onLimitsWheelButtonKey, LV_EVENT_KEY,
                          ctx);
    });
  };
  attachBtn(cancelBtn, &ctx->cancelBtn);
  attachBtn(okBtn, &ctx->okBtn);
}

void ModelInputsPage::buildQuickTuneTabs(Window* window)
{
  auto row = new Window(window, rect_t{});
  row->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_TINY);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
  row->setWidth(lv_pct(100));

  auto tabs = new Window(row, rect_t{});
  tabs->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_TINY);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
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

  if (quickTunePageIsInput(s_quickTunePage) && quickTuneFlightModesInUse()) {
    // Show which flight mode Expo/Rate edits apply to.
    std::string fmName = stringFromNtString(
        g_model.flightModeData[mixerCurrentFlightMode].name);
    if (fmName.empty()) {
      char fmId[8];
      fmName = getFlightModeString(fmId, mixerCurrentFlightMode + 1);
    }
    auto fmChip = new StaticText(row, rect_t{}, fmName,
                                 COLOR_THEME_SECONDARY1_INDEX, CENTERED);
    fmChip->setHeight(EdgeTxStyles::UI_ELEMENT_HEIGHT);
  }

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
    lv_obj_set_style_pad_row(o, PAD_SMALL, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_set_style_pad_column(o, PAD_SMALL, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_set_style_pad_all(o, PAD_SMALL, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  });

  static const lv_coord_t cardH = EdgeTxStyles::UI_ELEMENT_HEIGHT * 2 + PAD_SMALL;  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.

  // Shared card styling lambda
  auto styleCard = [](lv_obj_t* o) {
    etx_solid_bg(o, COLOR_THEME_SECONDARY3_INDEX, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 8, 0);
    etx_padding(o, PAD_SMALL, LV_PART_MAIN);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    etx_border_color(o, COLOR_THEME_SECONDARY2_INDEX, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_50, 0);
    lv_obj_set_layout(o, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(o, PAD_TINY, 0);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  };

  // --- Expo / Rate cards ---
  // readOnlyText: shown instead of the numeric value when not editable
  // (source/GVAR/curve-ref values must not be rendered as percentages).
  auto addInputCard = [&](const std::string& label,
                         const QuickTuneValueConfig& cfg,
                         bool editable = true,
                         const std::string& readOnlyText = std::string()) {
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
      std::string text = readOnlyText;
      if (text.empty()) {
        text = cfg.displayValue ? cfg.displayValue(cfg.getValue())
                                : std::to_string(cfg.getValue());
      }
      new StaticText(card, rect_t{}, text, COLOR_THEME_SECONDARY1_INDEX,
                     CENTERED);
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
    // GVAR-bound limits stay read-only here: show the GVAR name(s) and keep
    // editing in the advanced Outputs page.
    const bool gvarBound = outputLimitsGVarBound(channel);
    auto* card = new TextButton(grid, rect_t{}, "", [=]() {
      if (!gvarBound) openLimitsWheel(channel);
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

    const LimitData* output = limitAddress(channel);
    new StaticText(card, rect_t{},
                   std::string("Min ") +
                       limitCellString(output->min, cfg_min.getValue()),
                   COLOR_THEME_SECONDARY1_INDEX, FONT(XS) | CENTERED);
    new StaticText(card, rect_t{},
                   std::string("Max ") +
                       limitCellString(output->max, cfg_max.getValue()),
                   COLOR_THEME_SECONDARY1_INDEX, FONT(XS) | CENTERED);
  };

  // --- Populate ---
  if (inputPage) {
    // Captured per card so a whole wheel-edit session stays pinned to the
    // flight mode it was opened in; checkEvents() rebuilds on FM change.
    const uint8_t flightMode = mixerCurrentFlightMode;
    const bool fmInUse = quickTuneFlightModesInUse();
    // The "#n" suffix disambiguates only when a channel genuinely has
    // several lines active at once (e.g. switch-gated dual rates): in
    // profile mode the selector already disambiguates, and a pure FM split
    // has one line per flight mode.
    auto channelEnabledLineCount = [&](uint8_t chn) -> int {
      int count = 0;
      for (uint8_t i = 0; i < MAX_EXPOS; i++) {
        ExpoData* line = expoAddress(i);
        if (!EXPO_VALID(line)) break;
        if (line->chn != chn) continue;
        if (fmInUse && !quickTuneLineFMEnabled(line, flightMode)) continue;
        count += 1;
      }
      return count;
    };
    for (uint8_t input = 0; input < MAX_INPUTS; input++) {
      auto ref = quickResolveInputLine(input, s_quickTuneSelectedSwitch,
                                       flightMode);
      if (ref.index < 0) continue;
      ExpoData* lineData = expoAddress(ref.index);

      std::string label = (profileMode || channelEnabledLineCount(input) <= 1)
                              ? quickInputProfileLabel(input)
                              : quickInputLabel(lineData, ref.index);

      if (s_quickTunePage == QuickTunePage::Rate) {
        const bool editable = !sourceNumIsSource(lineData->weight);
        std::string readOnlyText;
        if (!editable) {
          // Weight is a source/GVAR reference: show its name.
          char buf[32];
          getValueOrSrcVarString(buf, sizeof(buf), lineData->weight, 0, "%");
          readOnlyText = buf;
        }
        addInputCard(label,
                     inputRateConfig(ref.index, input,
                                     s_quickTuneSelectedSwitch, flightMode),
                     editable, readOnlyText);
      } else {
        const bool editable = isQuickExpoEditable(lineData);
        std::string readOnlyText;
        if (!editable) {
          // Function/custom-curve or source-driven expo: show the curve
          // reference, not a bogus percentage.
          char buf[32] = "";
          getCurveRefString(buf, sizeof(buf), lineData->curve);
          readOnlyText = buf;
        }
        addInputCard(label,
                     inputExpoConfig(ref.index, input,
                                     s_quickTuneSelectedSwitch, flightMode),
                     editable, readOnlyText);
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

// True when something (wheel dialog, menu, another page...) is stacked above
// the page group this page lives in.  Rebuilding the page underneath such an
// overlay would destroy the widgets it is attached to, so model-driven
// rebuilds are deferred until the overlay is gone (checkEvents keeps polling).
static bool quickTuneOverlayActive()
{
  Window* top = Window::topWindow();
  return top && !top->isPageGroup();
}

void ModelInputsPage::checkEvents()
{
  // The classic (advanced) list needs none of the quick-tune polling.
  if (classicBuild) return;

  if (!pageWindow) return;
  if (quickTuneOverlayActive()) return;

  if (quickTuneFinalizePendingSplit()) {
    rebuildFromModel();
    return;
  }

  if (quickTunePageIsInput(s_quickTunePage)) {
    if (quickTuneFlightModesInUse() &&
        mixerCurrentFlightMode != s_quickTuneLastFlightMode) {
      s_quickTuneLastFlightMode = mixerCurrentFlightMode;
      rebuildFromModel();
      return;
    }

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
  ClassicInputsWindow() : Page(ICON_MODEL_INPUTS, Route{}), classicPage(classicInputsPageDef)
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
  // Rebuilds (rebuildFromModel) of an instance built via buildClassic() must
  // stay in the classic view; the quick builder below is only for the
  // quick-tune tab instance.
  if (classicBuild) {
    buildClassic(window);
    return;
  }

  bindPageWindow(window);

  // reset clipboard
  _copyMode = 0;
  _copySrc = nullptr;
  groups.clear();
  lines.clear();
  form = window;

  window->scrollbar();
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_TINY);  // ds-allow: quick-tune tiles - bespoke two-column expo/rate cards with per-card padding/row-column gaps tuned to the tile layout; custom card grid, not a DS FormRow/ListRow.
  window->setAutomationId("model.inputs.quick");

  s_quickTuneLastFlightMode = mixerCurrentFlightMode;

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

  auto advancedButton = new TextButton(
      window, rect_t{}, "Advanced input lines", [=]() {
        auto advanced = new ClassicInputsWindow();
        // Lines may have been added/edited/deleted there; rebuild so the
        // quick cards reflect the current model.  checkEvents() cannot see
        // this transition itself: an OPAQUE Page suspends checkEvents() of
        // everything underneath it.
        advanced->setCloseHandler([=]() {
          quickTuneFinalizePendingSplit();
          rebuildFromModel();
        });
        return 0;
      });
  advancedButton->setWidth(lv_pct(100));
}
