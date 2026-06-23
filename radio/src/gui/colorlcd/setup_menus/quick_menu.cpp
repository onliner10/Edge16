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

#include "quick_menu.h"

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "mainwindow.h"
#include "model_select.h"
#include "quick_menu_group.h"
#include "radio_tools.h"
#include "screen_setup.h"
#include "theme_manager.h"
#include "view_channels.h"
#include "view_main.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <new>
#include <sstream>

//-----------------------------------------------------------------------------

bool QMMainDef::isSubMenu(QMPage page) const
{
  for (int i = 0; subMenuItems[i].icon < EDGETX_ICONS_COUNT; i += 1)
    if (subMenuItems[i].qmPage == page)
      return true;
  return false;
}

bool QMMainDef::isSubMenu(QMPage page, EdgeTxIcon curIcon) const
{
  if ((icon != EDGETX_ICONS_COUNT) && (icon != curIcon))
    return false;
  return isSubMenu(page);
}

int QMMainDef::getIndex(QMPage n) const
{
  for (int i = 0; subMenuItems[i].icon < EDGETX_ICONS_COUNT; i += 1)
    if (subMenuItems[i].qmPage == n)
      return i;
  return -1;
}

//-----------------------------------------------------------------------------

#if VERSION_MAJOR > 2
class QuickSubMenu
{
 public:
  QuickSubMenu(Window* parent, QuickMenu* quickMenu, const QMMainDef* mainDef):
    parent(parent), quickMenu(quickMenu), mainDef(mainDef)
  {}

  bool isSubMenu(QMPage n)
  {
    return mainDef->isSubMenu(n);
  }

  bool isSubMenu(QMPage n, EdgeTxIcon curIcon)
  {
    return mainDef->isSubMenu(n, curIcon);
  }

  bool isSubMenu(ButtonBase* b)
  {
    return menuButton == b;
  }

  int getIndex(QMPage n)
  {
    return mainDef->getIndex(n);
  }

  ButtonBase* addButton()
  {
    menuButton = quickMenu->getTopMenu()->addButton(mainDef->icon, STR_VAL(mainDef->qmTitle),
                        [=]() -> uint8_t {
                          activate();
                          return 0;
                        }, 
	                        mainDef->enabled,
	                        [=](bool focus) {
	                          quickMenu->withLive([&](Window::LiveWindow&) {
	                            if (focus) {
	                              if (!subMenu) buildSubMenu();
	                              quickMenu->getTopMenu()->setCurrent(menuButton);
	                            }
	                            if (subMenu)
	                              subMenu->show(focus);
	                            if (!focus && quickMenu->getTopMenu())
	                              quickMenu->getTopMenu()->setGroup();
	                          });
	                        });

    return menuButton;
  }

  void enableSubMenu()
  {
    subMenu->activate();
    quickMenu->enableSubMenu();
  }

  void setDisabled(bool all)
  {
    if (subMenu)
      subMenu->setDisabled(all);
  }

  void setCurrent(QMPage n)
  {
    if (!subMenu) buildSubMenu();
    if (!subMenu) return;
    quickMenu->getTopMenu()->setCurrent(menuButton);
    quickMenu->getTopMenu()->setDisabled(false);
    subMenu->setCurrent(getIndex(n));
    enableSubMenu();
  }

  void activate()
  {
    if (!subMenu) buildSubMenu();
    if (!subMenu) return;
    quickMenu->getTopMenu()->setCurrent(menuButton);
    quickMenu->getTopMenu()->setDisabled(false);
    quickMenu->getTopMenu()->clearFocus();
    enableSubMenu();
  }

  void buildSubMenu()
  {
    subMenu = new (std::nothrow) QuickMenuGroup(parent);
    if (!subMenu) return;

    for (int i = 0; mainDef->subMenuItems[i].icon < EDGETX_ICONS_COUNT; i += 1) {
      auto pg = mainDef->subMenuItems[i].qmPage;
      const char* title;
      if (pg < QM_APP) {
        title = STR_VAL(mainDef->subMenuItems[i].qmTitle);
      } else {
        title = getLuaTool(pg - QM_APP)->label.c_str();
      }
      subMenu->addButton(mainDef->subMenuItems[i].icon, title,
          std::bind(&QuickSubMenu::onPress, this, i),
          mainDef->subMenuItems[i].enabled,
          [=](bool focus) { if (focus) QuickMenu::setCurrentPage(mainDef->subMenuItems[i].qmPage, mainDef->icon); });
    }

    doLayout();
    subMenu->hide();
    subMenu->setDisabled(true);
  }

