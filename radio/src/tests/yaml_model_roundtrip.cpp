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

#include "gtests.h"

#include "analogs.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "storage/yaml/yaml_datastructs.h"
#include "storage/yaml/yaml_parser.h"
#include "storage/yaml/yaml_tree_walker.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kVariantCount = 128;
static_assert(kVariantCount >= 100, "YAML corpus must keep at least 100 variants");

struct StringYamlWriter {
  std::string yaml;

  static bool write(void* opaque, const char* str, size_t len)
  {
    auto* writer = static_cast<StringYamlWriter*>(opaque);
    writer->yaml.append(str, len);
    return true;
  }
};

struct CorpusStats {
  std::vector<std::string> topLevelKeys;
  std::vector<uint64_t> hashes;

  void observe(const std::string& yaml)
  {
    const uint64_t hash = fnv1a64(yaml);
    if (std::find(hashes.begin(), hashes.end(), hash) == hashes.end()) {
      hashes.push_back(hash);
    }

    size_t lineStart = 0;
    while (lineStart < yaml.size()) {
      size_t lineEnd = yaml.find('\n', lineStart);
      if (lineEnd == std::string::npos) {
        lineEnd = yaml.size();
      }

      if (lineStart < lineEnd && yaml[lineStart] != ' ' && yaml[lineStart] != '-' &&
          yaml[lineStart] != '\r') {
        const size_t colon = yaml.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd) {
          std::string key = yaml.substr(lineStart, colon - lineStart);
          if (std::find(topLevelKeys.begin(), topLevelKeys.end(), key) ==
              topLevelKeys.end()) {
            topLevelKeys.push_back(key);
          }
        }
      }

      lineStart = lineEnd + 1;
    }
  }

 private:
  static uint64_t fnv1a64(const std::string& value)
  {
    uint64_t hash = 1469598103934665603ULL;
    for (char ch : value) {
      hash ^= static_cast<uint8_t>(ch);
      hash *= 1099511628211ULL;
    }
    return hash;
  }
};

std::string writeYamlToString(const YamlNode* nodes, uint8_t* data)
{
  YamlTreeWalker tree;
  tree.reset(nodes, data);

  StringYamlWriter writer;
  EXPECT_TRUE(tree.generate(StringYamlWriter::write, &writer));
  return writer.yaml;
}

void readYamlFromString(const YamlNode* nodes, uint8_t* data,
                        const std::string& yaml)
{
  YamlTreeWalker tree;
  tree.reset(nodes, data);

  YamlParser parser;
  parser.init(YamlTreeWalker::get_parser_calls(), &tree);
  EXPECT_EQ(YamlParser::CONTINUE_PARSING,
            parser.parse(yaml.data(), yaml.size()));
}

std::string writeModelYamlToString()
{
  return writeYamlToString(get_modeldata_nodes(), reinterpret_cast<uint8_t*>(&g_model));
}

void readModelYamlFromString(const std::string& yaml)
{
  readYamlFromString(get_modeldata_nodes(), reinterpret_cast<uint8_t*>(&g_model), yaml);
}

std::string writeRadioYamlToString()
{
  return writeYamlToString(get_radiodata_nodes(),
                           reinterpret_cast<uint8_t*>(&g_eeGeneral));
}

void readRadioYamlFromString(const std::string& yaml)
{
  readYamlFromString(get_radiodata_nodes(), reinterpret_cast<uint8_t*>(&g_eeGeneral), yaml);
}

uint32_t nextRand(uint32_t& state)
{
  state = state * 1664525UL + 1013904223UL;
  return state;
}

uint32_t nextRange(uint32_t& state, uint32_t limit)
{
  return limit ? nextRand(state) % limit : 0;
}

int32_t nextSignedRange(uint32_t& state, int32_t min, int32_t max)
{
  return min + static_cast<int32_t>(nextRange(state, static_cast<uint32_t>(max - min + 1)));
}

void setBoundedName(char* dest, size_t size, const char* prefix, uint32_t value)
{
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%s%lu", prefix,
           static_cast<unsigned long>(value));
  strncpy(dest, tmp, size);
  if (size > 0) {
    dest[size - 1] = '\0';
  }
}

bool hasKey(const std::vector<std::string>& keys, const char* key)
{
  return std::find(keys.begin(), keys.end(), key) != keys.end();
}

bool hasWritableOutput(const YamlNode* node)
{
  if (!node) {
    return false;
  }

  for (; node->type != YDT_NONE; ++node) {
    if (node->type == YDT_PADDING) {
      continue;
    }
    if (node->type == YDT_IDX) {
      continue;
    }
    if (node->type == YDT_CUSTOM && node->u._cust_attr.write == nullptr) {
      continue;
    }
    if ((node->type == YDT_ARRAY || node->type == YDT_UNION) &&
        !hasWritableOutput(node->u._array.child)) {
      continue;
    }
    return true;
  }
  return false;
}

