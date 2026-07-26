/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
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

#include "dialog.h"

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "keyboard_base.h"
#include "lcd.h"
#include "mainwindow.h"
#include "progress.h"
#include "static.h"
#include "textedit.h"
#include "widget_palette.h"  // BTN_FILL_* / buttonFillLvColor -- ConfirmDialog destructive-role test hook

#include <algorithm>
#include <new>

//-----------------------------------------------------------------------------

class BaseDialogForm : public Window
{
 public:
  BaseDialogForm(Window* parent, lv_coord_t width, bool flexLayout) : Window(parent, rect_t{})
  {
    withLive([](LiveWindow& live) { etx_scrollbar(live.lvobj()); });
    padAll(PAD_TINY);
    if (flexLayout)
      setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, width, LV_SIZE_CONTENT);
  }

 protected:
  void onLiveClicked(LiveWindow&) override { Keyboard::hide(false); }
};

BaseDialog::BaseDialog(const char* title,
                       bool closeIfClickedOutside, lv_coord_t width,
                       lv_coord_t maxHeight, bool flexLayout) :
    ModalWindow(closeIfClickedOutside)
{
#if defined(SIMU)
  setAutomationText(title ? title : "");
#endif

  requireWindow(form, static_cast<Window*>(this));

  RequiredWindow<Window> content;
  if (!initRequiredWindow(content, this, rect_t{})) return;
  content.with([&](Window& contentWindow) {
    contentWindow.setWindowFlag(OPAQUE);
    contentWindow.padAll(PAD_ZERO);
    contentWindow.setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, width,
                                LV_SIZE_CONTENT);
    contentWindow.withLive([](Window::LiveWindow& live) {
      etx_solid_bg(live.lvobj());
      lv_obj_center(live.lvobj());
    });

    header = Window::makeLive<StaticText>(
        &contentWindow, rect_t{0, 0, LV_PCT(100), 0}, title ? title : "",
        COLOR_THEME_PRIMARY2_INDEX);
  });
  if (header) {
    header->withLive([](Window::LiveWindow& live) {
      etx_solid_bg(live.lvobj(), COLOR_THEME_SECONDARY1_INDEX);
    });
    header->padAll(PAD_SMALL);
    header->show(title != nullptr);
  }

  content.with([&](Window& contentWindow) {
    Window* formWindow =
        Window::makeLive<BaseDialogForm>(&contentWindow, width, flexLayout);
    if (!formWindow) formWindow = &contentWindow;
    requireWindow(form, formWindow);
  });
  if (maxHeight != LV_SIZE_CONTENT) {
    form.with([&](Window& formWindow) {
      formWindow.withLive([&](Window::LiveWindow& live) {
        lv_obj_set_style_max_height(
            live.lvobj(), maxHeight - EdgeTxStyles::UI_ELEMENT_HEIGHT,
            LV_PART_MAIN);
      });
    });
  }
}

void BaseDialog::setTitle(const char* title)
{
  if (header) header->setText(title);
}

//-----------------------------------------------------------------------------

ProgressDialog::ProgressDialog(const char* title,
                               std::function<void()> onClose) :
    BaseDialog(title, false), onClose(std::move(onClose))
{
  form.with([&](Window& formWindow) {
    progress = Window::makeLive<Progress>(
        &formWindow, rect_t{0, 0, LV_PCT(100), 32});
    if (progress) updateProgress(0);
  });
}

void ProgressDialog::setTitle(std::string title)
{
  if (header) {
    header->setText(title);
    header->show();
  }
}

void ProgressDialog::updateProgress(int percentage)
{
  if (progress) {
    progress->setValue(percentage);
    lvglRefreshNowIfIdle();
  }
}

void ProgressDialog::closeDialog()
{
  deleteLater();
  onClose();
}

//-----------------------------------------------------------------------------

MessageDialog::MessageDialog(const char* title,
                             const char* message, const char* info,
                             LcdFlags messageFlags, LcdFlags infoFlags) :
    ds::Dialog(title, /*closeIfClickedOutside=*/true)
{
  // Dismissible message popup (POPUP_INFORMATION / POPUP_WARNING). ds::Dialog
  // owns the frame padding and inter-line gap; the legacy centered look is
  // preserved via body()'s `centered` flag.
  messageWidget = body(message, ds::TextRole::Body, (messageFlags & CENTERED) != 0);
  if (info)
    infoWidget = body(info, ds::TextRole::Body, (infoFlags & CENTERED) != 0);
}

