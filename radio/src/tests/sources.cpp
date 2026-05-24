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

#include "storage/yaml/yaml_tree_walker.h"
#include "storage/yaml/yaml_parser.h"
#include "storage/yaml/yaml_datastructs.h"
#include "storage/yaml/yaml_bits.h"

static const char _radio_config[] =
    "potsConfig: \n"
    "   P1:\n"
    "      type: without_detent\n"
    "      name: \"\"\n"
    "   P2:\n"
    "      type: multipos_switch\n"
    "      name: \"\"\n"
    "   P3:\n"
    "      type: with_detent\n"
    "      name: \"\"\n"
    "   SL1:\n"
    "      type: slider\n"
    "      name: \"\"\n"
    "   SL2:\n"
    "      type: slider\n"
    "      name: \"\"\n"
    "switchConfig: \n"
    "   SA:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SB:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SC:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SD:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SE:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SF:\n"
    "      type: 2pos\n"
    "      name: \"\"\n"
    "   SG:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SH:\n"
    "      type: toggle\n"
    "      name: \"\"\n";

static void loadRadioYamlStr(const char* str)
{
  YamlTreeWalker tree;
  tree.reset(get_radiodata_nodes(), (uint8_t*)&g_eeGeneral);

  YamlParser yp;
  yp.init(YamlTreeWalker::get_parser_calls(), &tree);

  size_t len = strlen(str);
  yp.parse(str, len);
}