bool isWritableTopLevelNode(const YamlNode& node)
{
  if (!node.tag || node.type == YDT_NONE || node.type == YDT_PADDING ||
      node.type == YDT_IDX ||
      (node.type == YDT_CUSTOM && node.u._cust_attr.write == nullptr)) {
    return false;
  }
  if (node.type == YDT_ARRAY || node.type == YDT_UNION) {
    return hasWritableOutput(node.u._array.child);
  }
  return true;
}

void expectAllWritableTopLevelKeysSeen(const char* rootName, const YamlNode* root,
                                       const CorpusStats& stats)
{
  ASSERT_EQ(YDT_ARRAY, root->type) << rootName;
  const YamlNode* node = root->u._array.child;
  for (; node->type != YDT_NONE; ++node) {
    if (!isWritableTopLevelNode(*node)) {
      continue;
    }
    EXPECT_TRUE(hasKey(stats.topLevelKeys, node->tag))
        << rootName << " missing top-level YAML key " << node->tag;
  }
}

std::string snippetAround(const std::string& value, size_t pos)
{
  const size_t begin = pos > 80 ? pos - 80 : 0;
  const size_t end = std::min(value.size(), pos + 80);
  return value.substr(begin, end - begin);
}

void expectYamlEqual(const char* rootName, uint32_t seed, const std::string& first,
                     const std::string& second)
{
  if (first == second) {
    return;
  }

  size_t pos = 0;
  const size_t limit = std::min(first.size(), second.size());
  while (pos < limit && first[pos] == second[pos]) {
    ++pos;
  }

  ADD_FAILURE() << rootName << " YAML roundtrip changed for seed=" << seed
                << " first_size=" << first.size()
                << " second_size=" << second.size() << " diff_offset=" << pos
                << " first_snippet=" << snippetAround(first, pos)
                << " second_snippet=" << snippetAround(second, pos);
}

void resetModelLikeYamlReader()
{
  memset(&g_model, 0, sizeof(g_model));
  for (int fm = 1; fm < MAX_FLIGHT_MODES; ++fm) {
    for (int gv = 0; gv < MAX_GVARS; ++gv) {
      g_model.flightModeData[fm].gvars[gv] = GVAR_MAX + 1;
    }
  }
  g_model.rfAlarms.warning = 45;
  g_model.rfAlarms.critical = 42;
#if defined(COLORLCD)
  g_model.resetScreenData();
#endif
}

void perturbCustomFunction(CustomFunctionData& cfn, uint32_t& state, uint8_t index)
{
  cfn.swtch = SWSRC_ON;
  cfn.func = FUNC_PLAY_SOUND;
  CFN_PARAM(&cfn) = nextRange(state, 16);
  CFN_ACTIVE(&cfn) = nextRange(state, 2);
  CFN_PLAY_REPEAT(&cfn) = 0;
  (void)index;
}