  uint8_t onPress(int n)
  {
    if (mainDef->subMenuItems[n].pageAction == PAGE_CREATE) {
      quickMenu->getTopMenu()->clearFocus();
      int pgIdx = getPageNumber(n);
      auto pageGroup = Window::pageGroup();
      if (pageGroup && pageGroup->hasSubMenu(mainDef->subMenuItems[n].qmPage)) {
        quickMenu->onSelect(false);
        pageGroup->setCurrentTab(pgIdx);
      } else {
        quickMenu->onSelect(true);
        new PageGroup(mainDef->icon, STR_VAL(mainDef->title),
                      mainDef->subMenuItems, pgIdx);
      }
    } else {
      quickMenu->getTopMenu()->setCurrent(menuButton);
      quickMenu->getTopMenu()->setDisabled(false);
      quickMenu->getTopMenu()->clearFocus();
      enableSubMenu();
      onSelect(true);
      mainDef->subMenuItems[n].action();
    }
    return 0;
  }

  void onSelect(bool close)
  {
    quickMenu->onSelect(close);
  }

  int getPageNumber(int iconNumber)
  {
    int pageNumber = 0;
    for (int i = 0; i < iconNumber; i += 1)
      if (mainDef->subMenuItems[i].pageAction == PAGE_CREATE)
        pageNumber += 1;
    return pageNumber;
  }

  void doLayout()
  {
    if (subMenu)
      subMenu->doLayout(QuickMenu::QM_SUB_COLS);
  }

  void clearSubMenu()
  {
    if (subMenu) {
      subMenu->deleteLater();
      subMenu = nullptr;
    }
  }

 protected:
  Window* parent;
  QuickMenu* quickMenu;
  const QMMainDef* mainDef;
  QuickMenuGroup* subMenu = nullptr;
  ButtonBase* menuButton = nullptr;
};
#endif

//-----------------------------------------------------------------------------

QuickMenu* QuickMenu::instance = nullptr;

// Indexed by section entry QMPage; only pages in (QM_NONE, QM_APP) are stored.
static QMPage lastVisitedByEntryPage[QM_APP] = {};

static bool canIndexLastVisited(QMPage page)
{
  return page > QM_NONE && page < QM_APP;
}

static bool isEnabledPage(const PageDef& page)
{
  return page.pageAction == PAGE_CREATE && (!page.enabled || page.enabled());
}

static QMPage firstCreatePage(const QMMainDef& section)
{
  if (!section.subMenuItems)
    return QM_NONE;

  for (int i = 0; section.subMenuItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (section.subMenuItems[i].pageAction == PAGE_CREATE)
      return section.subMenuItems[i].qmPage;
  }

  return QM_NONE;
}

static bool sectionContainsPage(const QMMainDef& section, QMPage page,
                                bool requireEnabled = false)
{
  if (!section.subMenuItems)
    return false;

  for (int i = 0; section.subMenuItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const PageDef& pageDef = section.subMenuItems[i];
    if (pageDef.qmPage == page && (!requireEnabled || isEnabledPage(pageDef)))
      return true;
  }

  return false;
}

static const QMMainDef* findSectionForPage(
    QMPage page, EdgeTxIcon groupIcon = EDGETX_ICONS_COUNT)
{
  for (int i = QuickMenu::FIRST_SEARCH_IDX;
       qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const QMMainDef& section = qmTopItems[i];
    if (section.pageAction != QM_SUBMENU)
      continue;

    if (groupIcon != EDGETX_ICONS_COUNT && section.icon != groupIcon)
      continue;

    if (sectionContainsPage(section, page))
      return &section;
  }

  return nullptr;
}

// Section entry reopens the last page actually visited in that section.
// If that page is no longer available, use the first enabled page in the section.
static QMPage resolveLastVisitedSectionPage(const QMMainDef& section,
                                            QMPage lastVisitedPage)
{
  if (!section.subMenuItems)
    return QM_NONE;

  QMPage firstAvailablePage = QM_NONE;

  for (int i = 0; section.subMenuItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const PageDef& page = section.subMenuItems[i];
    if (!isEnabledPage(page))
      continue;

    if (firstAvailablePage == QM_NONE)
      firstAvailablePage = page.qmPage;

    if (page.qmPage == lastVisitedPage)
      return lastVisitedPage;
  }

  return firstAvailablePage;
}

static QMPage restoreLastVisitedSectionPage(QMPage requestedPage)
{
  for (int i = QuickMenu::FIRST_SEARCH_IDX;
       qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const QMMainDef& section = qmTopItems[i];
    if (section.pageAction != QM_SUBMENU)
      continue;

    QMPage entryPage = firstCreatePage(section);
    if (requestedPage != entryPage)
      continue;

    QMPage rememberedPage = entryPage;
    if (canIndexLastVisited(entryPage) &&
        lastVisitedByEntryPage[entryPage] != QM_NONE)
      rememberedPage = lastVisitedByEntryPage[entryPage];

    QMPage page = resolveLastVisitedSectionPage(section, rememberedPage);
    return page == QM_NONE ? requestedPage : page;
  }

  return requestedPage;
}

