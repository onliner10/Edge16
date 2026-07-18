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

#pragma once

#include <memory>

#include "ds_core.h"
#include "list_line_button.h"
#include "edgetx.h"
#include "page.h"
#include "pagegroup.h"
#include "route.h"

struct CustomFunctionData;
class FunctionEditPage;
class FunctionLineButton;
class NumberEdit;

//-----------------------------------------------------------------------------

class FunctionLineButton : public ListLineButton
{
 public:
  FunctionLineButton(Window *parent, const rect_t &rect,
                     CustomFunctionData *cfn, uint8_t index,
                     const char *prefix);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "FunctionButton"; }
#endif

  void onLineLoaded() override;

  void onRefresh() override;
  bool functionEnabled() const;
  void setFunctionEnabled(bool enabled);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): row geometry is owned by
  // ds::RowContent — no per-screen coordinate constants.

 protected:
  CustomFunctionData *cfn;
  const char *prefix;

  void updateAutomationText();

  char sfNameText[16] = {};
  char sfSwitchText[32] = {};
  char sfFuncText[96] = {};
  char sfSubtitleText[56] = {};
  char sfRepeatText[32] = {};
  CheckButton *sfEnable = nullptr;
  std::unique_ptr<ds::RowContent> dsRow;

  virtual bool isActive() const override = 0;
  virtual void setDirty() const = 0;
};

//-----------------------------------------------------------------------------

class FunctionEditPage : public Page
{
 public:
  FunctionEditPage(uint8_t index, EdgeTxIcon icon, Route route, const char *title,
                   const char *prefix);

  void delayedInit() override;

 protected:
  uint8_t index;
  Window *specialFunctionOneWindow = nullptr;
  StaticText *headerSF = nullptr;
  bool active = false;

  virtual bool isActive() const = 0;
  virtual bool isSwitchAvailable(int value) const = 0;
  virtual CustomFunctionData *customFunctionData() const = 0;
  virtual bool isAssignableFunctionAvailable(int function) const = 0;
  virtual void setDirty() const = 0;

  void onLiveCheckEvents(LiveWindow& live) override;

  void buildHeader(Window *window, const char *title, const char *prefix);

  void addSourceChoice(FormLine *line, const char *title,
                       CustomFunctionData *cfn, int16_t vmax);

  NumberEdit *addNumberEdit(FormLine *line, const char *title,
                            CustomFunctionData *cfn, int16_t vmin, int16_t vmax);

  void updateSpecialFunctionOneWindow();

  void buildBody(Window *form);
};

//-----------------------------------------------------------------------------

class FunctionsPage : public PageGroupItem
{
 public:
  bool openRoute(const Route& r, uint8_t depth) override;
  FunctionsPage(CustomFunctionData* functions, const PageDef& pageDef, const char* prefix);

  void build(Window* window) override;

 protected:
  int8_t focusIndex = -1;
  int8_t prevFocusIndex = -1;
  bool isRebuilding = false;
  CustomFunctionData* functions;
  ButtonBase* addButton = nullptr;
  const char* prefix = nullptr;
  Window* pageWindow = nullptr;

  void rebuild(Window* window);
  void newSF(Window* window, bool pasteSF);
  void pasteSpecialFunctionData(uint8_t index);
  void editSpecialFunction(Window* window, uint8_t index);
  void editSpecialFunction(Window* window, uint8_t index,
                           FunctionLineButton& button);
  void pasteSpecialFunction(Window* window, uint8_t index);
  void pasteSpecialFunction(Window* window, uint8_t index,
                            FunctionLineButton& button);
  void plusPopup(Window* window);

  virtual CustomFunctionData* customFunctionData(uint8_t index) const = 0;
  virtual FunctionEditPage* editPage(uint8_t index) const = 0;
  virtual uint8_t editorPageId() const = 0;
  virtual FunctionLineButton* functionButton(Window* parent, const rect_t& rect,
                                         uint8_t index) const = 0;
  virtual void setDirty() const = 0;

  // Icon/title for the empty-state shown when nothing is configured yet.
  virtual EdgeTxIcon listIcon() const = 0;
  virtual const char* listTitle() const = 0;
};

//-----------------------------------------------------------------------------

class SpecialFunctionsPage : public FunctionsPage
{
 public:
  SpecialFunctionsPage(const PageDef& pageDef);

 protected:
  CustomFunctionData* customFunctionData(uint8_t index) const override;
  FunctionEditPage* editPage(uint8_t index) const override;
  uint8_t editorPageId() const override { return RP_SPECIAL_FUNCTION_EDIT; }
  FunctionLineButton* functionButton(Window* parent, const rect_t& rect,
                                 uint8_t index) const override;
  void setDirty() const override;
  EdgeTxIcon listIcon() const override { return ICON_MODEL_SPECIAL_FUNCTIONS; }
  const char* listTitle() const override { return STR_MENUCUSTOMFUNC; }
};

//-----------------------------------------------------------------------------

class GlobalFunctionsPage : public FunctionsPage
{
 public:
  GlobalFunctionsPage(const PageDef& pageDef);

 protected:
  CustomFunctionData* customFunctionData(uint8_t index) const override;
  FunctionEditPage* editPage(uint8_t index) const override;
  uint8_t editorPageId() const override { return RP_GLOBAL_FUNCTION_EDIT; }
  FunctionLineButton* functionButton(Window* parent, const rect_t& rect,
                                 uint8_t index) const override;
  void setDirty() const override;
  EdgeTxIcon listIcon() const override { return ICON_RADIO_GLOBAL_FUNCTIONS; }
  const char* listTitle() const override { return STR_MENUSPECIALFUNCS; }
};
