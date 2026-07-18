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

#include "topbar.h"

#include "edgetx.h"
#include "storage/storage.h"
#include "etx_lv_theme.h"
#include "topbar.h"
#include "view_main.h"
#include "widgets_setup.h"
#include "pagegroup.h"
#include "theme_manager.h"
#include "widget.h"

#include <new>
#include <utility>

//-----------------------------------------------------------------------------

namespace {

int moveDirectionOffset(WidgetMoveDirection direction)
{
  switch (direction) {
    case WidgetMoveDirection::Left:
      return -1;
    case WidgetMoveDirection::Right:
      return 1;
  }

  return 0;
}

}  // namespace

//-----------------------------------------------------------------------------

void TopBarPersistentData::clearZone(int idx)
{
  zones[idx].clear();
}

void TopBarPersistentData::clear()
{
  for (int i = 0; i < MAX_TOPBAR_ZONES; i += 1)
    clearZone(i);
}

const char* TopBarPersistentData::getWidgetName(int idx)
{
  return zones[idx].widgetName.c_str();
}

void TopBarPersistentData::setWidgetName(int idx, const char* s)
{
  zones[idx].widgetName = s;
}

WidgetPersistentData* TopBarPersistentData::getWidgetData(int idx)
{
  return &zones[idx].widgetData;
}

bool TopBarPersistentData::hasWidget(int idx)
{
  return !zones[idx].widgetName.empty();
}

//-----------------------------------------------------------------------------

SetupTopBarWidgetsPage::SetupTopBarWidgetsPage() :
    NavWindow(ViewMain::instance(), rect_t{})
{
  // remember focus
  pushLayer();

  auto viewMain = ViewMain::instance();

  // save current view & switch to 1st one
  viewMain->setCurrentMainView(0);

  // adopt the dimensions of the main view
  setRect(viewMain->getRect());

  auto topbar = viewMain->getTopbar();
  topbar->setSetupMode(true);
  refreshSlots(0);

#if defined(HARDWARE_TOUCH)
  addBackButton();
#endif
}

void SetupTopBarWidgetsPage::refreshSlots()
{
  auto viewMain = ViewMain::instance();
  if (!viewMain) return;

  auto topbar = viewMain->getTopbar();
  if (!topbar) return;

  topbar->updateZones();
  const unsigned int count = topbar->getZonesCount();
  for (unsigned int i = 0; i < MAX_TOPBAR_ZONES; i += 1) {
    if (i < count) {
      auto rect = topbar->getZone(i);
      if (slots[i]) {
        slots[i]->setSlotRect(rect);
      } else {
        slots[i] =
            new (std::nothrow) SetupWidgetsPageSlot(this, rect, topbar, i, this);
      }
    } else if (slots[i]) {
      slots[i]->deleteLater();
      slots[i] = nullptr;
    }
  }
}

void SetupTopBarWidgetsPage::refreshSlots(unsigned int preferredFocusIndex)
{
  refreshSlots();

  SetupWidgetsPageSlot* fallback = nullptr;
  for (unsigned int i = 0; i < MAX_TOPBAR_ZONES; i += 1) {
    if (!slots[i]) continue;
    if (i == preferredFocusIndex) {
      slots[i]->focus();
      return;
    }
    fallback = slots[i];
  }

  if (fallback) fallback->focus();
}

void SetupTopBarWidgetsPage::refreshSlots(const WidgetMoveResult& moveResult)
{
  refreshSlots(moveResult.to.asUnsigned());
}

void SetupTopBarWidgetsPage::onLiveClicked(Window::LiveWindow&)
{
  // block event forwarding (window is transparent)
}

void SetupTopBarWidgetsPage::onCancel() { deleteLater(); }

void SetupTopBarWidgetsPage::onDeleted()
{
  QuickMenu::openQuickMenu();

  auto viewMain = ViewMain::instance();
  if (viewMain && viewMain->getTopbar()) {
    viewMain->getTopbar()->setSetupMode(false);
    viewMain->getTopbar()->load();
  }

  storageDirty(EE_GENERAL);
}

//-----------------------------------------------------------------------------

TopBar::TopBar(Window * parent) :
  WidgetsContainer(parent, {0, 0, LCD_W, EdgeTxStyles::MENU_HEADER_HEIGHT}, MAX_TOPBAR_ZONES)
{
  setWindowFlag(NO_FOCUS);
  solidBg(COLOR_THEME_SECONDARY1_INDEX);

  headerIcon = new (std::nothrow) HeaderIcon(parent, ICON_EDGETX, [=]() { QuickMenu::openQuickMenu(); });
}