#if defined(SIMU)
static const PageDef* findPageDef(QMPage page)
{
  const auto* section = findSectionForPage(page);
  if (!section || !section->subMenuItems)
    return nullptr;

  for (int i = 0; section->subMenuItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const PageDef& pageDef = section->subMenuItems[i];
    if (pageDef.qmPage == page)
      return &pageDef;
  }

  return nullptr;
}

static const char* sectionToken(QMPage page)
{
  switch (page) {
    case QM_MODEL_SETUP:
    case QM_MODEL_FLIGHTMODES:
    case QM_MODEL_INPUTS:
    case QM_MODEL_MIXES:
    case QM_MODEL_OUTPUTS:
    case QM_MODEL_CURVES:
    case QM_MODEL_GVARS:
    case QM_MODEL_LS:
    case QM_MODEL_SF:
    case QM_MODEL_SCRIPTS:
    case QM_MODEL_TELEMETRY:
    case QM_MODEL_NOTES:
      return "model";
    case QM_RADIO_SETUP:
    case QM_RADIO_GF:
    case QM_RADIO_TRAINER:
    case QM_RADIO_HARDWARE:
    case QM_RADIO_VERSION:
      return "radio";
    case QM_UI_THEMES:
    case QM_UI_SETUP:
    case QM_UI_SCREEN1:
    case QM_UI_SCREEN2:
    case QM_UI_SCREEN3:
    case QM_UI_SCREEN4:
    case QM_UI_SCREEN5:
    case QM_UI_SCREEN6:
    case QM_UI_SCREEN7:
    case QM_UI_SCREEN8:
    case QM_UI_SCREEN9:
    case QM_UI_SCREEN10:
    case QM_UI_ADD_PG:
      return "ui";
    case QM_TOOLS_APPS:
    case QM_TOOLS_STORAGE:
    case QM_TOOLS_RESET:
    case QM_TOOLS_CHAN_MON:
    case QM_TOOLS_LS_MON:
    case QM_TOOLS_STATS:
    case QM_TOOLS_DEBUG:
      return "tools";
    default:
      return "unknown";
  }
}

static const char* pageToken(QMPage page)
{
  switch (page) {
    case QM_MODEL_SETUP: return "setup";
    case QM_MODEL_FLIGHTMODES: return "flight_modes";
    case QM_MODEL_INPUTS: return "inputs";
    case QM_MODEL_MIXES: return "mixes";
    case QM_MODEL_OUTPUTS: return "outputs";
    case QM_MODEL_CURVES: return "curves";
    case QM_MODEL_GVARS: return "gvars";
    case QM_MODEL_LS: return "logical_switches";
    case QM_MODEL_SF: return "special_functions";
    case QM_MODEL_SCRIPTS: return "scripts";
    case QM_MODEL_TELEMETRY: return "telemetry";
    case QM_MODEL_NOTES: return "notes";
    case QM_RADIO_SETUP: return "setup";
    case QM_RADIO_GF: return "global_functions";
    case QM_RADIO_TRAINER: return "trainer";
    case QM_RADIO_HARDWARE: return "hardware";
    case QM_RADIO_VERSION: return "about";
    case QM_UI_THEMES: return "themes";
    case QM_UI_SETUP: return "top_bar";
    case QM_UI_SCREEN1: return "screen1";
    case QM_UI_SCREEN2: return "screen2";
    case QM_UI_SCREEN3: return "screen3";
    case QM_UI_SCREEN4: return "screen4";
    case QM_UI_SCREEN5: return "screen5";
    case QM_UI_SCREEN6: return "screen6";
    case QM_UI_SCREEN7: return "screen7";
    case QM_UI_SCREEN8: return "screen8";
    case QM_UI_SCREEN9: return "screen9";
    case QM_UI_SCREEN10: return "screen10";
    case QM_UI_ADD_PG: return "add_screen";
    case QM_TOOLS_APPS: return "apps";
    case QM_TOOLS_STORAGE: return "storage";
    case QM_TOOLS_RESET: return "reset";
    case QM_TOOLS_CHAN_MON: return "channel_monitor";
    case QM_TOOLS_LS_MON: return "logical_switches_monitor";
    case QM_TOOLS_STATS: return "stats";
    case QM_TOOLS_DEBUG: return "debug";
    default: return nullptr;
  }
}