void MessageDialog::onLiveClicked(LiveWindow&) { deleteLater(); }

//-----------------------------------------------------------------------------

DynamicMessageDialog::DynamicMessageDialog(
    const char* title, std::function<std::string()> textHandler,
    const char* message, const int lineHeight, LcdColorIndex color, LcdFlags textFlags) :
    ds::Dialog(title, /*closeIfClickedOutside=*/true)
{
  messageWidget = body(message, ds::TextRole::Body, /*centered=*/true);

  // The live-updating info line is a DynamicText, which has no ds::Dialog
  // body() equivalent; build it straight into the DS-spaced form so it still
  // sits in the DS-owned line stack.
  form.with([&](Window& formWindow) {
    infoWidget = Window::makeLive<DynamicText>(
        &formWindow, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT}, textHandler,
        color, textFlags);
  });
}

void DynamicMessageDialog::onLiveClicked(LiveWindow&) { deleteLater(); }

//-----------------------------------------------------------------------------

ConfirmDialog::ConfirmDialog(const char* title,
                             const char* message,
                             std::function<void(void)> confirmHandler,
                             std::function<void(void)> cancelHandler,
                             bool destructive) :
    ds::Dialog(title, /*closeIfClickedOutside=*/false),
    confirmHandler(std::move(confirmHandler)),
    cancelHandler(std::move(cancelHandler))
{
  // Blocking confirm: an out-of-bounds tap does nothing (closeIfClickedOutside
  // is false, inherited unchanged from the original BaseDialog(title, false));
  // the pilot must pick YES or NO, or press EXIT (-> onCancel -> cancel).
  if (message) body(message, ds::TextRole::Body, /*centered=*/true);

  // DS action row: right-aligned, primary/destructive (confirm) rightmost.
  // ds::Dialog owns the button spacing/sizing and auto-closes the dialog
  // after the handler. Reference the members explicitly (the ctor params of
  // the same name shadow them in this scope).
  //
  // A destructive confirm's YES uses ds::ButtonRole::Destructive (warning-
  // colored) instead of Primary (bold filled accent), so an irreversible
  // choice (delete sensor/theme, receiver reset, ...) never looks like the
  // visually-preferred one. NO always stays Secondary -- the safe default.
  action(STR_NO, ds::ButtonRole::Secondary, [this]() {
    if (this->cancelHandler) this->cancelHandler();
  });
  yesButton = action(
      STR_YES, destructive ? ds::ButtonRole::Destructive : ds::ButtonRole::Primary,
      [this]() {
        if (this->confirmHandler) this->confirmHandler();
      });
}

void ConfirmDialog::onCancel()
{
  deleteLater();
  if (cancelHandler) cancelHandler();
}

void confirmDestructive(const char* title, const char* object,
                        std::function<void(void)> onConfirm)
{
  new (std::nothrow) ConfirmDialog(title, object, std::move(onConfirm),
                                   /*cancelHandler=*/nullptr,
                                   /*destructive=*/true);
}

//-----------------------------------------------------------------------------