void perturbModelWithinWriterDomain(uint32_t seed)
{
  setModelDefaults(static_cast<uint8_t>(seed % 16));

  uint32_t state = seed;
  setBoundedName(g_model.header.name, LEN_MODEL_NAME, "Rt", nextRand(state) % 10000);
#if LEN_BITMAP_NAME > 0
  setBoundedName(g_model.header.bitmap, LEN_BITMAP_NAME, "Bm", nextRand(state) % 1000);
#endif
#if defined(STORAGE_MODELSLIST)
  setBoundedName(g_model.header.labels, LABELS_LENGTH, "L", nextRand(state) % 1000);
#endif
  for (uint8_t i = 0; i < NUM_MODULES; ++i) {
    g_model.header.modelId[i] = nextRange(state, 255);
  }

  g_model.telemetryProtocol = nextRange(state, 4);
  g_model.thrTrim = nextRange(state, 2);
  g_model.noGlobalFunctions = nextRange(state, 2);
  g_model.displayTrims = nextRange(state, 4);
  g_model.ignoreSensorIds = nextRange(state, 2);
  g_model.trimInc = nextSignedRange(state, -2, 2);
  g_model.disableThrottleWarning = nextRange(state, 2);
  g_model.displayChecklist = nextRange(state, 2);
  g_model.extendedLimits = nextRange(state, 2);
  g_model.extendedTrims = nextRange(state, 2);
  g_model.throttleReversed = nextRange(state, 2);
  g_model.enableCustomThrottleWarning = nextRange(state, 2);
  g_model.disableTelemetryWarning = nextRange(state, 2);
  g_model.showInstanceIds = nextRange(state, 2);
  g_model.checklistInteractive = nextRange(state, 2);
  g_model.customThrottleWarningPosition = nextSignedRange(state, -100, 100);
  g_model.beepANACenter = nextRange(state, 0xffff);

  for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
    g_model.timers[i].start = nextRange(state, 7200);
    g_model.timers[i].value = nextRange(state, 7200);
    g_model.timers[i].mode = nextRange(state, TMRMODE_COUNT);
    g_model.timers[i].countdownBeep = nextRange(state, 4);
    g_model.timers[i].minuteBeep = nextRange(state, 2);
    g_model.timers[i].persistent = nextRange(state, 4);
    g_model.timers[i].countdownStart = nextSignedRange(state, -2, 1);
    g_model.timers[i].showElapsed = nextRange(state, 2);
    g_model.timers[i].extraHaptic = nextRange(state, 2);
    g_model.timers[i].minuteBeepStart = nextRange(state, 60);
    setBoundedName(g_model.timers[i].name, LEN_TIMER_NAME, "T", i);
  }

  for (uint8_t i = 0; i < MAX_MIXERS; ++i) {
    MixData& mix = g_model.mixData[i];
    mix.destCh = i % MAX_OUTPUT_CHANNELS;
    mix.srcRaw = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
    mix.weight = nextSignedRange(state, -100, 100);
    mix.offset = nextSignedRange(state, -100, 100);
    mix.mltpx = nextRange(state, 3);
    mix.carryTrim = nextRange(state, 2);
    mix.mixWarn = nextRange(state, 4);
    mix.flightModes = nextRange(state, 1U << MAX_FLIGHT_MODES);
    mix.swtch = (i % 3 == 0) ? SWSRC_ON : SWSRC_NONE;
    mix.delayUp = nextRange(state, 40);
    mix.delayDown = nextRange(state, 40);
    mix.speedUp = nextRange(state, 40);
    mix.speedDown = nextRange(state, 40);
    mix.curve.type = nextRange(state, 2);
    mix.curve.value = 0;
    setBoundedName(mix.name, LEN_EXPOMIX_NAME, "M", i);
  }

  for (uint8_t i = 0; i < MAX_EXPOS; ++i) {
    ExpoData& expo = g_model.expoData[i];
    expo.mode = 1 + nextRange(state, 3);
    expo.scale = nextRange(state, 1024);
    expo.trimSource = 0;
    expo.srcRaw = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
    expo.weight = nextSignedRange(state, -100, 100);
    expo.offset = nextSignedRange(state, -100, 100);
    expo.swtch = (i % 4 == 0) ? SWSRC_ON : SWSRC_NONE;
    expo.curve.type = nextRange(state, 2);
    expo.curve.value = 0;
    expo.chn = i % MAX_INPUTS;
    expo.flightModes = nextRange(state, 1U << MAX_FLIGHT_MODES);
    setBoundedName(expo.name, LEN_EXPOMIX_NAME, "E", i);
  }

  for (uint8_t i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    LimitData& limit = g_model.limitData[i];
    limit.min = nextSignedRange(state, -100, -1);
    limit.max = nextSignedRange(state, 1, 100);
    limit.ppmCenter = 1500 + nextSignedRange(state, -100, 100);
    limit.offset = nextSignedRange(state, -100, 100);
    limit.symetrical = nextRange(state, 2);
    limit.revert = nextRange(state, 2);
    limit.curve = nextSignedRange(state, -10, 10);
    setBoundedName(limit.name, LEN_CHANNEL_NAME, "C", i);
  }

  for (uint8_t i = 0; i < MAX_CURVES; ++i) {
    g_model.curves[i].type = nextRange(state, 2);
    g_model.curves[i].smooth = nextRange(state, 2);
    g_model.curves[i].points = nextSignedRange(state, -3, 12);
    setBoundedName(g_model.curves[i].name, LEN_CURVE_NAME, "Cv", i);
  }
  for (uint16_t i = 0; i < MAX_CURVE_POINTS; ++i) {
    g_model.points[i] = nextSignedRange(state, -100, 100);
  }

  for (uint8_t i = 0; i < MAX_LOGICAL_SWITCHES; ++i) {
    LogicalSwitchData& logical = g_model.logicalSw[i];
    logical.func = LS_FUNC_VPOS;
    logical.v1 = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
    logical.v2 = nextSignedRange(state, -100, 100);
    logical.andsw = (i % 5 == 0) ? SWSRC_ON : SWSRC_NONE;
    logical.delay = nextRange(state, 30);
    logical.duration = nextRange(state, 30);
  }

  for (uint8_t i = 0; i < MAX_SPECIAL_FUNCTIONS; ++i) {
    perturbCustomFunction(g_model.customFn[i], state, i);
  }
  for (uint8_t i = 0; i < MAX_SWITCHES; ++i) {
    g_model.setSwitchWarning(i, 1 + nextRange(state, 3));
  }

  g_model.swashR.type = 1;
  g_model.swashR.value = 50 + nextRange(state, 51);
  g_model.swashR.collectiveSource = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
  g_model.swashR.aileronSource = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
  g_model.swashR.elevatorSource = MIXSRC_FIRST_STICK + nextRange(state, MAX_STICKS);
  g_model.swashR.collectiveWeight = nextSignedRange(state, -100, 100);
  g_model.swashR.aileronWeight = nextSignedRange(state, -100, 100);
  g_model.swashR.elevatorWeight = nextSignedRange(state, -100, 100);

  for (uint8_t fm = 0; fm < MAX_FLIGHT_MODES; ++fm) {
    FlightModeData& fmd = g_model.flightModeData[fm];
    setBoundedName(fmd.name, LEN_FLIGHT_MODE_NAME, "Fm", fm);
    fmd.swtch = fm == 0 ? SWSRC_NONE : SWSRC_ON;
    fmd.fadeIn = nextRange(state, 15);
    fmd.fadeOut = nextRange(state, 15);
    for (uint8_t i = 0; i < MAX_TRIMS; ++i) {
      fmd.trim[i].value = nextSignedRange(state, -100, 100);
      fmd.trim[i].mode = nextRange(state, 4);
    }
    for (uint8_t gv = 0; gv < MAX_GVARS; ++gv) {
      fmd.gvars[gv] = nextSignedRange(state, -100, 100);
    }
  }

  for (uint8_t i = 0; i < MAX_GVARS; ++i) {
    setBoundedName(g_model.gvars[i].name, LEN_GVAR_NAME, "Gv", i);
    g_model.gvars[i].min = 0;
    g_model.gvars[i].max = 200;
    g_model.gvars[i].popup = nextRange(state, 2);
    g_model.gvars[i].prec = nextRange(state, 2);
    g_model.gvars[i].unit = nextRange(state, 4);
  }

  g_model.varioData.source = 0;
  g_model.varioData.centerSilent = nextRange(state, 2);
  g_model.varioData.centerMax = nextSignedRange(state, -10, 10);
  g_model.varioData.centerMin = nextSignedRange(state, -10, 10);
  g_model.varioData.min = nextSignedRange(state, -100, -1);
  g_model.varioData.max = nextSignedRange(state, 1, 100);
  g_model.rssiSource = 0;
  g_model.rfAlarms.warning = 30 + nextRange(state, 30);
  g_model.rfAlarms.critical = 20 + nextRange(state, 20);

  for (uint8_t i = 0; i < MAX_BATTERY_MONITORS; ++i) {
    BatteryMonitorData& monitor = g_model.batteryMonitors[i];
    monitor.enabled = 1;
    monitor.batteryType = nextRange(state, BATTERY_TYPE_LAST + 1);
    monitor.capAlertEnabled = nextRange(state, 2);
    monitor.voltAlertEnabled = nextRange(state, 2);
    monitor.cellCount = 1 + nextRange(state, 6);
    monitor.capacity = 500 + nextRange(state, 5000);
    monitor.sourceIndex = nextSignedRange(state, -1, 10);
    monitor.currentIndex = nextSignedRange(state, -1, 10);
    monitor.selectedPackSlot = nextRange(state, MAX_BATTERY_PACKS);
    monitor.compatiblePackMask = nextRange(state, 0xffff);
  }

  g_model.thrTrimSw = nextRange(state, MAX_STICKS);
  g_model.potsWarnMode = nextRange(state, 4);
  g_model.jitterFilter = nextRange(state, 3);

  for (uint8_t i = 0; i < NUM_MODULES; ++i) {
    ModuleData& module = g_model.moduleData[i];
    module.type = MODULE_TYPE_PPM;
    module.channelsStart = i * 8;
    module.channelsCount = 0;
    module.failsafeMode = nextRange(state, 4);
    module.ppm.delay = nextSignedRange(state, -10, 10);
    module.ppm.pulsePol = nextRange(state, 2);
    module.ppm.outputType = nextRange(state, 2);
    module.ppm.frameLength = nextSignedRange(state, -10, 10);
  }
  for (uint8_t i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    g_model.failsafeChannels[i] = nextSignedRange(state, -1024, 1024);
  }
  g_model.trainerData.mode = 0;
  g_model.trainerData.channelsStart = 0;
  g_model.trainerData.channelsCount = 0;
  g_model.trainerData.frameLength = 0;
  g_model.trainerData.delay = nextSignedRange(state, -10, 10);
  g_model.trainerData.pulsePol = nextRange(state, 2);