static const char* editorToken(uint8_t pageId)
{
  switch (pageId) {
    case RP_MIX_EDIT: return "mix_edit";
    case RP_INPUT_EDIT: return "input_edit";
    case RP_CURVE_EDIT: return "curve_edit";
    case RP_OUTPUT_EDIT: return "output_edit";
    case RP_LOGICAL_SWITCH_EDIT: return "logical_switch_edit";
    case RP_SPECIAL_FUNCTION_EDIT: return "special_function_edit";
    case RP_GLOBAL_FUNCTION_EDIT: return "global_function_edit";
    case RP_FLIGHT_MODE_EDIT: return "flight_mode_edit";
    case RP_GVAR_EDIT: return "gvar_edit";
    case RP_SENSOR_EDIT: return "sensor_edit";
    case RP_SCRIPT_EDIT: return "script_edit";
    case RP_TIMER_EDIT: return "timer_edit";
    case RP_USB_CHANNEL_EDIT: return "usb_channel_edit";
    case RP_MIX_ADVANCED: return "mix_advanced";
    case RP_INPUT_ADVANCED: return "input_advanced";
    case RP_BATTERY_PACK_EDIT: return "battery_pack_edit";
    default: return nullptr;
  }
}

static bool editorPageIdForToken(const std::string& token, uint8_t& pageId)
{
  const struct Entry {
    const char* token;
    uint8_t pageId;
  } entries[] = {
      {"mix_edit", RP_MIX_EDIT},
      {"input_edit", RP_INPUT_EDIT},
      {"curve_edit", RP_CURVE_EDIT},
      {"output_edit", RP_OUTPUT_EDIT},
      {"logical_switch_edit", RP_LOGICAL_SWITCH_EDIT},
      {"special_function_edit", RP_SPECIAL_FUNCTION_EDIT},
      {"global_function_edit", RP_GLOBAL_FUNCTION_EDIT},
      {"flight_mode_edit", RP_FLIGHT_MODE_EDIT},
      {"gvar_edit", RP_GVAR_EDIT},
      {"sensor_edit", RP_SENSOR_EDIT},
      {"script_edit", RP_SCRIPT_EDIT},
      {"timer_edit", RP_TIMER_EDIT},
      {"usb_channel_edit", RP_USB_CHANNEL_EDIT},
      {"mix_advanced", RP_MIX_ADVANCED},
      {"input_advanced", RP_INPUT_ADVANCED},
      {"battery_pack_edit", RP_BATTERY_PACK_EDIT},
  };

  for (const auto& entry : entries) {
    if (token == entry.token) {
      pageId = entry.pageId;
      return true;
    }
  }
  return false;
}
#endif

void QuickMenu::openQuickMenu()
{
  if (instance && instance->isVisible())
    return;

  if (!instance)
    instance = new QuickMenu();

  QMPage qmPage = QM_NONE;
  PageGroup* p = Window::pageGroup();
  if (p)
    qmPage = p->getCurrentTab()->pageId();

  instance->openQM(p, qmPage);
}

void QuickMenu::shutdownQuickMenu()
{
  if (instance) instance->deleteLater();
}

QuickMenu::QuickMenu() :
    NavWindow(MainWindow::instance(), {QM_X, QM_Y, QM_W, QM_H})
{
  setWindowFlag(OPAQUE);

  addStyle(styles->bg_opacity_90, LV_PART_MAIN);
  withLive([](LiveWindow& live) {
    etx_bg_color(live.lvobj(), COLOR_THEME_QM_BG_INDEX);
  });

  withLive([&](LiveWindow& live) {
    auto sep = lv_obj_create(live.lvobj());
    if (!requireLvObj(sep)) return false;
    etx_solid_bg(sep, COLOR_THEME_QM_FG_INDEX);
    lv_obj_set_size(sep, QM_W, PAD_THREE);
    return true;
  });

  auto mask = getBuiltinIcon(ICON_TOP_LOGO);
  new (std::nothrow) StaticIcon(this, (QM_W - mask->width) / 2, 0,
                                ICON_TOP_LOGO, COLOR_THEME_QM_FG_INDEX);

  new (std::nothrow) ButtonBase(
    this, {0, 0, QM_W, EdgeTxStyles::UI_ELEMENT_HEIGHT},
    [=]() -> uint8_t {
      inSubMenu = false;
      onCancel();
      return 0;
    },
    window_create);

  OptionalWindow<Window> subMenuBox;

#if VERSION_MAJOR > 2
  subMenuBox.reset(
      Window::makeLive<Window>(this, rect_t{QM_SUB_X, QM_SUB_Y, QM_SUB_W, QM_SUB_H}));

  updateFavorites();
#endif

  buildRequiredWindow<Window>(
      [&](Window& box) {
        buildRequiredWindow<QuickMenuGroup>(
            [&](QuickMenuGroup& menu) {
              mainMenu = &menu;

              for (int i = 0; qmTopItems[i].icon != EDGETX_ICONS_COUNT;
                   i += 1) {
                if (qmTopItems[i].pageAction == QM_ACTION) {
                  mainMenu->addButton(
                      qmTopItems[i].icon, STR_VAL(qmTopItems[i].qmTitle),
                      [=]() {
                        onSelect(true);
                        qmTopItems[i].action();
                      },
                      qmTopItems[i].enabled);
#if VERSION_MAJOR > 2
                } else {
                  subMenuBox.with([&](Window& subBox) {
                    auto sub =
                        new (std::nothrow) QuickSubMenu(&subBox, this,
                                                        &qmTopItems[i]);
                    if (!sub) return;
                    sub->addButton();
                    subMenus.emplace_back(sub);
                  });
#endif
                }
              }
            },
            &box);
      },
      this, rect_t{QM_MAIN_X, QM_MAIN_Y, QM_MAIN_W, QM_MAIN_H});
}

