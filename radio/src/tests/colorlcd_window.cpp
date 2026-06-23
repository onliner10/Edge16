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

#if defined(COLORLCD) && !GTEST_OS_WINDOWS

#include <sys/wait.h>
#include <unistd.h>

#include <new>

#include "mainwindow.h"
#include "page.h"
#include "setup_menus/pagegroup.h"

bool etxCreateObjectAllocationFailureReturnsNullForTest();
bool etxLabelAllocationFailureReturnsNullForTest();
bool etxStyleHelpersIgnoreNullObjectForTest();
bool windowObjectAllocationFailureLeavesNoLvObjForTest();
bool formFieldObjectAllocationFailureFailsClosedForTest();
bool childOfUnavailableParentFailsClosedForTest();
bool adoptLiveFailedChildDetachesFromParentForTest();
bool requiredWindowBuilderFailureFailsOwnerClosedForTest();
bool attachToUnavailableParentPreservesExistingParentForTest();
bool unavailableWindowDirectClickDoesNotBubbleForTest();
bool windowDelayedLoadGatesLoadedTasksForTest();
bool windowCloseHandlerRunsAfterDeferredCycleForTest();
bool windowClearUsesManagedAsyncChildDeletionForTest();
bool forcedScrollIgnoresNegativeEdgeDistancesForTest();
bool buttonMatrixObjectAllocationFailureFailsClosedForTest();
bool tableFieldObjectAllocationFailureFailsClosedForTest();
bool tableFieldInvalidSelectionClearsWithoutScrollForTest();
bool tableFieldSelectMovesAcrossColumnsForTest();
bool toggleSwitchObjectAllocationFailureFailsClosedForTest();
bool textEditTextAreaAllocationFailureDoesNotCacheDeadEditorForTest();
bool numberEditNumberAreaAllocationFailureDoesNotCacheDeadEditorForTest();
bool numberEditCancelActiveEditorDoesNotCrashForTest();
bool textKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboardForTest();
bool textKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboardForTest();
bool numberKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboardForTest();
bool numberKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboardForTest();
bool progressBarAllocationFailureFailsClosedForTest();
bool listBoxObjectAllocationFailureFailsClosedForTest();
bool lvglWrapperUnavailableMainWindowIsNotLoadedForTest();
bool fullScreenDialogMessageLabelCreateFailureFailsClosedForTest();
bool quickMenuInvalidRememberedPageFallsBackForTest();