unsigned int TopBar::getZonesCount() const
{
  int last = -1;
  for (int i = 0; i < zoneCount; i += 1) {
    if (hasLayoutWidget(i)) last = i;
  }

  // Keep one empty setup slot after the last configured widget so the topbar
  // behaves like an ordered list instead of a fixed-width grid.
  unsigned int count = last < 0 ? 1 : last + 2;
  if (count > zoneCount) count = zoneCount;
  return count;
}

rect_t TopBar::getZone(unsigned int index) const
{
  if (index >= zoneCount) return rect_t{};

  const coord_t left = MENU_HEADER_BUTTONS_LEFT + 1;
  const coord_t right = LCD_W - PAD_TINY; // ds-allow: main-view top bar computes its right edge for zones positioned absolutely across the header; not a DS row
  const coord_t gap = TOPBAR_WIDGET_GAP;
  const int first = firstLayoutWidget();
  const bool layoutWidget = hasLayoutWidget(index);
  const int pendingSetupIndex =
      setupMode && !hasLayoutWidget(getZonesCount() - 1) ? getZonesCount() - 1
                                                         : -1;
  const bool pendingSetupSlot = !layoutWidget && (int)index == pendingSetupIndex;

  if (first < 0 || index == (unsigned int)first) {
    coord_t x = left;
    coord_t w = right - left;

    coord_t statusWidth = 0;
    unsigned int statusCount = 0;
    for (unsigned int i = (first < 0 ? 1 : first + 1); i < zoneCount; i += 1) {
      if (!hasLayoutWidget(i) && (int)i != pendingSetupIndex) continue;
      statusWidth += intrinsicZoneWidth(i);
      statusCount += 1;
    }

    if (statusCount > 0) statusWidth += statusCount * gap;
    if (statusWidth > 0 && w > statusWidth) w -= statusWidth;
    if (w < TOPBAR_FLEX_MIN_WIDTH) w = TOPBAR_FLEX_MIN_WIDTH;
    if (x + w > right) w = right - x;

    return {x, PAD_THREE, w, TOPBAR_ZONE_HEIGHT}; // ds-allow: main-view top bar returns the flex zone rect positioned absolutely across the header; not a DS row
  }

  if (!layoutWidget && !pendingSetupSlot) {
    return {left, PAD_THREE, TOPBAR_FLEX_MIN_WIDTH, TOPBAR_ZONE_HEIGHT}; // ds-allow: main-view top bar returns a header zone rect positioned absolutely; not a DS row
  }

  coord_t x = right;
  for (int i = zoneCount - 1; i > first; i -= 1) {
    bool useSlot = hasLayoutWidget(i) || i == pendingSetupIndex;
    if (!useSlot) continue;

    coord_t w = intrinsicZoneWidth(i);
    x -= w;
    if (i == (int)index) return {x, PAD_THREE, w, TOPBAR_ZONE_HEIGHT}; // ds-allow: main-view top bar returns a status-widget zone rect positioned absolutely across the header; not a DS row
    x -= gap;
  }

  return {left, PAD_THREE, TOPBAR_FLEX_MIN_WIDTH, TOPBAR_ZONE_HEIGHT}; // ds-allow: main-view top bar fallback header zone rect positioned absolutely; not a DS row
}

bool TopBar::hasLayoutWidget(unsigned int index) const
{
  if (index >= zoneCount) return false;
  return g_eeGeneral.getTopbarData()->hasWidget(index);
}

int TopBar::firstLayoutWidget() const
{
  for (int i = 0; i < zoneCount; i += 1) {
    if (hasLayoutWidget(i)) return i;
  }
  return -1;
}

coord_t TopBar::intrinsicZoneWidth(unsigned int index) const
{
  if (index >= zoneCount) return TOPBAR_STATUS_WIDTH;

  const char* name = g_eeGeneral.getTopbarData()->getWidgetName(index);
  if (!strcmp(name, "Date Time")) return TOPBAR_DATETIME_WIDTH;
  if (!strcmp(name, "Clock")) return TOPBAR_CLOCK_WIDTH;
  if (!strcmp(name, "Today")) return TOPBAR_TODAY_WIDTH;
  if (!strcmp(name, "Link")) return TOPBAR_LINK_WIDTH;
  if (!strcmp(name, "TX Battery")) return TOPBAR_BATTERY_WIDTH;
  if (!strcmp(name, "Battery Monitor")) return TOPBAR_STATUS_WIDTH;
  if (!strcmp(name, "Volume")) return TOPBAR_VOLUME_WIDTH;
  if (!strcmp(name, "Internal GPS")) return TOPBAR_GPS_WIDTH;
  if (!strcmp(name, "Radio Info")) return TOPBAR_LEGACY_STATUS_WIDTH;

  return TOPBAR_STATUS_WIDTH;
}