void QuickMenu::onDelete()
{
  instance = nullptr;
}

void QuickMenu::openQM(PageGroupBase* newPageGroup, QMPage newCurPage)
{
  pushLayer();

#if VERSION_MAJOR == 2
  curPage = QM_NONE;
#endif

  mainMenu->doLayout(QM_MAIN_COLS);
#if VERSION_MAJOR > 2
  for (size_t i = 0; i < subMenus.size(); i += 1)
    subMenus[i]->doLayout();
#endif

  show();
  moveForeground();
  if (newPageGroup) {
    pageGroup = newPageGroup;
    curPage = newCurPage;
    curIcon = pageGroup->getIcon();
    mainMenu->setDisabled(false);
    mainMenu->clearFocus();
    setFocus(curPage);
  } else {
    pageGroup = nullptr;
    mainMenu->clearFocus();
    if (curPage != QM_MANAGE_MODELS && curPage != QM_NONE) {
      mainMenu->setDisabled(false);
      setFocus(curPage);
    } else {
#if VERSION_MAJOR > 2
      if (curPage == QM_MANAGE_MODELS)
        mainMenu->setCurrent(favoritesMenuItems[0].icon == EDGETX_ICONS_COUNT ? 0 : 1);
#endif
      focusMainMenu();
    }
  }
}

void QuickMenu::selected()
{
  if (instance)
    instance->onSelect(true);
}

void QuickMenu::openPage(QMPage page)
{
  page = restoreLastVisitedSectionPage(page);

  for (int i = FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (qmTopItems[i].pageAction == QM_ACTION) {
      if (qmTopItems[i].qmPage == page) {
        QuickMenu::selected();
        setCurrentPage(page, qmTopItems[i].icon);
        qmTopItems[i].action();
        return;
        }
    } else {
      const PageDef* sub = qmTopItems[i].subMenuItems;
      for (int j = 0, k = 0; sub[j].icon != EDGETX_ICONS_COUNT; j += 1) {
        if (sub[j].qmPage == page) {
          if (sub[j].pageAction == PAGE_ACTION) {
            QuickMenu::selected();
            setCurrentPage(page, qmTopItems[i].icon);
            sub[j].action();
          } else {
            QuickMenu::selected();
            new PageGroup(qmTopItems[i].icon, STR_VAL(qmTopItems[i].title),
                          sub, k);
            return;
          }
        } else if (sub[j].pageAction == PAGE_CREATE) {
          k += 1;
        }
      }
    }
  }
}

EdgeTxIcon QuickMenu::subMenuIcon(QMPage page)
{
  for (int i = FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (qmTopItems[i].pageAction == QM_ACTION) {
      if (qmTopItems[i].qmPage == page) {
        return qmTopItems[i].icon;
        }
    } else {
      const PageDef* sub = qmTopItems[i].subMenuItems;
      for (int j = 0; sub[j].icon != EDGETX_ICONS_COUNT; j += 1) {
        if (sub[j].qmPage == page) {
          return qmTopItems[i].icon;
        }
      }
    }
  }
  return EDGETX_ICONS_COUNT;
}

int QuickMenu::pageIndex(QMPage page)
{
  for (int i = FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (qmTopItems[i].pageAction == QM_ACTION) {
      if (qmTopItems[i].qmPage == page) {
        return 0;
        }
    } else {
      const PageDef* sub = qmTopItems[i].subMenuItems;
      for (int j = 0, k = 0; sub[j].icon != EDGETX_ICONS_COUNT; j += 1) {
        if (sub[j].qmPage == page) {
          if (sub[j].pageAction == PAGE_CREATE)
            return k;
          else
            return 0;
        } else if (sub[j].pageAction == PAGE_CREATE) {
          k += 1;
        }
      }
    }
  }
  return 0;
}