LabelDialog::LabelDialog(const char *label, int length, const char* title,
            std::function<void(std::string)> _saveHandler) :
    ModalWindow(false), saveHandler(std::move(_saveHandler))
{
  int labelLength = std::max(0, std::min(length, MAX_LABEL_LENGTH));
  this->label[0] = '\0';
  if (label && labelLength > 0) {
    strncpy(this->label, label, labelLength);
  }
  this->label[labelLength] = '\0';

  buildRequiredWindow<Window>(
      [&](Window& form) {
        form.padAll(PAD_ZERO);
        form.setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, LCD_W * 0.8,
                           LV_SIZE_CONTENT);
        form.withLive([](Window::LiveWindow& live) {
          etx_solid_bg(live.lvobj());
          lv_obj_center(live.lvobj());
        });

        auto hdr = Window::makeLive<StaticText>(
            &form, rect_t{0, 0, LV_PCT(100), 0}, title,
            COLOR_THEME_PRIMARY2_INDEX);
        if (hdr) {
          hdr->withLive([](Window::LiveWindow& live) {
            etx_solid_bg(live.lvobj(), COLOR_THEME_SECONDARY1_INDEX);
          });
          hdr->padAll(PAD_MEDIUM);
        }

        buildRequiredWindow<Window>(
            [&](Window& box) {
              box.padAll(PAD_MEDIUM);
              box.setFlexLayout(LV_FLEX_FLOW_ROW, 40, LV_PCT(100),
                                LV_SIZE_CONTENT);
              box.withLive([](Window::LiveWindow& live) {
                lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_SPACE_BETWEEN);
              });

              buildRequiredWindow<TextEdit>(
                  [](TextEdit&) {}, &box, rect_t{0, 0, LV_PCT(100), 0},
                  this->label, labelLength);
            },
            &form, rect_t{});

        buildRequiredWindow<Window>(
            [&](Window& box) {
              box.padAll(PAD_MEDIUM);
              box.setFlexLayout(LV_FLEX_FLOW_ROW, 40, LV_PCT(100),
                                LV_SIZE_CONTENT);
              box.withLive([](Window::LiveWindow& live) {
                lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_SPACE_BETWEEN);
              });

              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_CANCEL,
                  [=]() {
                    deleteLater();
                    return 0;
                  });

              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_SAVE,
                  [=]() {
                    if (saveHandler != nullptr) saveHandler(this->label);
                    deleteLater();
                    return 0;
                  });
            },
            &form, rect_t{});
      },
      this, rect_t{});
}

//-----------------------------------------------------------------------------

ModelNameDialog::ModelNameDialog(
    const char* currentName, uint8_t length,
    std::function<void(bool, std::string)> _doneHandler) :
    ModalWindow(false), doneHandler(std::move(_doneHandler))
{
  fieldLen = (uint8_t)std::max(
      0, std::min((int)length, (int)MAX_NAME_LEN));

  // Capture the auto-generated name as the placeholder hint; trim the
  // trailing padding a fixed-length model-name buffer may carry so the hint
  // reads cleanly (e.g. "MODEL03" rather than "MODEL03\0\0\0\0\0\0\0\0").
  if (currentName && fieldLen > 0) {
    memcpy(hint, currentName, fieldLen);
  }
  hint[fieldLen] = '\0';
  for (int i = fieldLen - 1; i >= 0 && (hint[i] == '\0' || hint[i] == ' '); i--)
    hint[i] = '\0';

  // Edit buffer starts empty: with the placeholder showing, the pilot's
  // first keystroke is a real character, not a backspace over the hint.
  edited[0] = '\0';

  buildRequiredWindow<Window>(
      [&](Window& form) {
        form.padAll(PAD_ZERO);
        form.setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO, LCD_W * 0.8,
                           LV_SIZE_CONTENT);
        form.withLive([](Window::LiveWindow& live) {
          etx_solid_bg(live.lvobj());
          // Anchored to the top, not centered: the on-screen keyboard opens
          // immediately and occupies the bottom ~2/5 of the screen (see
          // Keyboard::Keyboard / KEYBOARD_HEIGHT), so a vertically centered
          // dialog would push its own EXIT/OK buttons underneath the
          // keyboard and make them untappable while it's showing.
          lv_obj_align(live.lvobj(), LV_ALIGN_TOP_MID, 0, PAD_LARGE);
        });

        auto hdr = Window::makeLive<StaticText>(
            &form, rect_t{0, 0, LV_PCT(100), 0}, STR_MODELNAME,
            COLOR_THEME_PRIMARY2_INDEX);
        if (hdr) {
          hdr->withLive([](Window::LiveWindow& live) {
            etx_solid_bg(live.lvobj(), COLOR_THEME_SECONDARY1_INDEX);
          });
          hdr->padAll(PAD_MEDIUM);
        }

        buildRequiredWindow<Window>(
            [&](Window& box) {
              box.padAll(PAD_MEDIUM);
              box.setFlexLayout(LV_FLEX_FLOW_ROW, 40, LV_PCT(100),
                                LV_SIZE_CONTENT);
              box.withLive([](Window::LiveWindow& live) {
                lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_SPACE_BETWEEN);
              });

              buildRequiredWindow<TextEdit>(
                  [&](TextEdit& te) { nameField = &te; }, &box,
                  rect_t{0, 0, LV_PCT(100), 0}, this->edited, fieldLen,
                  nullptr, this->hint);
            },
            &form, rect_t{});

        buildRequiredWindow<Window>(
            [&](Window& box) {
              box.padAll(PAD_MEDIUM);
              box.setFlexLayout(LV_FLEX_FLOW_ROW, 40, LV_PCT(100),
                                LV_SIZE_CONTENT);
              box.withLive([](Window::LiveWindow& live) {
                lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_SPACE_BETWEEN);
              });

              // EXIT/RTN: never blocks creation (the model already exists),
              // and never discards typed text -- it commits whatever is
              // currently in the field, exactly like OK.
              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_EXIT,
                  [=]() {
                    finish();
                    return 0;
                  });

              // Checkmark/confirm: applies the typed name.
              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_OK,
                  [=]() {
                    finish();
                    return 0;
                  });
            },
            &form, rect_t{});
      },
      this, rect_t{});

  // Open the keyboard immediately -- no tap needed to start typing, so the
  // pilot's first keystroke always counts.
  if (nameField) nameField->openNow();
}