void TopBar::compactLayoutWidgets()
{
  auto topbarData = g_eeGeneral.getTopbarData();
  unsigned int writeIndex = 0;
  bool changed = false;

  for (unsigned int readIndex = 0; readIndex < zoneCount; readIndex += 1) {
    if (!topbarData->hasWidget(readIndex)) continue;

    if (readIndex != writeIndex) {
      topbarData->zones[writeIndex] = topbarData->zones[readIndex];
      topbarData->clearZone(readIndex);
      changed = true;
    }
    writeIndex += 1;
  }

  if (changed) storageDirty(EE_GENERAL);
}

void TopBar::setVisible(float visible) // 0.0 -> 1.0
{
  coord_t y = 0;
  if (visible == 0.0) {
    y = -EdgeTxStyles::MENU_HEADER_HEIGHT;
  } else if (visible > 0.0 && visible < 1.0){
    y = -(float)EdgeTxStyles::MENU_HEADER_HEIGHT * (1.0 - visible);
  }
  if (y != top()) setTop(y);
}

void TopBar::setEdgeTxButtonVisible(float visible) // 0.0 -> 1.0
{
  coord_t y = 0;
  if (visible == 0.0) {
    y = -EdgeTxStyles::MENU_HEADER_HEIGHT;
  } else if (visible > 0.0 && visible < 1.0){
    y = -(float)EdgeTxStyles::MENU_HEADER_HEIGHT * (1.0 - visible);
  }
  if (headerIcon && y != headerIcon->top()) headerIcon->setTop(y);
}

coord_t TopBar::getVisibleHeight(float visible) const // 0.0 -> 1.0
{
  if (visible == 0.0) {
    return 0;
  }
  else if (visible == 1.0) {
    return EdgeTxStyles::MENU_HEADER_HEIGHT;
  }

  float h = (float)EdgeTxStyles::MENU_HEADER_HEIGHT * visible;
  return (coord_t)h;
}

void TopBar::removeWidget(unsigned int index)
{
  if (index >= zoneCount) return;

  g_eeGeneral.getTopbarData()->clearZone(index);

  WidgetsContainer::removeWidget(index);
}

bool TopBar::canMoveWidget(WidgetSlotIndex slot,
                           WidgetMoveDirection direction) const
{
  unsigned int index = slot.asUnsigned();
  if (index >= zoneCount) return false;

  int offset = moveDirectionOffset(direction);
  if (offset == 0) return false;

  int targetIndex = (int)index + offset;
  if (targetIndex < 0 || targetIndex >= zoneCount) return false;

  auto topbarData = g_eeGeneral.getTopbarData();
  return topbarData->hasWidget(index) && topbarData->hasWidget(targetIndex);
}

WidgetMoveResult TopBar::moveWidget(WidgetSlotIndex slot,
                                    WidgetMoveDirection direction)
{
  if (!widgets || !canMoveWidget(slot, direction))
    return {slot, slot};

  auto topbarData = g_eeGeneral.getTopbarData();
  unsigned int index = slot.asUnsigned();
  int targetIndex = (int)index + moveDirectionOffset(direction);
  WidgetSlotIndex targetSlot{static_cast<uint8_t>(targetIndex)};
  std::swap(topbarData->zones[index], topbarData->zones[targetIndex]);

  storageDirty(EE_GENERAL);
  load();
  return {slot, targetSlot};
}

void TopBar::load()
{
  if (!widgets) return;
  compactLayoutWidgets();

  for (unsigned int i = 0; i < zoneCount; i++) {
    // remove old widget
    if (widgets[i]) {
      widgets[i]->deleteLater();
      widgets[i] = nullptr;
    }
  }

  for (unsigned int i = 0; i < zoneCount; i++) {
    // and load new one if required
    if (g_eeGeneral.getTopbarData()->hasWidget(i)) {
      widgets[i] = WidgetFactory::newWidget(
          g_eeGeneral.getTopbarData()->getWidgetName(i), this, getZone(i),
          WidgetLocation(TopBarWidgetLocation{static_cast<uint8_t>(i)}));
    }
  }
}

Widget* TopBar::createWidget(unsigned int index,
                      const WidgetFactory* factory)
{
  if (!widgets || index >= zoneCount) return nullptr;

  // remove old one if existing
  removeWidget(index);

  Widget* widget = nullptr;
  if (factory) {
    g_eeGeneral.getTopbarData()->setWidgetName(index, factory->getName());
    widget = factory->create(
        this, getZone(index),
        WidgetLocation(TopBarWidgetLocation{static_cast<uint8_t>(index)}));
  }
  widgets[index] = widget;
  updateZones();
  storageDirty(EE_GENERAL);

  return widget;
}

//-----------------------------------------------------------------------------