std::string replaceAll(std::string str, const std::string& from, const std::string& to)
{
    auto&& pos = str.find(from, size_t{});
    while (pos != std::string::npos)
    {
        str.replace(pos, from.length(), to);
        // easy to forget to add to.length()
        pos = str.find(from, pos + to.length());
    }
    return str;
}

std::vector<std::string>& QuickMenu::menuPageNames(bool forFavorites)
{
  static std::vector<std::string> qmPages;

  qmPages.clear();
  qmPages.emplace_back(STR_NONE);
  qmPages.emplace_back(STR_OPEN_QUICK_MENU);

  for (int i = FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (qmTopItems[i].pageAction == QM_ACTION) {
      qmPages.emplace_back(STR_VAL(qmTopItems[i].title));
    } else {
      const PageDef* sub = qmTopItems[i].subMenuItems;
      for (int j = 0; sub[j].icon != EDGETX_ICONS_COUNT; j += 1) {
        std::string s(STR_VAL(qmTopItems[i].title));
        s += " - ";
        if (!forFavorites && sub[j].qmPage >= QM_UI_SCREEN1 && sub[j].qmPage <= QM_UI_SCREEN10)
          s += STR_CURRENT_SCREEN;
        else
          s += STR_VAL(sub[j].title);
        s = replaceAll(s, "\n", " ");
        qmPages.emplace_back(s);
      }
    }
  }

#if defined(LUA)
  getLuaToolNames(qmPages);
#endif

  return qmPages;
}

#if VERSION_MAJOR > 2
void QuickMenu::resetFavorites()
{
  if (instance)
    instance->updateFavorites();
}

void QuickMenu::updateFavorites()
{
  loadLuaTools();

  int f = 0;
  for (int i = 0; i < MAX_QM_FAVORITES; i += 1) {
    if (setupFavorite(i, f))
      f += 1;
  }
  favoritesMenuItems[f].icon = EDGETX_ICONS_COUNT;

  if (subMenus.size() >= 1)
    subMenus[0]->clearSubMenu();
}

bool QuickMenu::setupFavorite(int favIdx, int favBtn)
{
  auto page = g_eeGeneral.qmFavorites[favIdx].shortcut;
  PageDef& fav = favoritesMenuItems[favBtn];

  if (page == QM_NONE)
    return false;

  if (page == QM_APP) {
    std::string nm = g_eeGeneral.getFavoriteToolName(favIdx);
    int idx = getLuaToolId(nm);
    if (idx >= 0) {
      fav.icon = ICON_TOOLS_APPS;
      fav.qmTitle = nullptr;  // retreived on use (no translation strings)
      fav.title = nullptr;    // retrieved on use (no translation strings)
      fav.pageAction = PAGE_ACTION;
      fav.qmPage = (QMPage)(page + idx);
      fav.create = nullptr;
      fav.enabled = nullptr;
      fav.action = [=]() { runLuaTool(nm); };
      return true;
    }
    return false;
  }

  for (int i = FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    if (qmTopItems[i].pageAction == QM_ACTION) {
      if (qmTopItems[i].qmPage == page) {
          fav.icon = qmTopItems[i].icon;
          fav.qmTitle = qmTopItems[i].qmTitle;
          fav.title = qmTopItems[i].title;
          fav.pageAction = PAGE_ACTION;
          fav.qmPage = qmTopItems[i].qmPage;
          fav.create = nullptr;
          fav.enabled = nullptr;
          fav.action = qmTopItems[i].action;
        return true;
        }
    } else {
      const PageDef* sub = qmTopItems[i].subMenuItems;
      for (int j = 0; sub[j].icon != EDGETX_ICONS_COUNT; j += 1) {
        if (sub[j].qmPage == page) {
          fav.icon = sub[j].icon;
          fav.qmTitle = sub[j].qmTitle;
          fav.title = sub[j].title;
          fav.pageAction = sub[j].pageAction;
          fav.qmPage = sub[j].qmPage;
          fav.create = sub[j].create;
          fav.enabled = sub[j].enabled;
          fav.action = sub[j].action;
          return true;
        }
      }
    }
  }

  return false;
}
#endif

void QuickMenu::setCurrentPage(QMPage newPage, EdgeTxIcon newIcon)
{
  if (instance) {
    instance->curPage = newPage;
    instance->curIcon = newIcon;
  }
}

void QuickMenu::rememberVisitedPage(QMPage page, EdgeTxIcon groupIcon)
{
  const QMMainDef* section = findSectionForPage(page, groupIcon);
  if (!section || !sectionContainsPage(*section, page, true))
    return;

  QMPage entryPage = firstCreatePage(*section);
  if (canIndexLastVisited(entryPage))
    lastVisitedByEntryPage[entryPage] = page;
}