// Locks persisted MixSources IDs. Values were verified against main when
// MIXSRC_MODEL_ARMED was introduced; update only with an intentional storage
// or Lua API migration.
TEST(Sources, MixSourceIdsAreStable)
{
  struct ExpectedSourceId {
    int actual;
    int expected;
    const char* name;
  };

#if defined(RADIO_TX16SMK3)
  const ExpectedSourceId expected[] = {
      {MIXSRC_FIRST_INPUT, 1, "MIXSRC_FIRST_INPUT"},
      {MIXSRC_LAST_INPUT, 32, "MIXSRC_LAST_INPUT"},
      {MIXSRC_FIRST_LUA, 33, "MIXSRC_FIRST_LUA"},
      {MIXSRC_LAST_LUA, 86, "MIXSRC_LAST_LUA"},
      {MIXSRC_FIRST_STICK, 87, "MIXSRC_FIRST_STICK"},
      {MIXSRC_LAST_STICK, 90, "MIXSRC_LAST_STICK"},
      {MIXSRC_FIRST_POT, 91, "MIXSRC_FIRST_POT"},
      {MIXSRC_LAST_POT, 106, "MIXSRC_LAST_POT"},
      {MIXSRC_TILT_X, 107, "MIXSRC_TILT_X"},
      {MIXSRC_TILT_Y, 108, "MIXSRC_TILT_Y"},
      {MIXSRC_FIRST_SPACEMOUSE, 109, "MIXSRC_FIRST_SPACEMOUSE"},
      {MIXSRC_LAST_SPACEMOUSE, 114, "MIXSRC_LAST_SPACEMOUSE"},
      {MIXSRC_MIN, 115, "MIXSRC_MIN"},
      {MIXSRC_MAX, 116, "MIXSRC_MAX"},
      {MIXSRC_LIGHT, 117, "MIXSRC_LIGHT"},
      {MIXSRC_FIRST_HELI, 118, "MIXSRC_FIRST_HELI"},
      {MIXSRC_LAST_HELI, 120, "MIXSRC_LAST_HELI"},
      {MIXSRC_FIRST_TRIM, 121, "MIXSRC_FIRST_TRIM"},
      {MIXSRC_LAST_TRIM, 128, "MIXSRC_LAST_TRIM"},
      {MIXSRC_FIRST_SWITCH, 129, "MIXSRC_FIRST_SWITCH"},
      {MIXSRC_LAST_SWITCH, 148, "MIXSRC_LAST_SWITCH"},
      {MIXSRC_FIRST_CUSTOMSWITCH_GROUP, 149,
       "MIXSRC_FIRST_CUSTOMSWITCH_GROUP"},
      {MIXSRC_LAST_CUSTOMSWITCH_GROUP, 151,
       "MIXSRC_LAST_CUSTOMSWITCH_GROUP"},
      {MIXSRC_FIRST_LOGICAL_SWITCH, 152, "MIXSRC_FIRST_LOGICAL_SWITCH"},
      {MIXSRC_LAST_LOGICAL_SWITCH, 215, "MIXSRC_LAST_LOGICAL_SWITCH"},
      {MIXSRC_FIRST_TRAINER, 216, "MIXSRC_FIRST_TRAINER"},
      {MIXSRC_LAST_TRAINER, 231, "MIXSRC_LAST_TRAINER"},
      {MIXSRC_FIRST_CH, 232, "MIXSRC_FIRST_CH"},
      {MIXSRC_LAST_CH, 263, "MIXSRC_LAST_CH"},
      {MIXSRC_FIRST_GVAR, 264, "MIXSRC_FIRST_GVAR"},
      {MIXSRC_LAST_GVAR, 278, "MIXSRC_LAST_GVAR"},
      {MIXSRC_TX_VOLTAGE, 279, "MIXSRC_TX_VOLTAGE"},
      {MIXSRC_TX_TIME, 280, "MIXSRC_TX_TIME"},
      {MIXSRC_TX_GPS, 281, "MIXSRC_TX_GPS"},
      {MIXSRC_FIRST_TIMER, 282, "MIXSRC_FIRST_TIMER"},
      {MIXSRC_LAST_TIMER, 284, "MIXSRC_LAST_TIMER"},
      {MIXSRC_FIRST_TELEM, 285, "MIXSRC_FIRST_TELEM"},
      {MIXSRC_LAST_TELEM, 581, "MIXSRC_LAST_TELEM"},
      {MIXSRC_INVERT, 582, "MIXSRC_INVERT"},
      {MIXSRC_VALUE, 583, "MIXSRC_VALUE"},
      {MIXSRC_MODEL_ARMED, 584, "MIXSRC_MODEL_ARMED"},
  };
#elif defined(RADIO_TX16S)
  const ExpectedSourceId expected[] = {
      {MIXSRC_FIRST_INPUT, 1, "MIXSRC_FIRST_INPUT"},
      {MIXSRC_LAST_INPUT, 32, "MIXSRC_LAST_INPUT"},
      {MIXSRC_FIRST_LUA, 33, "MIXSRC_FIRST_LUA"},
      {MIXSRC_LAST_LUA, 86, "MIXSRC_LAST_LUA"},
      {MIXSRC_FIRST_STICK, 87, "MIXSRC_FIRST_STICK"},
      {MIXSRC_LAST_STICK, 90, "MIXSRC_LAST_STICK"},
      {MIXSRC_FIRST_POT, 91, "MIXSRC_FIRST_POT"},
      {MIXSRC_LAST_POT, 106, "MIXSRC_LAST_POT"},
      {MIXSRC_TILT_X, 107, "MIXSRC_TILT_X"},
      {MIXSRC_TILT_Y, 108, "MIXSRC_TILT_Y"},
      {MIXSRC_FIRST_SPACEMOUSE, 109, "MIXSRC_FIRST_SPACEMOUSE"},
      {MIXSRC_LAST_SPACEMOUSE, 114, "MIXSRC_LAST_SPACEMOUSE"},
      {MIXSRC_MIN, 115, "MIXSRC_MIN"},
      {MIXSRC_MAX, 116, "MIXSRC_MAX"},
      {MIXSRC_FIRST_HELI, 117, "MIXSRC_FIRST_HELI"},
      {MIXSRC_LAST_HELI, 119, "MIXSRC_LAST_HELI"},
      {MIXSRC_FIRST_TRIM, 120, "MIXSRC_FIRST_TRIM"},
      {MIXSRC_LAST_TRIM, 125, "MIXSRC_LAST_TRIM"},
      {MIXSRC_FIRST_SWITCH, 126, "MIXSRC_FIRST_SWITCH"},
      {MIXSRC_LAST_SWITCH, 145, "MIXSRC_LAST_SWITCH"},
      {MIXSRC_FIRST_LOGICAL_SWITCH, 146, "MIXSRC_FIRST_LOGICAL_SWITCH"},
      {MIXSRC_LAST_LOGICAL_SWITCH, 209, "MIXSRC_LAST_LOGICAL_SWITCH"},
      {MIXSRC_FIRST_TRAINER, 210, "MIXSRC_FIRST_TRAINER"},
      {MIXSRC_LAST_TRAINER, 225, "MIXSRC_LAST_TRAINER"},
      {MIXSRC_FIRST_CH, 226, "MIXSRC_FIRST_CH"},
      {MIXSRC_LAST_CH, 257, "MIXSRC_LAST_CH"},
      {MIXSRC_FIRST_GVAR, 258, "MIXSRC_FIRST_GVAR"},
      {MIXSRC_LAST_GVAR, 266, "MIXSRC_LAST_GVAR"},
      {MIXSRC_TX_VOLTAGE, 267, "MIXSRC_TX_VOLTAGE"},
      {MIXSRC_TX_TIME, 268, "MIXSRC_TX_TIME"},
      {MIXSRC_TX_GPS, 269, "MIXSRC_TX_GPS"},
      {MIXSRC_FIRST_TIMER, 270, "MIXSRC_FIRST_TIMER"},
      {MIXSRC_LAST_TIMER, 272, "MIXSRC_LAST_TIMER"},
      {MIXSRC_FIRST_TELEM, 273, "MIXSRC_FIRST_TELEM"},
      {MIXSRC_LAST_TELEM, 452, "MIXSRC_LAST_TELEM"},
      {MIXSRC_INVERT, 453, "MIXSRC_INVERT"},
      {MIXSRC_VALUE, 454, "MIXSRC_VALUE"},
      {MIXSRC_MODEL_ARMED, 455, "MIXSRC_MODEL_ARMED"},
  };
#else
  GTEST_SKIP() << "Mix source ABI lock covers TX16S targets only";
#endif

  for (const auto& entry : expected) {
    EXPECT_EQ(entry.expected, entry.actual) << entry.name;
  }
}