#if MAX_SCRIPTS > 0
  for (uint8_t i = 0; i < MAX_SCRIPTS; ++i) {
    setBoundedName(g_model.scriptsData[i].file, LEN_SCRIPT_FILENAME, "sc", i);
    setBoundedName(g_model.scriptsData[i].name, LEN_SCRIPT_NAME, "Sn", i);
  }
#endif
  for (uint8_t i = 0; i < MAX_INPUTS; ++i) {
    setBoundedName(g_model.inputNames[i], LEN_INPUT_NAME, "I", i);
  }
  g_model.potsWarnEnabled = 0xffff;
  for (uint8_t i = 0; i < MAX_POTS; ++i) {
    g_model.potsWarnPosition[i] = nextSignedRange(state, -100, 100);
  }

  for (uint8_t i = 0; i < MAX_TELEMETRY_SENSORS; ++i) {
    TelemetrySensor& sensor = g_model.telemetrySensors[i];
    sensor.id = 0x100 + i;
    sensor.instance = i % 32;
    setBoundedName(sensor.label, TELEM_LABEL_LEN, "Ts", i);
    sensor.subId = i;
    sensor.type = TELEM_TYPE_CUSTOM;
    sensor.unit = 0;
    sensor.prec = nextRange(state, 3);
    sensor.autoOffset = nextRange(state, 2);
    sensor.filter = nextRange(state, 2);
    sensor.logs = nextRange(state, 2);
    sensor.persistent = nextRange(state, 2);
    sensor.onlyPositive = nextRange(state, 2);
    sensor.custom.ratio = 100 + nextRange(state, 1000);
    sensor.custom.offset = nextSignedRange(state, -100, 100);
  }