namespace {

bool routeApiScopedAndOneShotForTest()
{
  // --- Route struct ---
  Route r;
  r.rootIcon = ICON_MODEL;
  r.pages[0] = QM_MODEL_MIXES;
  r.pages[1] = RP_MIX_EDIT;
  r.params[1] = 42;
  r.depth = 2;

  if (!r.valid()) return false;

  // appended() adds a segment without mutating the original
  Route r2 = r.appended(RP_CURVE_EDIT, 7);
  if (r2.depth != 3 || r2.pages[2] != RP_CURVE_EDIT ||
      r2.params[2] != 7 || r.depth != 2)
    return false;

  // --- Pending route: remember / clear / one-shot ---
  Page::clearRememberedRoute();
  if (Page::pendingRoute().valid) return false;

  Page::rememberRoute(r);
  if (!Page::pendingRoute().valid) return false;
  if (!Page::pendingRoute().modelScoped) return false;  // ICON_MODEL
  if (Page::pendingRoute().route.pages[1] != RP_MIX_EDIT) return false;
  if (Page::pendingRoute().route.params[1] != 42) return false;

  Page::clearRememberedRoute();
  if (Page::pendingRoute().valid) return false;

  // Non-model icon → not model-scoped
  Route radioRoute;
  radioRoute.rootIcon = ICON_RADIO;
  radioRoute.pages[0] = QM_RADIO_HARDWARE;
  radioRoute.depth = 1;
  Page::rememberRoute(radioRoute);
  if (Page::pendingRoute().modelScoped) return false;
  Page::clearRememberedRoute();

  // --- PageGroupItem openRoute dispatch ---
  class TestTab : public PageGroupItem {
   public:
    bool opened = false;
    uint8_t openedPage = 0;
    int16_t openedParam = 0;

    TestTab() : PageGroupItem("Test", QM_MODEL_MIXES) {}
    void build(Window*) override {}
    bool openRoute(const Route& r, uint8_t depth) override
    {
      if (depth >= r.depth) return true;
      opened = true;
      openedPage = r.pages[depth];
      openedParam = r.params[depth];
      return true;
    }
  };

  TestTab tab;
  Route tabRoute;
  tabRoute.rootIcon = ICON_MODEL;
  tabRoute.pages[0] = QM_MODEL_MIXES;
  tabRoute.depth = 1;
  tab.setRoute(tabRoute);

  // Tab's openRoute receives depth 1 (segment 0 = tab, handled by PageGroup)
  if (!tab.openRoute(r, 1)) return false;
  if (!tab.opened || tab.openedPage != RP_MIX_EDIT || tab.openedParam != 42)
    return false;

  // Default PageGroupItem (no override): can't descend, but route-exhausted = ok
  class DefaultTab : public PageGroupItem {
   public:
    DefaultTab() : PageGroupItem("Default", QM_MODEL_INPUTS) {}
    void build(Window*) override {}
  };
  DefaultTab defaultTab;
  if (defaultTab.openRoute(r, 0)) return false;   // can't descend
  if (!defaultTab.openRoute(r, 2)) return false;  // exhausted → true

  return true;
}

class LongPressReturnTestPageGroupItem : public PageGroupItem
{
 public:
  explicit LongPressReturnTestPageGroupItem(const PageDef& pageDef) :
      PageGroupItem(pageDef)
  {
  }
  void build(Window*) override {}
};

class LongPressReturnTestEditorPage : public Page
{
 public:
  LongPressReturnTestEditorPage() : Page(ICON_MODEL_MIXER, Route{}) {}
  void longPressReturnForTest() { onLongPressRTN(); }
};

static const PageDef longPressReturnPages[] = {
    { ICON_MODEL_MIXER, STR_DEF(STR_QM_MIXES), STR_DEF(STR_MIXES),
      PAGE_CREATE, QM_MODEL_MIXES,
      [](const PageDef& pageDef) -> PageGroupItem* {
        return new (std::nothrow) LongPressReturnTestPageGroupItem(pageDef);
      } },
    { EDGETX_ICONS_COUNT }
};

template <typename Fn>
bool withLongPressReturnStackForTest(Fn check)
{
  auto pageGroup = new (std::nothrow) PageGroup(ICON_MODEL, "Mixes",
                                                longPressReturnPages);
  auto editor = new (std::nothrow) LongPressReturnTestEditorPage();
  if (!pageGroup || !editor) {
    delete editor;
    delete pageGroup;
    return false;
  }

  return check(pageGroup, editor);
}

bool pageLongPressReturnDefersPageGroupCloseForTest()
{
  return withLongPressReturnStackForTest(
      [](PageGroup* pageGroup, LongPressReturnTestEditorPage* editor) {
        const bool setup = Window::pageGroup() == pageGroup;
        editor->longPressReturnForTest();

        const bool notClosedInline = Window::pageGroup() == pageGroup;
        Window::runDeferredCloseHandlersForTest();
        const bool notClosedSameCycle = Window::pageGroup() == pageGroup;
        Window::runDeferredCloseHandlersForTest();
        const bool closedAfterDeferredCycle = Window::pageGroup() == nullptr;

        return setup && notClosedInline && notClosedSameCycle &&
               closedAfterDeferredCycle;
      });
}

bool ownerBoundDeferredMutationSkipsDeletedWindowForTest()
{
  bool called = false;
  auto window = new (std::nothrow) Window(MainWindow::instance(), rect_t{});
  if (!window) return false;

  window->deferWindowMutation([&](Window&, UiMutationToken&) { called = true; });
  window->deleteLater();
  Window::runDeferredCloseHandlersForTest();

  return !called;
}

bool pageLongPressReturnKeepsEditorVisibleUntilHomeForTest()
{
  return withLongPressReturnStackForTest(
      [](PageGroup* pageGroup, LongPressReturnTestEditorPage* editor) {
        const bool setup = Window::pageGroup() == pageGroup &&
                           Window::topWindow() == editor;
        editor->longPressReturnForTest();

        const bool editorStillTopInline = Window::topWindow() == editor;
        Window::runDeferredCloseHandlersForTest();
        const bool editorStillTopSameCycle = Window::topWindow() == editor;
        Window::runDeferredCloseHandlersForTest();
        const bool closedAfterDeferredCycle = Window::pageGroup() == nullptr &&
                                              Window::topWindow() != pageGroup;

        return setup && editorStillTopInline && editorStillTopSameCycle &&
               closedAfterDeferredCycle;
      });
}

}  // namespace