TEST(Sources, getSourceString)
{
  loadRadioYamlStr(_radio_config);

#if defined(IMU)
  EXPECT_STREQ(getSourceString(MIXSRC_TILT_X), "TltX");
  EXPECT_STREQ(getSourceString(MIXSRC_TILT_Y), "TltY");
#endif

#if defined(PCBHORUS)
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_A), "smA");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_B), "smB");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_C), "smC");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_D), "smD");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_E), "smE");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_F), "smF");
#endif

  EXPECT_STREQ(getSourceString(MIXSRC_MODEL_ARMED), "Armed");
  EXPECT_STREQ(getSourceString(MIXSRC_MIN), STR_MENU_MIN);
  EXPECT_STREQ(getSourceString(MIXSRC_MAX), STR_MENU_MAX);

#if defined(HELI)
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI), "CYC1");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI + 1), "CYC2");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI + 2), "CYC3");
#else
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI), "[C1]");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI + 1), "[C2]");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_HELI + 2), "[C3]");
#endif
#if defined(SURFACE_RADIO)
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_TRIM), CHAR_TRIM "ST");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_TRIM + 1), CHAR_TRIM "TH");
#else
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_TRIM), CHAR_TRIM "Rud");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_TRIM + 1), CHAR_TRIM "Ele");
  EXPECT_STREQ(getSourceString(MIXSRC_FIRST_TRIM + 2), CHAR_TRIM "Thr");
#endif
}