#if defined(COLORLCD)
  for (uint8_t i = 0; i < MAX_CUSTOM_SCREENS; ++i) {
    g_model.setScreenLayoutId(i, (i % 2) ? "Layout1x1" : "Layout2P1");
  }
#endif
  g_model.view = nextRange(state, MAX_CUSTOM_SCREENS);
  setBoundedName(g_model.modelRegistrationID, PXX2_LEN_REGISTRATION_ID, "Rg", seed);

#if defined(FUNCTION_SWITCHES)
  for (uint8_t i = 0; i < NUM_FUNCTIONS_SWITCHES; ++i) {
    setBoundedName(g_model.customSwitches[i].name, LEN_SWITCH_NAME, "Fs", i);
    g_model.customSwitches[i].type = SWITCH_2POS;
    g_model.customSwitches[i].group = i % NUM_FUNCTIONS_GROUPS;
    g_model.customSwitches[i].start = FS_START_PREVIOUS;
    g_model.customSwitches[i].state = nextRange(state, 2);
  }
  g_model.cfsGroupOn = 0xff;
#endif

  g_model.usbJoystickExtMode = nextRange(state, 2);
  g_model.usbJoystickIfMode = nextRange(state, 3);
  g_model.usbJoystickCircularCut = nextRange(state, 16);
  for (uint8_t i = 0; i < USBJ_MAX_JOYSTICK_CHANNELS; ++i) {
    g_model.usbJoystickCh[i].mode = nextRange(state, 4);
    g_model.usbJoystickCh[i].inversion = nextRange(state, 2);
    g_model.usbJoystickCh[i].param = nextRange(state, 4);
    g_model.usbJoystickCh[i].btn_num = i % 31;
    g_model.usbJoystickCh[i].switch_npos = nextRange(state, 4);
  }

#if defined(COLORLCD)
  g_model.radioThemesDisabled = nextRange(state, 3);
#endif
  g_model.radioGFDisabled = nextRange(state, 3);
  g_model.radioTrainerDisabled = nextRange(state, 3);
  g_model.modelHeliDisabled = nextRange(state, 3);
  g_model.modelFMDisabled = nextRange(state, 3);
  g_model.modelCurvesDisabled = nextRange(state, 3);
  g_model.modelGVDisabled = nextRange(state, 3);
  g_model.modelLSDisabled = nextRange(state, 3);
  g_model.modelSFDisabled = nextRange(state, 3);
  g_model.modelCustomScriptsDisabled = nextRange(state, 3);
  g_model.modelTelemetryDisabled = nextRange(state, 3);
}