TEST(ColorEtxTheme, ObjectAllocationFailureReturnsNull)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(etxCreateObjectAllocationFailureReturnsNullForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorEtxTheme, LabelAllocationFailureReturnsNull)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(etxLabelAllocationFailureReturnsNullForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorEtxTheme, StyleHelpersIgnoreNullObject)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(etxStyleHelpersIgnoreNullObjectForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ObjectAllocationFailureLeavesNoLvObj)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(windowObjectAllocationFailureLeavesNoLvObjForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, FormFieldObjectAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(formFieldObjectAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ChildOfUnavailableParentFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(childOfUnavailableParentFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, AdoptLiveFailedChildDetachesFromParent)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(adoptLiveFailedChildDetachesFromParentForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, RequiredWindowBuilderFailureFailsOwnerClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(requiredWindowBuilderFailureFailsOwnerClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, AttachToUnavailableParentPreservesExistingParent)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(attachToUnavailableParentPreservesExistingParentForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, UnavailableWindowDirectClickDoesNotBubble)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(unavailableWindowDirectClickDoesNotBubbleForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, DelayedLoadGatesLoadedTasks)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(windowDelayedLoadGatesLoadedTasksForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, CloseHandlerRunsAfterDeferredCycle)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(windowCloseHandlerRunsAfterDeferredCycleForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, QuickMenuInvalidRememberedPageFallsBack)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(quickMenuInvalidRememberedPageFallsBackForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, RouteApiScopedAndOneShot)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(routeApiScopedAndOneShotForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, LongPressReturnDefersPageGroupClose)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(pageLongPressReturnDefersPageGroupCloseForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, LongPressReturnKeepsEditorVisibleUntilHome)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(pageLongPressReturnKeepsEditorVisibleUntilHomeForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, OwnerBoundDeferredMutationSkipsDeletedWindow)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(ownerBoundDeferredMutationSkipsDeletedWindowForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ClearUsesManagedAsyncChildDeletion)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(windowClearUsesManagedAsyncChildDeletionForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ForcedScrollIgnoresNegativeEdgeDistances)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(forcedScrollIgnoresNegativeEdgeDistancesForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ButtonMatrixObjectAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(buttonMatrixObjectAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TableFieldObjectAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(tableFieldObjectAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TableFieldInvalidSelectionClearsWithoutScroll)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(tableFieldInvalidSelectionClearsWithoutScrollForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TableFieldSelectMovesAcrossColumns)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(tableFieldSelectMovesAcrossColumnsForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ToggleSwitchObjectAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(toggleSwitchObjectAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TextEditTextAreaAllocationFailureDoesNotCacheDeadEditor)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(textEditTextAreaAllocationFailureDoesNotCacheDeadEditorForTest() ? 0
                                                                           : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, NumberEditNumberAreaAllocationFailureDoesNotCacheDeadEditor)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(numberEditNumberAreaAllocationFailureDoesNotCacheDeadEditorForTest()
              ? 0
              : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, NumberEditCancelActiveEditorDoesNotCrash)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(numberEditCancelActiveEditorDoesNotCrashForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TextKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboard)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(textKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboardForTest()
              ? 0
              : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, TextKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboard)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(textKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboardForTest()
              ? 0
              : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, NumberKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboard)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(numberKeyboardWindowAllocationFailureDoesNotCacheDeadKeyboardForTest()
              ? 0
              : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, NumberKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboard)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(numberKeyboardKeypadAllocationFailureDoesNotCacheDeadKeyboardForTest()
              ? 0
              : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ProgressBarAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(progressBarAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, ListBoxObjectAllocationFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(listBoxObjectAllocationFailureFailsClosedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, FullScreenDialogMessageLabelCreateFailureFailsClosed)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(fullScreenDialogMessageLabelCreateFailureFailsClosedForTest() ? 0
                                                                        : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindow, UnavailableMainWindowIsNotLoaded)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    alarm(2);
    _exit(lvglWrapperUnavailableMainWindowIsNotLoadedForTest() ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ColorWindowLayers, PoppingMiddleLayerKeepsTopLayerActive)
{
  const pid_t pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {
    Window* parent = MainWindow::instance();
    auto bottom = new (std::nothrow) Window(parent, {0, 0, 10, 10});
    auto middle = new (std::nothrow) Window(parent, {0, 0, 10, 10});
    auto top = new (std::nothrow) Window(parent, {0, 0, 10, 10});

    if (!bottom || !middle || !top) _exit(2);

    bottom->pushLayer();
    lv_group_t* bottomGroup = lv_group_get_default();
    middle->pushLayer();
    top->pushLayer();
    lv_group_t* topGroup = lv_group_get_default();

    if (Window::topWindow() != top) _exit(3);

    middle->popLayer();

    if (Window::topWindow() != top) _exit(1);
    if (lv_group_get_default() != topGroup) _exit(4);

    top->popLayer();

    if (Window::topWindow() != bottom) _exit(5);
    if (lv_group_get_default() != bottomGroup) _exit(6);

    _exit(0);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status)) << "child process did not exit normally";
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

#endif