void ModelNameDialog::finish()
{
  // The model already exists by the time this dialog is showing (see
  // ModelLabelsWindow::newModel), so there's no "cancel" outcome to
  // distinguish -- OK and EXIT/RTN both commit whatever is currently typed.
  // Only a genuinely empty field (trailing padding/spaces trimmed) leaves
  // 'applied' false, so the caller keeps whichever name creation already
  // produced instead of writing a blank one.
  bool applied = false;
  std::string result;

  int len = fieldLen;
  while (len > 0 &&
        (edited[len - 1] == '\0' || edited[len - 1] == ' '))
    len--;
  if (len > 0) {
    result.assign(edited, len);
    applied = true;
  }

  auto handler = doneHandler;
  deleteLater();
  if (handler) handler(applied, result);
}

#if defined(SIMU)
namespace {
class TestModelNameDialog : public ModelNameDialog
{
 public:
  TestModelNameDialog(const char* currentName, uint8_t length,
                      std::function<void(bool, std::string)> doneHandler) :
      ModelNameDialog(currentName, length, std::move(doneHandler))
  {
  }

  // Simulates the pilot typing, bypassing the real on-screen keyboard so the
  // test doesn't depend on LVGL input-device plumbing.
  void typeForTest(const char* text)
  {
    strncpy(edited, text, sizeof(edited) - 1);
    edited[sizeof(edited) - 1] = '\0';
  }

  void pressOkForTest() { finish(); }
  void pressExitForTest() { finish(); }
  bool keyboardOpenedForTest() const { return nameField != nullptr; }
  std::string hintForTest() const { return std::string(hint); }
};
}  // namespace

// Confirming with a typed name applies it, and the placeholder hint offered
// is the auto-generated name -- i.e. the ghost-text design described in
// ModelLabelsWindow::newModel.
bool modelNameDialogAppliesTypedNameOnConfirmForTest()
{
  bool applied = false;
  std::string result = "unset";

  auto* dlg = new (std::nothrow) TestModelNameDialog(
      "MODEL03", 7, [&](bool a, std::string n) {
        applied = a;
        result = n;
      });
  if (!dlg || !dlg->isAvailable()) return false;

  bool keyboardOpened = dlg->keyboardOpenedForTest();
  bool hintIsAutoName = dlg->hintForTest() == "MODEL03";

  dlg->typeForTest("Test1");
  dlg->pressOkForTest();
  MainWindow::instance()->runMainLoopTick();

  return keyboardOpened && hintIsAutoName && applied && result == "Test1";
}

// EXIT/RTN never blocks creation (the model already exists) -- but unlike a
// real cancel, it must not discard a typed name either: the model is
// already there, so throwing away what the pilot just typed would silently
// leave it stuck on the auto-generated default (e.g. "MODEL06"). EXIT
// commits the current field exactly like OK.
bool modelNameDialogAppliesTypedNameOnExitForTest()
{
  bool applied = false;
  std::string result = "unset";

  auto* dlg = new (std::nothrow) TestModelNameDialog(
      "MODEL04", 7, [&](bool a, std::string n) {
        applied = a;
        result = n;
      });
  if (!dlg || !dlg->isAvailable()) return false;

  dlg->typeForTest("Test1");
  dlg->pressExitForTest();
  MainWindow::instance()->runMainLoopTick();

  return applied && result == "Test1";
}

