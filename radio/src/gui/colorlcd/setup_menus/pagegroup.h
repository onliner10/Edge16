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

#include "bitmaps.h"
#include "messaging.h"
#include "quick_menu.h"
#include "route.h"

class HeaderIcon;
class PageGroupItem;
class PageGroupBase;
#if VERSION_MAJOR == 2
class SelectedTabIcon;
class PageGroupIconButton;
#endif

//-----------------------------------------------------------------------------

enum PageDefAction {
  PAGE_CREATE,
  PAGE_ACTION
};

struct PageDef {
  EdgeTxIcon icon;
  STR_TYP qmTitle;
  STR_TYP title;
  PageDefAction pageAction;
  QMPage qmPage;
  std::function<PageGroupItem*(const PageDef& pageDef)> create;
  std::function<bool()> enabled;
  std::function<void()> action;
};

#if VERSION_MAJOR > 2
extern PageDef favoritesMenuItems[];
#endif

//-----------------------------------------------------------------------------

class PageGroupItem
{
 public:
  PageGroupItem(std::string title, QMPage qmPage = QM_NONE) :
      title(std::move(title)), icon(ICON_EDGETX), qmPageId(qmPage), padding(PAD_SMALL)
  {}

  PageGroupItem(const PageDef& pageDef, PaddingSize padding = PAD_SMALL) :
      title(STR_VAL(pageDef.title)), icon(pageDef.icon), qmPageId(pageDef.qmPage),
      padding(padding), pageDef(&pageDef)
  {}

  virtual ~PageGroupItem() = default;

  bool isVisible() const { return (pageDef && pageDef->enabled) ? pageDef->enabled() : true; }

  virtual void build(Window* window) = 0;

  virtual void checkEvents() {}

  void setTitle(const std::string& value) { title = std::move(value); }
  std::string getTitle() const { return title; }

  void setIcon(EdgeTxIcon icon) { this->icon = icon; }
  EdgeTxIcon getIcon() const { return icon; }

  PaddingSize getPadding() const { return padding; }

  virtual void cleanup() {}

  QMPage pageId() const { return qmPageId; }

  // --- Route API ---------------------------------------------------------
  // Each tab carries a route prefix ({sectionIcon, tabPageId}) set by
  // PageGroupBase::addTab().  Tab pages that open sub-editors override
  // openRoute() to descend one more level.
  const Route& route() const { return _route; }
  void setRoute(Route r) { _route = r; }

  // Restore a sub-route starting at *depth* within this tab's subtree.
  // Default: route exhausted (true) or cannot descend (false).
  virtual bool openRoute(const Route& r, uint8_t depth)
  {
    return depth >= r.depth;
  }

 protected:
  std::string title;
  EdgeTxIcon icon;
  QMPage qmPageId = QM_NONE;
  PaddingSize padding;
  const PageDef* pageDef = nullptr;
  Route _route;
};

//-----------------------------------------------------------------------------

class PageGroupHeaderBase : public Window
{
 public:
  PageGroupHeaderBase(Window* parent, coord_t height, EdgeTxIcon icon, const char* parentTitle, PageGroupBase* menu);

  void setTitle(const char* title);
  void setIcon(EdgeTxIcon newIcon);

  void enablePageSelector();
  void openPageSelector();

  virtual void chgTab(int dir) = 0;

  void nextTab() { chgTab(1); }
  void prevTab() { chgTab(-1); }

  void addTab(PageGroupItem* page);

  void setCurrentIndex(uint8_t index);

  bool hasSubMenu(QMPage n);

  PageGroupItem* pageTab(uint8_t idx) const
  {
    return idx < pages.size() ? pages[idx] : nullptr;
  }
  bool isCurrent(uint8_t idx) const { return currentIndex == idx; }
  uint8_t tabCount() const { return pages.size(); }

  void onDelete() override;

#if VERSION_MAJOR == 2
  static LAYOUT_VAL_SCALED(ICON_EXTRA_H, 10)
  static LAYOUT_VAL_SCALED(MENU_HEADER_BUTTON_WIDTH, 33)
#endif

 protected:
  uint8_t currentIndex = 0;
  RequiredLvObj titleLabel;
  OptionalLvObj parentLabel;
  OptionalLvObj pageSelectorChevron;
  HeaderIcon* hdrIcon = nullptr;
  std::string parentTitle;
  std::vector<PageGroupItem*> pages;
  PageGroupBase* menu;
  bool pageSelectorEnabled = false;
#if VERSION_MAJOR ==2
  SelectedTabIcon* selectedIcon = nullptr;
  Window* carousel = nullptr;
  std::vector<PageGroupIconButton*> buttons;

  coord_t getX(uint8_t idx);
  void onLiveCheckEvents(LiveWindow& live) override;
#endif
};

