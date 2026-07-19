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

bool modelSelectMissingImageLoadReportsWorkForTest();
bool modelButtonClickHandlerMayDeleteButtonForTest();
bool chosenModelNameSurvivesSimulatedTemplateLoadForTest();
bool unchosenModelNameLeavesTemplateNameUntouchedForTest();
bool modelPressOnUnfocusedModelOnlyFocusesForTest();
bool modelPressOnFocusedModelOpensMenuWhenQuickSelectDisabledForTest();
bool modelPressOnFocusedModelQuickSelectsWhenEnabledForTest();
bool buildDuplicateModelNameDiffersFromSourceForTest();
bool buildDuplicateModelNameSkipsTakenSuffixForTest();
bool buildDuplicateModelNameTruncatesBaseToFitForTest();

TEST(ColorModelSelect, MissingThumbnailLoadCountsAsUiWork)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    _exit(modelSelectMissingImageLoadReportsWorkForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorModelSelect, ClickHandlerMayDeleteModelButton)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(modelButtonClickHandlerMayDeleteButtonForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

// A name typed in the new-model name prompt must survive template loading
// (not just apply to Blank Model), because loadModel() overwrites
// g_model.header.name wholesale with the template's own stored name.
TEST(ColorModelSelect, TypedNameSurvivesTemplateLoad)
{
  EXPECT_TRUE(chosenModelNameSurvivesSimulatedTemplateLoadForTest());
}

TEST(ColorModelSelect, UnappliedNameLeavesTemplateNameUntouched)
{
  EXPECT_TRUE(unchosenModelNameLeavesTemplateNameUntouchedForTest());
}

// Safety regression: a single tap on a model card must never force-load it
// with no confirmation. A NOT-yet-focused model only becomes focused --
// regardless of the quick select setting -- it never loads.
TEST(ColorModelSelect, PressOnUnfocusedModelOnlyFocuses)
{
  EXPECT_TRUE(modelPressOnUnfocusedModelOnlyFocusesForTest());
}

// With modelQuickSelect off (the default), pressing the already-focused
// model must open the context menu (STR_SELECT_MODEL lives there), not load
// the model directly.
TEST(ColorModelSelect, PressOnFocusedModelOpensMenuWhenQuickSelectDisabled)
{
  EXPECT_TRUE(modelPressOnFocusedModelOpensMenuWhenQuickSelectDisabledForTest());
}

// With modelQuickSelect on, pressing the already-focused model loads it
// immediately, matching legacy/mono-LCD behavior.
TEST(ColorModelSelect, PressOnFocusedModelQuickSelectsWhenEnabled)
{
  EXPECT_TRUE(modelPressOnFocusedModelQuickSelectsWhenEnabledForTest());
}

// Duplicate Model must never produce a card indistinguishable from its
// source -- addModel() copies modelName verbatim, so duplicateModel() must
// rename the copy itself.
TEST(ColorModelSelect, DuplicateModelNameDiffersFromSource)
{
  EXPECT_TRUE(buildDuplicateModelNameDiffersFromSourceForTest());
}

// Duplicating the same model twice must not produce two more
// identically-named cards: a taken suffix is skipped in favor of the next
// free one.
TEST(ColorModelSelect, DuplicateModelNameSkipsTakenSuffix)
{
  EXPECT_TRUE(buildDuplicateModelNameSkipsTakenSuffixForTest());
}

// A max-length model name still gets a disambiguating suffix -- the base is
// truncated to make room instead of the suffix being silently dropped.
TEST(ColorModelSelect, DuplicateModelNameTruncatesBaseToFit)
{
  EXPECT_TRUE(buildDuplicateModelNameTruncatesBaseToFitForTest());
}

#endif
