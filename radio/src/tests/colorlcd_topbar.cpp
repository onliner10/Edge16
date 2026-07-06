/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/th9x
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

bool replaceDefaultLayoutPreservesRadioTopbarForTest();
bool explicitTopbarRemovalStillClearsZoneForTest();

// The topbar widget configuration is radio-global. Each of these helpers
// mutates that shared state and the UI singletons, so run each in a forked
// child to keep tests isolated (mirrors colorlcd_layout.cpp).
static void expectChildSucceeds(bool (*body)())
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(body() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

// Loading/creating a model with no custom screen data drives
// replaceDefaultLayout(); it must not erase the radio-global top bar.
TEST(ColorTopBar, DefaultLayoutRebuildPreservesRadioTopbar)
{
  expectChildSucceeds(replaceDefaultLayoutPreservesRadioTopbarForTest);
}

// Guard against over-fixing: an explicit widget removal must still clear the
// zone.
TEST(ColorTopBar, ExplicitRemovalStillClearsZone)
{
  expectChildSucceeds(explicitTopbarRemovalStillClearsZoneForTest);
}

#endif