#if defined(SIMU)
bool quickMenuInvalidRememberedPageFallsBackForTest()
{
  for (int i = QuickMenu::FIRST_SEARCH_IDX;
       qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const QMMainDef& section = qmTopItems[i];
    if (section.pageAction != QM_SUBMENU)
      continue;

    QMPage entryPage = firstCreatePage(section);
    if (!canIndexLastVisited(entryPage))
      continue;

    const QMPage saved = lastVisitedByEntryPage[entryPage];
    lastVisitedByEntryPage[entryPage] = QM_APP;
    const bool fallsBack = restoreLastVisitedSectionPage(entryPage) == entryPage;
    lastVisitedByEntryPage[entryPage] = saved;
    return fallsBack;
  }

  return false;
}
#endif

void QuickMenu::focusMainMenu()
{
  inSubMenu = false;
  mainMenu->activate();
#if VERSION_MAJOR > 2
  for(auto sub : subMenus)
    sub->setDisabled(true);
#endif
}

void QuickMenu::onSelect(bool close)
{
  closeQM();
  Messaging::send(Messaging::QUICK_MENU_ITEM_SELECT, close);
}

void QuickMenu::closeQM()
{
  if (isVisible()) {
    popLayer();
    hide();
  }
}

void QuickMenu::onCancel()
{
  if (inSubMenu) {
    focusMainMenu();
    curPage = QM_NONE;
  } else {
    closeQM();
  }
}

void QuickMenu::setFocus(QMPage selection)
{
#if VERSION_MAJOR > 2
  for(auto sub : subMenus) {
    if (sub->isSubMenu(selection, curIcon)) {
      sub->setCurrent(selection);
      return;
    }
  }
#endif
}

void QuickMenu::enableSubMenu()
{
  inSubMenu = true;
}

#if defined(HARDWARE_KEYS)
void QuickMenu::doKeyShortcut(event_t event)
{
  QMPage pg = g_eeGeneral.getKeyShortcut(event);
  if (pg == QM_APP) {
    closeQM();
    runLuaTool(g_eeGeneral.getKeyToolName(event));
  } else if (pg == QM_OPEN_QUICK_MENU) {
    closeQM();
  } else {
    onSelect(true);
    QuickMenu::openPage(pg);
  }
}
void QuickMenu::onPressSYS() { doKeyShortcut(EVT_KEY_BREAK(KEY_SYS)); }
void QuickMenu::onLongPressSYS() { doKeyShortcut(EVT_KEY_LONG(KEY_SYS)); }
void QuickMenu::onPressMDL() { doKeyShortcut(EVT_KEY_BREAK(KEY_MODEL)); }
void QuickMenu::onLongPressMDL() { doKeyShortcut(EVT_KEY_LONG(KEY_MODEL)); }
void QuickMenu::onPressTELE() { doKeyShortcut(EVT_KEY_BREAK(KEY_TELE)); }
void QuickMenu::onLongPressTELE() { doKeyShortcut(EVT_KEY_LONG(KEY_TELE)); }
void QuickMenu::onLongPressRTN() { closeQM(); }

void QuickMenu::afterPG()
{
  auto b = mainMenu->getFocusedButton();
  if (b) {
#if VERSION_MAJOR > 2
    for(auto sub : subMenus) {
      if (sub->isSubMenu(b)) {
        sub->activate();
        return;
      }
    }
#endif
    focusMainMenu();
    curPage = QM_NONE;
  }
}

void QuickMenu::onPressPGDN()
{
  mainMenu->nextEntry();
  afterPG();
}

void QuickMenu::onPressPGUP()
{
  mainMenu->prevEntry();
  afterPG();
}
#endif

#if defined(SIMU)
std::string routeToStableId(const Route& route)
{
  if (!route.valid() || route.depth == 0)
    return {};

  const QMPage page = static_cast<QMPage>(route.pages[0]);
  const char* section = sectionToken(page);
  const char* pageName = pageToken(page);
  if (!section || !pageName || std::string(section) == "unknown")
    return {};

  std::string out = std::string(section) + "." + pageName;
  for (uint8_t depth = 1; depth < route.depth; depth += 1) {
    const char* token = editorToken(route.pages[depth]);
    if (token)
      out += "." + std::string(token);
    else
      out += ".seg" + std::to_string(route.pages[depth]);
    out += "[" + std::to_string(route.params[depth]) + "]";
  }
  return out;
}