//-----------------------------------------------------------------------------

class PageGroupBase : public NavWindow
{
 public:
  PageGroupBase(coord_t bodyY, EdgeTxIcon icon);

  void setCurrentTab(unsigned index);

  // Walk a Route from the tab level (depth 0 = first segment after root).
  // Finds the tab whose pageId matches, activates it, and delegates to
  // tab->openRoute() to descend further.
  bool openRoute(const Route& r, uint8_t depth);

  // If the pending remembered route matches this group's section icon (and
  // model filename for model-scoped routes), consume it and descend.
  // Called once from the PageGroup constructor.
  bool tryRestorePendingRoute();

  void onLiveClicked(LiveWindow&) override;
  void onCancel() override;
  void returnHomeFromLongPress();

  uint8_t tabCount() const;

  void addTab(PageGroupItem* page);

  bool hasSubMenu(QMPage n);

  coord_t getScrollY();
  void setScrollY(coord_t y);

#if defined(SIMU)
  bool automationRoute(Route& out) const override;
#endif

  EdgeTxIcon getIcon() const { return icon; }

 protected:
  RequiredWindow<PageGroupHeaderBase> header;
  RequiredWindow<Window> body;
  PageGroupItem* currentTab = nullptr;
  EdgeTxIcon icon;
  Messaging quickMenuMsg;
  // When true, addTab() does not auto-build the first tab; the constructor
  // is expected to call setCurrentTab() once after all tabs are added.
  bool deferInitialBuild = false;

  void onDelete() override;
  void onLiveCheckEvents(LiveWindow& live) override;

  void doBuild(Window& body, PageGroupItem* tab);

#if defined(HARDWARE_KEYS)
  void doKeyShortcut(event_t event);
  void onPressSYS() override;
  void onLongPressSYS() override;
  void onPressMDL() override;
  void onLongPressMDL() override;
  void onPressTELE() override;
  void onLongPressTELE() override;
  void onPressPGUP() override;
  void onPressPGDN() override;
  void onLongPressPGUP() override;
  void onLongPressPGDN() override;
  void onLongPressRTN() override;
#endif
};

//-----------------------------------------------------------------------------

class PageGroup : public PageGroupBase
{
 public:
  explicit PageGroup(EdgeTxIcon icon, const char* title, const PageDef* pages,
                     unsigned initialTab = 0);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "PageGroup"; }
#endif

  PageGroupItem* getCurrentTab() const { return currentTab; }

  bool isPageGroup() override { return true; }

#if VERSION_MAJOR == 2
  static LAYOUT_VAL_SCALED(PAGE_GROUP_TOP_BAR_H, 48)
  static constexpr coord_t PAGE_GROUP_ALT_TITLE_H = EdgeTxStyles::STD_FONT_HEIGHT;
  static constexpr coord_t PAGE_GROUP_BACK_BTN_W = 0;
  static LAYOUT_VAL_SCALED(PAGE_GROUP_BACK_BTN_XO, 45)
#else
  static LAYOUT_VAL_SCALED(PAGE_GROUP_TOP_BAR_H, 45)
  static constexpr coord_t PAGE_GROUP_ALT_TITLE_H = 0;
  static constexpr coord_t PAGE_GROUP_BACK_BTN_W = PAGE_GROUP_TOP_BAR_H;
  static constexpr coord_t PAGE_GROUP_BACK_BTN_XO = PAGE_GROUP_TOP_BAR_H;
#endif
  static constexpr coord_t PAGE_GROUP_BODY_Y = PAGE_GROUP_TOP_BAR_H + PAGE_GROUP_ALT_TITLE_H;

 protected:
};

//-----------------------------------------------------------------------------

class TabsGroup : public PageGroupBase
{
 public:
  explicit TabsGroup(EdgeTxIcon icon, const char* parentTitle);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "TabsGroup"; }
#endif

  void hidePageButtons();

  bool isPageGroup() override { return false; }

#if VERSION_MAJOR == 2
  static LAYOUT_VAL_SCALED(TABS_GROUP_TOP_BAR_H, 48)
  static constexpr coord_t TABS_GROUP_ALT_TITLE_H = EdgeTxStyles::STD_FONT_HEIGHT;
#else
  static LAYOUT_ORIENTATION_SCALED(TABS_GROUP_TOP_BAR_H, 45, 48)
  static LAYOUT_ORIENTATION(TABS_GROUP_ALT_TITLE_H, 0, EdgeTxStyles::STD_FONT_HEIGHT)
#endif
  static constexpr coord_t TABS_GROUP_BODY_Y = TABS_GROUP_TOP_BAR_H + TABS_GROUP_ALT_TITLE_H;

 protected:
};
