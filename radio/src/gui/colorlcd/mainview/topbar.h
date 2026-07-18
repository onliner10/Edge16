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

#include "layout.h"

class HeaderIcon;
class SetupWidgetsPageSlot;

//-----------------------------------------------------------------------------

class SetupTopBarWidgetsPage : public NavWindow
{
 public:
  explicit SetupTopBarWidgetsPage();

#if defined(DEBUG_WINDOWS)
  std::string getName() const override { return "SetupTopBarWidgetsPage"; }
#endif

  void onLiveClicked(LiveWindow&) override;
  void onCancel() override;
  void onDeleted() override;
  void refreshSlots();
  void refreshSlots(unsigned int preferredFocusIndex);
  void refreshSlots(const WidgetMoveResult& moveResult);

#if defined(HARDWARE_KEYS)
  void onPressSYS() override {}
  void onLongPressSYS() override {}
  void onPressMDL() override {}
  void onLongPressMDL() override {}
  void onPressPGUP() override {}
  void onPressPGDN() override {}
  void onLongPressPGUP() override {}
  void onLongPressPGDN() override {}
  void onPressTELE() override { onCancel(); }
#endif

 protected:
  SetupWidgetsPageSlot* slots[MAX_TOPBAR_ZONES] = {};
};

//-----------------------------------------------------------------------------

class TopBar: public WidgetsContainer
{
 public:
  explicit TopBar(Window * parent);

#if defined(DEBUG_WINDOWS)
  std::string getName() const override
  {
    return "TopBar";
  }
#endif

  unsigned int getZonesCount() const override;

  rect_t getZone(unsigned int index) const override;

  void setVisible(float visible);
  void setEdgeTxButtonVisible(float visible);
  coord_t getVisibleHeight(float visible) const; // 0.0 -> 1.0
  void setSetupMode(bool enable) { setupMode = enable; }

  bool isTopBar() override { return true; }

  void removeWidget(unsigned int index) override;
  bool canMoveWidget(WidgetSlotIndex index,
                     WidgetMoveDirection direction) const override;
  [[nodiscard]] WidgetMoveResult moveWidget(WidgetSlotIndex index,
                                            WidgetMoveDirection direction) override;

  Widget* createWidget(unsigned int index, const WidgetFactory* factory) override;

  void load();

  static LAYOUT_VAL_SCALED(HDR_DATE_XO, 48) // ds-allow: main-view top bar date x-offset constant for content placed absolutely within the header; not a DS row

  static constexpr coord_t TOPBAR_ZONE_HEIGHT = EdgeTxStyles::MENU_HEADER_HEIGHT - 2 * PAD_THREE; // ds-allow: main-view top bar zone height for absolutely-positioned header zones; not a DS row
  static constexpr coord_t TOPBAR_WIDGET_GAP = PAD_TINY; // ds-allow: main-view top bar inter-zone gap used in absolute zone-rect math; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_FLEX_MIN_WIDTH, 96) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_STATUS_WIDTH, 50) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_LINK_WIDTH, 48) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_BATTERY_WIDTH, 50) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_VOLUME_WIDTH, 56) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_DATETIME_WIDTH, 50) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_CLOCK_WIDTH, 64) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_TODAY_WIDTH, 52) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_GPS_WIDTH, 38) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row
  static LAYOUT_VAL_SCALED(TOPBAR_LEGACY_STATUS_WIDTH, 74) // ds-allow: main-view top bar zone width constant for absolutely-positioned header zones; not a DS row

 protected:
  uint32_t lastRefresh = 0;
  HeaderIcon* headerIcon = nullptr;
  bool setupMode = false;

  bool hasLayoutWidget(unsigned int index) const;
  int firstLayoutWidget() const;
  coord_t intrinsicZoneWidth(unsigned int index) const;
  void compactLayoutWidgets();
};

//-----------------------------------------------------------------------------
