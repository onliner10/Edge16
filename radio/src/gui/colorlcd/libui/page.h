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

#include "route.h"
#include "static.h"
#include "messaging.h"
#include "quick_menu_def.h"

#include <cstdint>

class PageGroupItem;
class PageHeader : public Window
{
 public:
  PageHeader(Window* parent, EdgeTxIcon icon);
  PageHeader(Window* parent, const char* iconFile);

  void setTitle(std::string txt) { if (title) title->setText(std::move(txt)); }
  StaticText* setTitle2(std::string txt);

  static LAYOUT_VAL_SCALED(PAGE_TITLE_LEFT, 50)
  static constexpr coord_t PAGE_TITLE_TOP = PAD_TINY;

 protected:
  StaticText* title = nullptr;
  StaticText* title2 = nullptr;
};

class Page : public NavWindow
{
 public:
  explicit Page(EdgeTxIcon icon, Route route, PaddingSize padding = PAD_MEDIUM, bool pauseRefresh = false);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "Page"; }
#endif

  void onCancel() override;
  void onLiveClicked(LiveWindow&) override;

  // --- Route API -----------------------------------------------------------
  // Every Page carries a Route describing its location in the page tree.
  // The Route is set at construction — the compiler demands it, so no
  // editor can accidentally forget to declare its restorable path.
  // Long-press RTN remembers the top page's route; reopening the matching
  // section auto-descends to the remembered depth via openRoute().
  const Route& route() const { return _route; }

  // Restore a sub-route within this page's subtree.  Default: the route is
  // exhausted (true) or this page cannot descend (false).  Override in pages
  // that spawn sub-editors.
  virtual bool openRoute(const Route& r, uint8_t depth)
  {
    return depth >= r.depth;
  }

  // Remembered-route management (one global slot, one-shot).
  static void rememberRoute(const Route& r);
  static void clearRememberedRoute();
  static RememberedRoute& pendingRoute();

#if defined(SIMU)
  std::string automationRole() const override { return "page"; }
  bool automationRoute(Route& out) const override
  {
    out = _route;
    return _route.valid();
  }
#endif

  void enableRefresh();

protected:
  RequiredWindow<PageHeader> headerHandle;
  RequiredWindow<Window> bodyHandle;
  PageHeader* header = nullptr;
  Window* body = nullptr;
  Messaging quickMenuMsg;
  Route _route;

  bool bubbleEvents() override { return false; }

  NavWindow* navWindow();

  // Cancel this Page and run its closeHandler while the parent PageGroup (and
  // its line buttons) are still alive.  deleteLater() would otherwise defer the
  // handler until after SYS/MDL/TELE / Quick Menu navigation tears down that
  // parent — a UAF/freeze when the handler rebuilds or updates line buttons.
  void cancelWithCloseHandler();

  template <typename Fn>
  bool withPageHeader(Fn&& fn)
  {
    return headerHandle.with(std::forward<Fn>(fn));
  }

  template <typename Fn>
  bool withPageBody(Fn&& fn)
  {
    return bodyHandle.with(std::forward<Fn>(fn));
  }

  template <typename Fn>
  bool withPageFrame(Fn&& fn)
  {
    return headerHandle.with([&](PageHeader& header) {
      return bodyHandle.with(
          [&](Window& body) { return fn(header, body); });
    });
  }

#if defined(HARDWARE_KEYS)
  void onPressSYS() override;
  void onLongPressSYS() override;
  void onPressMDL() override;
  void onLongPressMDL() override;
  void onPressTELE() override;
  void onLongPressTELE() override;
  void onLongPressRTN() override;
#endif
};

class SubPage : public Page
{
 public:
  SubPage(EdgeTxIcon icon, Route route, const char* title, const char* subtitle, bool pauseRefresh = false);
  SubPage(EdgeTxIcon icon, Route route, const char* title, const char* subtitle, const SetupLineDef* setupLines);

  Window* setupLine(const char* title, std::function<void(SetupLine*, coord_t, coord_t)> createEdit, coord_t lblYOffset = 0);

  void useFlexLayout();

  static LAYOUT_SIZE(EDT_X, LCD_W * 9 / 20, LCD_W * 8 / 20)

 protected:
  coord_t y = 0;
};