std::string routeToTitle(const Route& route)
{
  if (!route.valid() || route.depth == 0)
    return {};

  const PageDef* pageDef = findPageDef(static_cast<QMPage>(route.pages[0]));
  if (!pageDef)
    return routeToStableId(route);

  std::string title = STR_VAL(pageDef->title);
  for (uint8_t depth = 1; depth < route.depth; depth += 1) {
    if (const char* token = editorToken(route.pages[depth])) {
      title += " / ";
      title += token;
      title += " ";
      title += std::to_string(route.params[depth]);
    }
  }
  return title;
}

bool routeIdToRoute(const std::string& routeId, Route& route, std::string* error)
{
  route = {};
  const PageDef* matchedPage = nullptr;
  std::string matchedBase;

  for (int i = QuickMenu::FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const QMMainDef& section = qmTopItems[i];
    if (section.pageAction != QM_SUBMENU || !section.subMenuItems)
      continue;

    for (int j = 0; section.subMenuItems[j].icon != EDGETX_ICONS_COUNT; j += 1) {
      const PageDef& pageDef = section.subMenuItems[j];
      if (pageDef.pageAction != PAGE_CREATE)
        continue;

      std::string base = routeToStableId(Route{section.icon, 1, {static_cast<uint8_t>(pageDef.qmPage)}, {0}});
      if (base.empty())
        continue;
      if (routeId == base || routeId.rfind(base + ".", 0) == 0) {
        if (base.size() > matchedBase.size()) {
          matchedBase = base;
          matchedPage = &pageDef;
        }
      }
    }
  }

  if (!matchedPage) {
    if (error) *error = "unknown route: " + routeId;
    return false;
  }

  const auto* section = findSectionForPage(matchedPage->qmPage);
  if (!section) {
    if (error) *error = "route section not found: " + routeId;
    return false;
  }

  route.rootIcon = section->icon;
  route.depth = 1;
  route.pages[0] = static_cast<uint8_t>(matchedPage->qmPage);

  size_t pos = matchedBase.size();
  while (pos < routeId.size()) {
    if (routeId[pos] != '.') {
      if (error) *error = "invalid route segment in: " + routeId;
      return false;
    }
    pos += 1;

    size_t bracket = routeId.find('[', pos);
    if (bracket == std::string::npos) {
      if (error) *error = "missing parameter in route segment: " + routeId;
      return false;
    }
    std::string token = routeId.substr(pos, bracket - pos);
    size_t endBracket = routeId.find(']', bracket + 1);
    if (endBracket == std::string::npos) {
      if (error) *error = "unterminated route parameter: " + routeId;
      return false;
    }
    std::string paramText = routeId.substr(bracket + 1, endBracket - bracket - 1);
    errno = 0;
    char* endPtr = nullptr;
    long parsed = strtol(paramText.c_str(), &endPtr, 10);
    if (errno != 0 || !endPtr || *endPtr != '\0' ||
        parsed < std::numeric_limits<int16_t>::min() ||
        parsed > std::numeric_limits<int16_t>::max()) {
      if (error) *error = "invalid route parameter: " + paramText;
      return false;
    }
    int param = static_cast<int>(parsed);

    uint8_t pageId = 0;
    if (!editorPageIdForToken(token, pageId)) {
      if (error) *error = "unknown route segment: " + token;
      return false;
    }
    if (route.depth >= Route::MAX_SEGMENTS) {
      if (error) *error = "route too deep: " + routeId;
      return false;
    }
    route.pages[route.depth] = pageId;
    route.params[route.depth] = param;
    route.depth += 1;
    pos = endBracket + 1;
  }

  return route.valid();
}

std::string routeSitemapJson()
{
  std::ostringstream out;
  out << "\"routes\":[";
  bool first = true;
  for (int i = QuickMenu::FIRST_SEARCH_IDX; qmTopItems[i].icon != EDGETX_ICONS_COUNT; i += 1) {
    const QMMainDef& section = qmTopItems[i];
    if (section.pageAction != QM_SUBMENU || !section.subMenuItems)
      continue;

    for (int j = 0; section.subMenuItems[j].icon != EDGETX_ICONS_COUNT; j += 1) {
      const PageDef& pageDef = section.subMenuItems[j];
      if (pageDef.pageAction != PAGE_CREATE || (pageDef.enabled && !pageDef.enabled()))
        continue;
      Route route;
      route.rootIcon = section.icon;
      route.depth = 1;
      route.pages[0] = static_cast<uint8_t>(pageDef.qmPage);
      if (!first) out << ",";
      first = false;
      out << "{\"id\":\"" << routeToStableId(route)
          << "\",\"label\":\"" << replaceAll(STR_VAL(pageDef.title), "\"", "\\\"")
          << "\",\"section\":\"" << sectionToken(pageDef.qmPage) << "\"}";
    }
  }
  out << "],\"templates\":[]";
  return out.str();
}
#endif