void perturbRadioWithinWriterDomain(uint32_t seed)
{
  generalDefault();

  uint32_t state = 0x9e3779b9UL ^ seed;
  g_eeGeneral.manuallyEdited = 1;
  g_eeGeneral.timezoneMinutes = nextSignedRange(state, -3, 3);
  g_eeGeneral.ppmunit = nextRange(state, 3);
  g_eeGeneral.vBatWarn = 65 + nextRange(state, 40);
  g_eeGeneral.vBatCrit = 30 + nextRange(state, 30);
  g_eeGeneral.txVoltageCalibration = nextSignedRange(state, -20, 20);
  g_eeGeneral.backlightMode = nextRange(state, e_backlight_mode_on + 1);
  g_eeGeneral.antennaMode = nextSignedRange(state, 0, 2);
  g_eeGeneral.disableRtcWarning = nextRange(state, 2);
  g_eeGeneral.keysBacklight = nextRange(state, 2);
  g_eeGeneral.dontPlayHello = nextRange(state, 2);
  g_eeGeneral.internalModule = MODULE_TYPE_PPM;
  g_eeGeneral.view = nextRange(state, 4);
  g_eeGeneral.fai = nextRange(state, 2);
  g_eeGeneral.beepMode = nextSignedRange(state, -2, 1);
  g_eeGeneral.alarmsFlash = nextRange(state, 2);
  g_eeGeneral.disableMemoryWarning = nextRange(state, 2);
  g_eeGeneral.disableAlarmWarning = nextRange(state, 2);
  g_eeGeneral.stickMode = nextRange(state, 4);
  g_eeGeneral.timezone = nextSignedRange(state, -12, 12);
  g_eeGeneral.adjustRTC = nextRange(state, 2);
  g_eeGeneral.inactivityTimer = nextRange(state, 60);
  g_eeGeneral.internalModuleBaudrate = nextRange(state, 4);
  g_eeGeneral.splashMode = nextSignedRange(state, -4, 3);
  g_eeGeneral.hapticMode = nextSignedRange(state, -2, 1);
  g_eeGeneral.switchesDelay = nextSignedRange(state, 0, 20);
  g_eeGeneral.lightAutoOff = 1 + nextRange(state, 30);
  g_eeGeneral.templateSetup = nextRange(state, 24);
  g_eeGeneral.PPM_Multiplier = nextSignedRange(state, -10, 10);
  g_eeGeneral.hapticLength = nextSignedRange(state, -2, 2);
  g_eeGeneral.beepLength = nextSignedRange(state, -2, 2);
  g_eeGeneral.hapticStrength = nextSignedRange(state, -2, 2);
  g_eeGeneral.gpsFormat = nextRange(state, 2);
  g_eeGeneral.audioMuteEnable = nextRange(state, 2);
  g_eeGeneral.speakerPitch = 50 + nextRange(state, 51);
  g_eeGeneral.speakerVolume = nextSignedRange(state, -10, 10);
  g_eeGeneral.vBatMin = nextSignedRange(state, -20, 20);
  g_eeGeneral.vBatMax = nextSignedRange(state, -20, 20);
  g_eeGeneral.backlightBright = 20 + nextRange(state, 81);
  g_eeGeneral.blOffBright = nextRange(state, 20);
  g_eeGeneral.globalTimer = nextRand(state);
  g_eeGeneral.bluetoothBaudrate = nextRange(state, 8);
  g_eeGeneral.bluetoothMode = nextRange(state, BLUETOOTH_MAX + 1);
  g_eeGeneral.countryCode = nextRange(state, 4);
  g_eeGeneral.pwrOnSpeed = nextSignedRange(state, 0, 3);
  g_eeGeneral.pwrOffSpeed = nextSignedRange(state, 0, 3);
  g_eeGeneral.noJitterFilter = nextRange(state, 2);
  g_eeGeneral.imperial = nextRange(state, 2);
  g_eeGeneral.disableRssiPoweroffAlarm = nextRange(state, 2);
  g_eeGeneral.USBMode = nextRange(state, 4);
  g_eeGeneral.jackMode = nextRange(state, 4);
  g_eeGeneral.ttsLanguage[0] = 'a' + nextRange(state, 26);
  g_eeGeneral.ttsLanguage[1] = 'a' + nextRange(state, 26);
  g_eeGeneral.uiLanguage[0] = 'a' + nextRange(state, 26);
  g_eeGeneral.uiLanguage[1] = 'a' + nextRange(state, 26);
  g_eeGeneral.beepVolume = nextSignedRange(state, -2, 2);
  g_eeGeneral.wavVolume = nextSignedRange(state, -2, 2);
  g_eeGeneral.varioVolume = nextSignedRange(state, -2, 2);
  g_eeGeneral.backgroundVolume = nextSignedRange(state, -2, 2);
  g_eeGeneral.varioPitch = nextSignedRange(state, -10, 10);
  g_eeGeneral.varioRange = nextSignedRange(state, -10, 10);
  g_eeGeneral.varioRepeat = nextSignedRange(state, -10, 10);

  for (uint8_t i = 0; i < MAX_CALIB_ANALOG_INPUTS; ++i) {
    g_eeGeneral.calib[i].mid = 1500 + nextSignedRange(state, -100, 100);
    g_eeGeneral.calib[i].spanNeg = 500 + nextRange(state, 200);
    g_eeGeneral.calib[i].spanPos = 500 + nextRange(state, 200);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    g_eeGeneral.trainer.calib[i] = nextSignedRange(state, -100, 100);
    g_eeGeneral.trainer.mix[i].srcChn = i;
    g_eeGeneral.trainer.mix[i].mode = nextRange(state, 3);
    g_eeGeneral.trainer.mix[i].studWeight = nextSignedRange(state, -100, 100);
  }
  for (uint8_t i = 0; i < MAX_BATTERY_PACKS; ++i) {
    g_eeGeneral.batteryPacks[i].active = 1;
    g_eeGeneral.batteryPacks[i].batteryType = nextRange(state, BATTERY_TYPE_LAST + 1);
    g_eeGeneral.batteryPacks[i].cellCount = 1 + nextRange(state, 6);
    g_eeGeneral.batteryPacks[i].capacity = 500 + nextRange(state, 5000);
  }
  for (uint8_t i = 0; i < MAX_SPECIAL_FUNCTIONS; ++i) {
    perturbCustomFunction(g_eeGeneral.customFn[i], state, i);
  }

  g_eeGeneral.serialPort = nextRand(state);
  g_eeGeneral.potsConfig = adcGetDefaultPotsConfig();
  for (uint8_t i = 0; i < MAX_FLEX_SWITCHES && i < MAX_POTS; ++i) {
    g_eeGeneral.potsConfig |= static_cast<potconfig_t>(FLEX_SWITCH)
                              << (POT_CFG_BITS * i);
  }
  for (uint8_t i = 0; i < MAX_STICKS; ++i) {
    char name[8];
    snprintf(name, sizeof(name), "St%u", i);
    analogSetCustomLabel(ADC_INPUT_MAIN, i, name, strlen(name));
  }
  for (uint8_t i = 0; i < MAX_POTS; ++i) {
    char name[8];
    snprintf(name, sizeof(name), "Pt%u", i);
    analogSetCustomLabel(ADC_INPUT_FLEX, i, name, strlen(name));
  }
  for (uint8_t i = 0; i < MAX_SWITCHES; ++i) {
    setBoundedName(g_eeGeneral.switchConfig[i].name, LEN_SWITCH_NAME, "Sw", i);
  }
  for (uint8_t i = 0; i < MAX_FLEX_SWITCHES; ++i) {
    if (adcGetMaxInputs(ADC_INPUT_FLEX) > 0) {
      switchConfigFlex_raw(i, i % adcGetMaxInputs(ADC_INPUT_FLEX));
    }
  }

  setBoundedName(g_eeGeneral.currModelFilename, LEN_MODEL_FILENAME + 1, "model", seed);
  setBoundedName(g_eeGeneral.bluetoothName, LEN_BLUETOOTH_NAME, "Bt", seed);
  setBoundedName(g_eeGeneral.ownerRegistrationID, PXX2_LEN_REGISTRATION_ID, "Ow", seed);
#if defined(COLORLCD)
  setBoundedName(g_eeGeneral.selectedTheme, SELECTED_THEME_NAME_LEN, "Th", seed);
#endif
  g_eeGeneral.rotEncMode = nextRange(state, 4);
#if defined(STM32F2) || defined(STM32F4)
  g_eeGeneral.uartSampleMode = nextRange(state, 2);
#endif
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = nextRange(state, 5);
#endif
#if defined(IMU)
  g_eeGeneral.imuMax = nextSignedRange(state, -100, 100);
  g_eeGeneral.imuOffset = nextSignedRange(state, -100, 100);
#endif
  g_eeGeneral.backlightSrc = 0;
  g_eeGeneral.volumeSrc = 0;
  g_eeGeneral.radioGFDisabled = nextRange(state, 2);
  g_eeGeneral.radioTrainerDisabled = nextRange(state, 2);
  g_eeGeneral.modelHeliDisabled = nextRange(state, 2);
  g_eeGeneral.modelFMDisabled = nextRange(state, 2);
  g_eeGeneral.modelCurvesDisabled = nextRange(state, 2);
  g_eeGeneral.modelGVDisabled = nextRange(state, 2);
  g_eeGeneral.modelLSDisabled = nextRange(state, 2);
  g_eeGeneral.modelSFDisabled = nextRange(state, 2);
  g_eeGeneral.modelCustomScriptsDisabled = nextRange(state, 2);
  g_eeGeneral.modelTelemetryDisabled = nextRange(state, 2);
  g_eeGeneral.disableTrainerPoweroffAlarm = nextRange(state, 2);
  g_eeGeneral.disablePwrOnOffHaptic = nextRange(state, 2);
  g_eeGeneral.modelQuickSelect = nextRange(state, 2);
#if defined(COLORLCD)
  g_eeGeneral.labelSingleSelect = nextRange(state, 2);
  g_eeGeneral.labelMultiMode = nextRange(state, 2);
  g_eeGeneral.favMultiMode = nextRange(state, 2);
  g_eeGeneral.modelSelectLayout = nextRange(state, 3);
  g_eeGeneral.radioThemesDisabled = nextRange(state, 2);
#endif
  g_eeGeneral.oneLogPerDay = nextRange(state, 2);
  g_eeGeneral.pwrOffIfInactive = nextRange(state, 60);
#if defined(COLORLCD)
  for (uint8_t i = 0; i < MAX_KEY_SHORTCUTS; ++i) {
    g_eeGeneral.keyShortcuts[i].shortcut = nextRange(state, 16);
  }
  for (uint8_t i = 0; i < MAX_QM_FAVORITES; ++i) {
    g_eeGeneral.qmFavorites[i].shortcut = nextRange(state, 16);
  }
  for (uint8_t i = 0; i < MAX_TOPBAR_ZONES; ++i) {
    g_eeGeneral.topbarWidgetWidth[i] = 20 + nextRange(state, 80);
  }
#endif
}

}  // namespace