// Confirming with an empty field must not block or "apply" a blank name --
// it keeps the auto-generated default.
bool modelNameDialogEmptyConfirmKeepsAutoNameForTest()
{
  bool applied = true;
  std::string result = "unset";

  auto* dlg = new (std::nothrow) TestModelNameDialog(
      "MODEL05", 7, [&](bool a, std::string n) {
        applied = a;
        result = n;
      });
  if (!dlg || !dlg->isAvailable()) return false;

  dlg->pressOkForTest();  // 'edited' left empty -- nothing typed
  MainWindow::instance()->runMainLoopTick();

  return !applied && result.empty();
}

// EXIT/RTN with nothing typed has nothing to commit -- it degrades to the
// same outcome as an empty OK, keeping the auto-generated default rather
// than applying a blank name.
bool modelNameDialogEmptyExitKeepsAutoNameForTest()
{
  bool applied = true;
  std::string result = "unset";

  auto* dlg = new (std::nothrow) TestModelNameDialog(
      "MODEL06", 7, [&](bool a, std::string n) {
        applied = a;
        result = n;
      });
  if (!dlg || !dlg->isAvailable()) return false;

  dlg->pressExitForTest();  // 'edited' left empty -- nothing typed
  MainWindow::instance()->runMainLoopTick();

  return !applied && result.empty();
}

//-----------------------------------------------------------------------------
// Migration proof: ConfirmDialog / MessageDialog / DynamicMessageDialog now
// build on ds::Dialog (DS-owned title/body/action spacing). These hooks assert
// the behavior that MUST survive that change: a confirm dialog stays BLOCKING
// (an out-of-bounds tap neither dismisses it nor fires a handler), its cancel
// path runs the cancel handler exactly once, a message popup stays dismissible,
// and a ds::Dialog action button actually invokes its handler (the substrate
// the confirm YES/NO buttons ride on).

namespace {
class TestConfirmDialog : public ConfirmDialog
{
 public:
  using ConfirmDialog::ConfirmDialog;
  bool isBlockingForTest() const { return !closeWhenClickOutside; }
  void outsideTapForTest()
  {
    withLive([&](LiveWindow& live) { onLiveClicked(live); });
  }
  void cancelForTest() { onCancel(); }
  // Whether YES currently renders with the Destructive fill (see
  // ds::DSButton / widget_palette BTN_FILL_*), as opposed to Primary.
  bool yesIsDestructiveForTest() const
  {
    if (!yesButton) return false;
    bool destructive = false;
    yesButton->withLive([&](LiveWindow& live) {
      lv_color_t fill = lv_obj_get_style_bg_color(live.lvobj(), LV_PART_MAIN);
      destructive = lv_color_eq(fill, buttonFillLvColor(BTN_FILL_DESTRUCTIVE));
    });
    return destructive;
  }
};

class TestMessageDialog : public MessageDialog
{
 public:
  using MessageDialog::MessageDialog;
  bool isDismissibleForTest() const { return closeWhenClickOutside; }
};
}  // namespace

// A blocking confirm dialog ignores an out-of-bounds tap entirely (no dismiss,
// no handler) -- the mis-tap-safety guarantee for a destructive confirm -- and
// resolves only on an explicit choice / EXIT, running the cancel handler once.
bool confirmDialogBlockingIgnoresOutsideTapForTest()
{
  bool confirmed = false;
  int cancelCount = 0;

  auto* dlg = new (std::nothrow) TestConfirmDialog(
      "Delete?", "INPUT1", [&]() { confirmed = true; },
      [&]() { cancelCount++; });
  if (!dlg || !dlg->isAvailable()) return false;

  bool blocking = dlg->isBlockingForTest();

  dlg->outsideTapForTest();  // an out-of-bounds tap must be inert
  MainWindow::instance()->runMainLoopTick();
  bool inert = dlg->isAvailable() && !confirmed && cancelCount == 0;

  dlg->cancelForTest();  // explicit EXIT/cancel resolves it
  MainWindow::instance()->runMainLoopTick();
  bool cancelledOnce = !confirmed && cancelCount == 1;

  return blocking && inert && cancelledOnce;
}

