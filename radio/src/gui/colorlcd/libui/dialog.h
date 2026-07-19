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

#pragma once

// base_dialog.h provides BaseDialog; ds_core.h provides ds::Dialog (built on
// BaseDialog). The concrete message/confirm dialogs below are migrated onto
// ds::Dialog so their title/body/action spacing is design-system owned.
#include "base_dialog.h"
#include "ds_core.h"

class StaticText;
class DynamicText;
class Progress;
class TextEdit;

//-----------------------------------------------------------------------------

class ProgressDialog : public BaseDialog
{
 public:
  ProgressDialog(const char* title, std::function<void()> onClose);

  void updateProgress(int percentage);
  void setTitle(std::string title);
  void closeDialog();

 protected:
  uint32_t lastUpdate = 0;
  Progress* progress = nullptr;

  std::function<void()> onClose;

  // disable keys
  void onLiveEvent(LiveWindow&, event_t) override {}
};

//-----------------------------------------------------------------------------

class MessageDialog : public ds::Dialog
{
 public:
  MessageDialog(const char* title, const char* message,
                const char* info = nullptr, LcdFlags messageFlags = CENTERED,
                LcdFlags infoFlags = CENTERED);

 protected:
  StaticText* messageWidget = nullptr;
  StaticText* infoWidget = nullptr;

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "MessageDialog"; }
#endif

  void onLiveClicked(LiveWindow&) override;
};

//-----------------------------------------------------------------------------

class DynamicMessageDialog : public ds::Dialog
{
 public:
  DynamicMessageDialog(const char* title,
                       std::function<std::string()> textHandler,
                       const char* message = "",
                       const int lineHeight = EdgeTxStyles::STD_FONT_HEIGHT,
                       LcdColorIndex color = COLOR_THEME_PRIMARY1_INDEX, LcdFlags textFlags = CENTERED);
  // Attn.: FONT(XXL) is not supported by DynamicMessageDialog

 protected:
  StaticText* messageWidget = nullptr;
  DynamicText* infoWidget = nullptr;

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "DynamicMessageDialog"; }
#endif

  void onLiveClicked(LiveWindow&) override;
};

//-----------------------------------------------------------------------------

class ConfirmDialog : public ds::Dialog
{
 public:
  // `destructive` (default false) routes the YES action onto
  // ds::ButtonRole::Destructive instead of Primary -- a dangerous confirm
  // (delete sensor/theme, receiver reset, ...) must not present its
  // irreversible choice as the visually-preferred one. NO always stays
  // Secondary.
  ConfirmDialog(const char* title, const char* message,
                std::function<void(void)> confirmHandler,
                std::function<void(void)> cancelHandler = nullptr,
                bool destructive = false);

 protected:
  std::function<void(void)> confirmHandler;
  std::function<void(void)> cancelHandler;
  ds::DSButton* yesButton = nullptr;  // exposed for test role assertions

  void onCancel() override;
};

//-----------------------------------------------------------------------------

class LabelDialog : public ModalWindow
{
 public:
  LabelDialog(const char *label, int length, const char* title,
              std::function<void(std::string)> _saveHandler = nullptr);

  static constexpr int MAX_LABEL_LENGTH = 255;

  void onCancel() override { deleteLater(); }

 protected:
  std::function<void(std::string)> saveHandler;
  char label[MAX_LABEL_LENGTH + 1];
};

//-----------------------------------------------------------------------------

// Prompts for a name with the on-screen keyboard already open, so the
// pilot's very first keystroke replaces the ghost/placeholder hint instead
// of being spent tapping the field open or backspacing over an existing
// auto-generated value. Used at model creation time (see
// ModelLabelsWindow::newModel).
//
// 'currentName'/'length' is the fixed-length (not necessarily
// null-terminated) auto-generated name shown as the placeholder hint; it is
// never mutated by this dialog.
//
// The model this dialog names already exists by the time it's shown (see
// ModelLabelsWindow::newModel), so there is no "cancel creation" outcome --
// only whether the typed text is applied. doneHandler(applied, name) fires
// exactly once, however the dialog closes:
//  - Checkmark/OK, or EXIT/RTN, with a non-empty typed name: applied=true,
//    name=typed text. EXIT is a close action, not a discard -- whatever the
//    pilot typed still wins.
//  - Checkmark/OK, or EXIT/RTN, with an empty field: applied=false, name=""
//    -- the caller keeps whichever name creation would otherwise have
//    produced. Nothing was typed, so there is nothing to commit.
class ModelNameDialog : public ModalWindow
{
 public:
  ModelNameDialog(const char* currentName, uint8_t length,
                  std::function<void(bool applied, std::string name)>
                      doneHandler);

  static constexpr uint8_t MAX_NAME_LEN = 64;

  void onCancel() override { finish(); }

 protected:
  uint8_t fieldLen;
  char edited[MAX_NAME_LEN + 1] = {};
  char hint[MAX_NAME_LEN + 1] = {};
  std::function<void(bool, std::string)> doneHandler;
  TextEdit* nameField = nullptr;

  void finish();
};
