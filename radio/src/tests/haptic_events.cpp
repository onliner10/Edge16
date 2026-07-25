/*
 * Copyright (C) EdgeTX
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

// hapticQueue::event() dispatches on the AU_* ordinal. Its middle branch used
// to be bounded by AU_MIX_WARNING_3 -- whatever happened to be the last event
// at the time it was written -- so every AU_* appended afterwards fell into a
// silent gap between that bound and the special-sound palette. That gap had
// swallowed AU_TIMER1/2/3_ELAPSED and, most seriously, AU_MODEL_ARMED /
// AU_MODEL_DISARMED: arming a model, the moment the props can spin, could not
// buzz at ANY hapticMode setting.
//
// These tests pin the property rather than the bound: every event below the
// selectable special-sound palette must produce a buzz when haptics are fully
// enabled, so the next AU_* appended to the enum cannot silently reopen the
// gap.

#include "gtests.h"

#include "haptic.h"

namespace
{

// haptic.play(..., PLAY_NOW) sets buzzTimeLeft immediately, so busy() is the
// observable "a buzz was issued".
bool buzzed(uint8_t event)
{
  haptic.play(0, 0, PLAY_NOW);  // drain: start from a known state
  while (haptic.busy()) haptic.heartbeat();
  haptic.event(event);
  return haptic.busy();
}

}  // namespace

TEST(HapticEvents, ArmingBuzzes)
{
  const uint8_t saved = g_eeGeneral.hapticMode;
  g_eeGeneral.hapticMode = e_mode_all;

  EXPECT_TRUE(buzzed(AU_MODEL_ARMED))
      << "arming the model produced no haptic feedback";
  EXPECT_TRUE(buzzed(AU_MODEL_DISARMED))
      << "disarming the model produced no haptic feedback";

  g_eeGeneral.hapticMode = saved;
}

TEST(HapticEvents, TimerElapsedBuzzes)
{
  const uint8_t saved = g_eeGeneral.hapticMode;
  g_eeGeneral.hapticMode = e_mode_all;

  EXPECT_TRUE(buzzed(AU_TIMER1_ELAPSED));
  EXPECT_TRUE(buzzed(AU_TIMER2_ELAPSED));
  EXPECT_TRUE(buzzed(AU_TIMER3_ELAPSED));

  g_eeGeneral.hapticMode = saved;
}

// The general property: no silent gap anywhere below the special-sound
// palette. Guards against the next append to AU_SOUNDS reopening the hole.
TEST(HapticEvents, NoSilentGapBelowSpecialSounds)
{
  const uint8_t saved = g_eeGeneral.hapticMode;
  g_eeGeneral.hapticMode = e_mode_all;

  for (uint8_t e = 0; e < AU_SPECIAL_SOUND_FIRST; e++) {
    EXPECT_TRUE(buzzed(e)) << "AU_* ordinal " << int(e)
                           << " falls in a haptic dead zone";
  }

  g_eeGeneral.hapticMode = saved;
}
