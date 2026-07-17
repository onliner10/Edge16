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
    BaseDialog(title, true)
{
  form.with([&](Window& formWindow) {
    messageWidget = Window::makeLive<StaticText>(
        &formWindow, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT}, message,
        COLOR_THEME_PRIMARY1_INDEX, messageFlags);

    if (info) {
      infoWidget = Window::makeLive<StaticText>(
          &formWindow, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT}, info,
          COLOR_THEME_PRIMARY1_INDEX, infoFlags);
    }
  });
}

void MessageDialog::onLiveClicked(LiveWindow&) { deleteLater(); }

//-----------------------------------------------------------------------------

DynamicMessageDialog::DynamicMessageDialog(
    const char* title, std::function<std::string()> textHandler,
    const char* message, const int lineHeight, LcdColorIndex color, LcdFlags textFlags) :
    BaseDialog(title, true)
{
  form.with([&](Window& formWindow) {
    messageWidget = Window::makeLive<StaticText>(
        &formWindow, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT}, message,
        COLOR_THEME_PRIMARY1_INDEX, CENTERED);

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
                             std::function<void(void)> cancelHandler) :
    BaseDialog(title, false),
    confirmHandler(std::move(confirmHandler)),
    cancelHandler(std::move(cancelHandler))
{
  form.with([&](Window& formWindow) {
    if (message) {
      Window::makeLive<StaticText>(
          &formWindow, rect_t{0, 0, LV_PCT(100), 0}, message,
          COLOR_THEME_PRIMARY1_INDEX, CENTERED);
    }

    buildRequiredWindow<Window>(
        [&](Window& box) {
          box.padAll(PAD_TINY);
          box.setFlexLayout(LV_FLEX_FLOW_ROW, 40, LV_PCT(100),
                            LV_SIZE_CONTENT);
          box.withLive([](Window::LiveWindow& live) {
            lv_obj_set_flex_align(live.lvobj(), LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_SPACE_BETWEEN);
          });

          buildRequiredWindow<TextButton>(
              [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_NO,
              [=]() -> int8_t {
                onCancel();
                return 0;
              });

          buildRequiredWindow<TextButton>(
              [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_YES,
              [=]() -> int8_t {
                this->deleteLater();
                this->confirmHandler();
                return 0;
              });
        },
        &formWindow, rect_t{});
  });
}

void ConfirmDialog::onCancel()
{
  deleteLater();
  if (cancelHandler) cancelHandler();
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

              // EXIT/cancel: never blocks creation, just keeps whatever
              // name creation would otherwise have produced.
              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_EXIT,
                  [=]() {
                    finish(false);
                    return 0;
                  });

              // Checkmark/confirm: applies the typed name.
              buildRequiredWindow<TextButton>(
                  [](TextButton&) {}, &box, rect_t{0, 0, 96, 0}, STR_OK,
                  [=]() {
                    finish(true);
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

void ModelNameDialog::finish(bool wantApply)
{
  bool applied = false;
  std::string result;

  if (wantApply) {
    int len = fieldLen;
    while (len > 0 &&
          (edited[len - 1] == '\0' || edited[len - 1] == ' '))
      len--;
    if (len > 0) {
      result.assign(edited, len);
      applied = true;
    }
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

  void pressOkForTest() { finish(true); }
  void pressExitForTest() { finish(false); }
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

// EXIT/cancel never blocks creation and never applies partially typed text
// -- the caller is told to keep whatever name creation would otherwise have
// produced.
bool modelNameDialogKeepsAutoNameOnExitForTest()
{
  bool applied = true;
  std::string result = "unset";

  auto* dlg = new (std::nothrow) TestModelNameDialog(
      "MODEL04", 7, [&](bool a, std::string n) {
        applied = a;
        result = n;
      });
  if (!dlg || !dlg->isAvailable()) return false;

  dlg->typeForTest("ShouldBeIgnored");
  dlg->pressExitForTest();
  MainWindow::instance()->runMainLoopTick();

  return !applied && result.empty();
}

// Confirming with an empty field must not block or "apply" a blank name --
// it degrades to the same outcome as EXIT.
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
#endif
