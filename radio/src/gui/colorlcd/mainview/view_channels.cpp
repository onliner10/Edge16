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

#include "view_channels.h"

#include "channel_bar.h"
#include "model_select.h"
#include "edgetx.h"
#if VERSION_MAJOR == 2
#include "view_logical_switches.h"
#endif

#include <new>

// TODO: find better way to detect only used channels!
#define ALL_CHANNELS true

//-----------------------------------------------------------------------------

class ChannelsViewFooter : public Window
{
 public:
  explicit ChannelsViewFooter(Window* parent) :
      Window(parent, {0, parent->height() - FOOTER_H, LCD_W, FOOTER_H})
  {
    solidBg(COLOR_THEME_SECONDARY1_INDEX);

    auto w =
        new Window(this, {PAD_MEDIUM, PAD_SMALL, LEG_COLORBOX + PAD_TINY, LEG_COLORBOX + PAD_TINY}); // ds-allow: channel monitor footer legend swatch positioned absolutely; not a DS row
    w->setWindowFlag(NO_FOCUS);
    w->solidBg(COLOR_THEME_SECONDARY3_INDEX);
    w = new Window(w, {1, 1, LEG_COLORBOX, LEG_COLORBOX});
    w->setWindowFlag(NO_FOCUS);
    w->solidBg(COLOR_THEME_ACTIVE_INDEX);

    new StaticText(this, {LEG_COLORBOX + PAD_MEDIUM + PAD_SMALL, PAD_TINY, LV_SIZE_CONTENT, TXT_H}, STR_MONITOR_OUTPUT_DESC, COLOR_THEME_PRIMARY2_INDEX); // ds-allow: channel monitor footer legend label positioned absolutely beside its swatch; not a DS row

    int x = getTextWidth(STR_MONITOR_OUTPUT_DESC) + LEG_COLORBOX + PAD_MEDIUM * 2; // ds-allow: channel monitor footer computes the next legend item's absolute x offset; not a DS row

    w = new Window(this, {x + PAD_MEDIUM, PAD_SMALL, LEG_COLORBOX + PAD_TINY, LEG_COLORBOX + PAD_TINY}); // ds-allow: channel monitor footer second legend swatch positioned absolutely; not a DS row
    w->setWindowFlag(NO_FOCUS);
    w->solidBg(COLOR_THEME_SECONDARY3_INDEX);
    w = new Window(w, {1, 1, LEG_COLORBOX, LEG_COLORBOX});
    w->setWindowFlag(NO_FOCUS);
    w->solidBg(COLOR_THEME_FOCUS_INDEX);

    new StaticText(this, {x + LEG_COLORBOX + PAD_MEDIUM + PAD_SMALL, PAD_TINY, LV_SIZE_CONTENT, TXT_H}, // ds-allow: channel monitor footer second legend label positioned absolutely; not a DS row
                   STR_MONITOR_MIXER_DESC, COLOR_THEME_PRIMARY2_INDEX);
  }

  static LAYOUT_VAL_SCALED(LEG_COLORBOX, 14) // ds-allow: channel monitor footer legend swatch size constant; absolutely-laid-out legend, not a DS row
  static LAYOUT_VAL_SCALED(TXT_H, 18) // ds-allow: channel monitor footer legend label height constant; absolutely-laid-out legend, not a DS row
  static constexpr coord_t FOOTER_H = LEG_COLORBOX + PAD_SMALL * 2 + PAD_TINY; // ds-allow: channel monitor footer height constant for the absolutely-placed footer window; not a DS surface
};

//-----------------------------------------------------------------------------

class ChannelsViewPage : public PageGroupItem
{
 public:
  explicit ChannelsViewPage(uint8_t startChan, int rows, int cols, const char* title) :
      PageGroupItem(title), startChan(startChan), rows(rows), cols(cols)
  {
    icon = ICON_MONITOR;
  }

  static constexpr coord_t CHANS_H = 3 * ChannelBar::BAR_HEIGHT + PAD_THREE; // ds-allow: channel monitor group height constant for absolutely-stacked channel bars; not a DS list

 protected:
  uint8_t startChan;
  int rows;
  int cols;

  void build(Window* window) override
  {
    window->padAll(PAD_ZERO); // ds-allow: channel monitor zeroes page padding to place channel bars at absolute offsets; not a DS list

#if PORTRAIT
    coord_t w = window->width() - (PAD_SMALL * 2); // ds-allow: channel monitor computes bar column width inside its absolute layout; not a DS list
#else
    coord_t w = cols == 1 ? window->width() - (PAD_SMALL * 2) // ds-allow: channel monitor computes bar column width inside its absolute layout; not a DS list
                          : window->width() / 2 - (PAD_SMALL * 2); // ds-allow: channel monitor computes two-column bar width inside its absolute layout; not a DS list
#endif

    // Channels bars
    for (int i = 0, j = 0; j < rows * cols; i += 1) {
      int chan = startChan + i;
      if (chan >= MAX_OUTPUT_CHANNELS) break;
      if (ALL_CHANNELS || isChannelUsed(chan)) {
#if PORTRAIT
        coord_t xPos = PAD_SMALL; // ds-allow: channel monitor places each bar at an absolute x offset; not a DS list
        coord_t yPos = j * ((window->height() - PAD_LARGE * 3) / rows); // ds-allow: channel monitor places each bar at an absolute y offset across rows; not a DS list
#else
        coord_t xPos = cols == 1 ? PAD_SMALL // ds-allow: channel monitor places each bar at an absolute x offset per column; not a DS list
                                 : ((j & 1) ? w + (PAD_SMALL * 2) : PAD_SMALL); // ds-allow: channel monitor places each bar at an absolute x offset per column; not a DS list
        coord_t yPos = (j / cols) * ((window->height() - ChannelsViewFooter::FOOTER_H) / rows);
#endif
        auto channelBar = new (std::nothrow)
            ComboChannelBar(window, {xPos, yPos, w, CHANS_H}, uint8_t(chan));
        if (channelBar) j += 1;
      }
    }

    // Footer
    new ChannelsViewFooter(window);
  }
};

//-----------------------------------------------------------------------------

ChannelsViewMenu::ChannelsViewMenu() :
    TabsGroup(ICON_MONITOR, STR_MAIN_MENU_CHANNEL_MONITOR)
{
#if PORTRAIT
    int cols = 1;
    int rows = 8;
#else
#if LCD_W < 800
    int cols = 1;
    int rows = 4;
#else
    int cols = 2;
    int rows = 4;
#endif
#endif

  int pages = 0;
  int chansPerPage = rows * cols;

  char s[50];

  for (int i = 0; i < MAX_OUTPUT_CHANNELS;) {
    int start = i;
    while (!ALL_CHANNELS && !isChannelUsed(start)) {
      start += 1;
      if (start >= MAX_OUTPUT_CHANNELS) break;
    }
    if (start >= MAX_OUTPUT_CHANNELS) break;
    int count = 1;
    int last = start;
    int end = start + 1;
    while (end < MAX_OUTPUT_CHANNELS && count < chansPerPage) {
      if (ALL_CHANNELS || isChannelUsed(end)) {
        count += 1;
        last = end;
      }
      end += 1;
    }
    sprintf(s, STR_MONITOR_CHANNELS, start + 1, last + 1);
    auto page = new (std::nothrow) ChannelsViewPage(start, rows, cols, s);
    if (page) addTab(page);
    pages += 1;
    i = end;
  }

#if VERSION_MAJOR == 2
  addTab(new LogicalSwitchesViewPage());
#endif

  if (pages < 2) hidePageButtons();
}
