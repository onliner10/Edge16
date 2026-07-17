/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
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

#if defined(COLORLCD) && defined(SIMU) && !GTEST_OS_WINDOWS

#include <sys/wait.h>
#include <unistd.h>

bool presetNewFunctionDataIsEnabledAndInertForTest();
bool presetFunctionDataDoesNotPlayWithEmptyTrackNameForTest();
bool firstTimeFunctionPickStaysEnabledForTest();
bool functionChangeOnExistingSlotStillRequiresReenableForTest();

// A brand new Special/Global Function slot (used by FunctionsPage::newSF and
// the "Insert" action, shared by both Special and Global Functions) must
// come out enabled by default, but with a genuinely inert function until the
// pilot picks a real one -- not the raw zero-default FUNC_OVERRIDE_CHANNEL,
// which is an *active* channel-1 override, not a placeholder.
TEST(ColorSpecialFunctions, NewSlotIsEnabledAndInert)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(presetNewFunctionDataIsEnabledAndInertForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorSpecialFunctions, InertDefaultFunctionDoesNotPlay)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(presetFunctionDataDoesNotPlayWithEmptyTrackNameForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

// Reproduces "create a new Special Function (trigger + Play Sound), exit"
// end-to-end at the data level: CFN_RESET() (fired when the pilot picks a
// function) must not silently re-disable a slot that was empty when its
// editor opened.
TEST(ColorSpecialFunctions, FirstTimeFunctionPickStaysEnabled)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(firstTimeFunctionPickStaysEnabledForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

// Repurposing an already-configured, already-enabled function must still
// require an explicit re-enable.
TEST(ColorSpecialFunctions, FunctionChangeOnExistingSlotStillRequiresReenable)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(functionChangeOnExistingSlotStillRequiresReenableForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

#endif