TEST(YamlModelRoundtrip, GeneratedModelsSerializeParseSerializeIdentically)
{
  CorpusStats stats;
  for (uint32_t seed = 1; seed <= kVariantCount; ++seed) {
    perturbModelWithinWriterDomain(seed);
    const std::string first = writeModelYamlToString();
    stats.observe(first);

    resetModelLikeYamlReader();
    readModelYamlFromString(first);
    const std::string second = writeModelYamlToString();

    resetModelLikeYamlReader();
    readModelYamlFromString(second);
    const std::string third = writeModelYamlToString();

    expectYamlEqual("ModelData", seed, second, third);
  }

  EXPECT_GE(stats.hashes.size(), 100u);
  expectAllWritableTopLevelKeysSeen("ModelData", get_modeldata_nodes(), stats);
}

TEST(YamlModelRoundtrip, ParserIgnoresSchemaLocatorComments)
{
  perturbModelWithinWriterDomain(1);
  const std::string generated = writeModelYamlToString();

  resetModelLikeYamlReader();
  readModelYamlFromString(generated);
  const std::string expected = writeModelYamlToString();

  const std::string withPreamble =
      "# yaml-language-server: $schema=https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas/v1/schema-versions/1.0/tx16s/model.schema.json\r\n"
      "# edge16-schema-format: 1\r\n"
      "# edge16-schema-version: 1.0\r\n"
      "# edge16-firmware-commit: 00000000\r\n"
      "# edge16-target: tx16s\r\n"
      "# edge16-yaml-root: model\r\n" +
      generated;

  resetModelLikeYamlReader();
  readModelYamlFromString(withPreamble);
  const std::string actual = writeModelYamlToString();

  expectYamlEqual("ModelData comments", 1, expected, actual);
}

TEST(YamlRadioRoundtrip, GeneratedRadioSetupsSerializeParseSerializeIdentically)
{
  CorpusStats stats;
  for (uint32_t seed = 1; seed <= kVariantCount; ++seed) {
    perturbRadioWithinWriterDomain(seed);
    const std::string first = writeRadioYamlToString();
    stats.observe(first);

    generalDefault();
    readRadioYamlFromString(first);
    const std::string second = writeRadioYamlToString();

    generalDefault();
    readRadioYamlFromString(second);
    const std::string third = writeRadioYamlToString();

    expectYamlEqual("RadioData", seed, second, third);
  }

  EXPECT_GE(stats.hashes.size(), 100u);
  expectAllWritableTopLevelKeysSeen("RadioData", get_radiodata_nodes(), stats);
}