// A message popup (POPUP_INFORMATION / POPUP_WARNING) stays dismissible.
bool messageDialogIsDismissibleForTest()
{
  auto* dlg = new (std::nothrow) TestMessageDialog("Info", "Hello");
  if (!dlg || !dlg->isAvailable()) return false;
  bool dismissible = dlg->isDismissibleForTest();
  dlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();
  return dismissible;
}

// A ds::Dialog action button invokes its handler when clicked -- the path the
// migrated confirm dialog's YES/NO buttons rely on.
bool dsDialogActionInvokesHandlerForTest()
{
  bool ran = false;
  auto* dlg = new (std::nothrow) ds::Dialog("Title");
  if (!dlg || !dlg->isAvailable()) return false;
  dlg->body("Body");
  auto* btn =
      dlg->action("OK", ds::ButtonRole::Primary, [&]() { ran = true; });
  if (!btn) {
    dlg->deleteLater();
    MainWindow::instance()->runMainLoopTick();
    return false;
  }
  MainWindow::instance()->runMainLoopTick();
  btn->sendLvEvent(LV_EVENT_CLICKED);  // the click also deleteLater()s dlg
  MainWindow::instance()->runMainLoopTick();
  return ran;
}

// The action row is CENTERED as a group, not right-aligned. Every dialog in
// this app centers its body text, so a right-aligned action row put the
// buttons hard against the right edge under centered prose, with a wide dead
// gap on the left. Asserted as real geometry -- the slack to the left of the
// first button and to the right of the last must match -- rather than by
// reading back the flex constant, so the property survives a refactor that
// achieves the layout some other way.
bool dsDialogActionsAreCenteredForTest()
{
  auto* dlg = new (std::nothrow) ds::Dialog("Title");
  if (!dlg || !dlg->isAvailable()) return false;
  dlg->body("A reasonably long body line so the dialog is wider than the "
            "buttons");
  auto* no = dlg->action("No", ds::ButtonRole::Secondary, []() {});
  auto* yes = dlg->action("Yes", ds::ButtonRole::Primary, []() {});
  if (!no || !yes) {
    dlg->deleteLater();
    MainWindow::instance()->runMainLoopTick();
    return false;
  }
  for (int i = 0; i < 3; i++) MainWindow::instance()->runMainLoopTick();

  lv_area_t rowArea{}, firstArea{}, lastArea{};
  bool got = false;
  no->withLive([&](Window::LiveWindow& live) {
    lv_obj_t* btn = live.lvobj();
    lv_obj_t* row = lv_obj_get_parent(btn);
    if (!row) return;
    lv_obj_update_layout(row);
    lv_obj_get_content_coords(row, &rowArea);
    lv_obj_get_coords(btn, &firstArea);
    got = true;
  });
  yes->withLive(
      [&](Window::LiveWindow& live) { lv_obj_get_coords(live.lvobj(), &lastArea); });

  const int32_t leftSlack = firstArea.x1 - rowArea.x1;
  const int32_t rightSlack = rowArea.x2 - lastArea.x2;

  dlg->deleteLater();
  for (int i = 0; i < 3; i++) MainWindow::instance()->runMainLoopTick();

  // Allow 1 px for odd-width rounding when the slack is split in two.
  return got && leftSlack > 0 && std::abs(leftSlack - rightSlack) <= 1;
}

// A destructive confirm's YES action renders with the Destructive fill
// (warning-colored), not Primary -- an irreversible choice (delete
// sensor/theme, receiver reset, ...) must never look like the
// visually-preferred one (#8). A confirm that does NOT ask for `destructive`
// stays Primary, unaffected.
bool confirmDialogYesIsDestructiveWhenFlaggedForTest()
{
  auto* destructiveDlg = new (std::nothrow) TestConfirmDialog(
      "Delete?", "All sensors", []() {}, nullptr, /*destructive=*/true);
  if (!destructiveDlg || !destructiveDlg->isAvailable()) return false;
  bool destructiveIsFlagged = destructiveDlg->yesIsDestructiveForTest();
  destructiveDlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();

  auto* benignDlg =
      new (std::nothrow) TestConfirmDialog("Apply?", "Changes", []() {});
  if (!benignDlg || !benignDlg->isAvailable()) return false;
  bool benignStaysPrimary = !benignDlg->yesIsDestructiveForTest();
  benignDlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();

  return destructiveIsFlagged && benignStaysPrimary;
}
#endif
