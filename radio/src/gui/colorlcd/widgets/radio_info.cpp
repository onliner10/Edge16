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

#include <stdio.h>

#include <new>

#include "edgetx.h"
#include "hal/usb_driver.h"
#include "layout.h"
#include "theme_manager.h"
#include "widget.h"
#include "widget_palette.h"

namespace
{

constexpr uint8_t LINK_BARS = 5;
const uint8_t linkBarThresholds[LINK_BARS] = {30, 40, 50, 60, 80};

struct StatusContentBox {
  coord_t x;
  coord_t y;
  coord_t w;
  coord_t h;
};

coord_t minCoord(coord_t a, coord_t b) { return a < b ? a : b; }
coord_t maxCoord(coord_t a, coord_t b) { return a > b ? a : b; }

coord_t clampCoord(coord_t value, coord_t low, coord_t high)
{
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

FontIndex textFontForBox(const char* text, coord_t width, coord_t height)
{
  for (auto font : kAutoFitFontLadder) {
    LcdFlags flags = LcdFlags(font) << 8u;
    if (getFontHeight(flags) <= height &&
        getTextWidth(text, 0, flags) <= width) {
      return font;
    }
  }

  return FONT_XXS_INDEX;
}

FontIndex chipTextFontForBox(const char* text, coord_t width, coord_t height)
{
  static const FontIndex candidates[] = {FONT_BOLD_INDEX, FONT_STD_INDEX,
                                         FONT_XS_INDEX, FONT_XXS_INDEX};

  for (auto font : candidates) {
    LcdFlags flags = LcdFlags(font) << 8u;
    if (getFontHeight(flags) <= height &&
        getTextWidth(text, 0, flags) <= width) {
      return font;
    }
  }

  return FONT_XXS_INDEX;
}

constexpr coord_t TOPBAR_CONTENT_PAD = PAD_TINY; // ds-allow: radio-info status widgets — shared inner inset for top-bar content boxes; canvas widgets positioning elements at pixel offsets, not a DS row/form.

StatusContentBox topbarContentBox(coord_t w, coord_t h)
{
  coord_t contentW = w > 2 * TOPBAR_CONTENT_PAD ? w - 2 * TOPBAR_CONTENT_PAD
                                                : maxCoord(w, (coord_t)1);
  coord_t contentH = h > 2 * TOPBAR_CONTENT_PAD ? h - 2 * TOPBAR_CONTENT_PAD
                                                : maxCoord(h, (coord_t)1);
  return {TOPBAR_CONTENT_PAD, TOPBAR_CONTENT_PAD, contentW, contentH};
}

lv_obj_t* makeStatusPart(lv_obj_t* parent)
{
  auto obj = lv_obj_create(parent);
  if (!obj) return nullptr;

  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  return obj;
}

template <typename Fn>
void withStatusPart(lv_obj_t* obj, Fn&& fn)
{
  if (obj) fn(obj);
}

LcdColorIndex statusPrimaryColor(bool topbar)
{
  return topbar ? COLOR_THEME_PRIMARY2_INDEX : COLOR_THEME_SECONDARY1_INDEX;
}

LcdColorIndex statusMutedColor(bool topbar)
{
  return topbar ? COLOR_THEME_SECONDARY2_INDEX : COLOR_THEME_SECONDARY2_INDEX;
}

void setStatusPartColor(lv_obj_t* obj, LcdColorIndex color)
{
  withStatusPart(obj, [&](lv_obj_t* part) { etx_solid_bg(part, color); });
}

void setStatusPartBorder(lv_obj_t* obj, LcdColorIndex color, coord_t width)
{
  withStatusPart(obj, [&](lv_obj_t* part) {
    etx_border_color(part, color);
    lv_obj_set_style_border_width(part, width, LV_PART_MAIN);
  });
}

// lv_color_t variants: state-aware colours come out of widget_palette.h as
// runtime-computed (contrast-corrected) lv_color_t values, not fixed
// LcdColorIndex table entries, so they cannot go through etx_solid_bg /
// etx_border_color (which add a theme-indexed style object). Mirrors
// etx_bg_color_from_flags' RGB_FLAG path: drop any previously-added indexed
// style, then set the raw colour as a local style property.
lv_color_t lvColorFromIndex(LcdColorIndex color)
{
  return makeLvColor(COLOR(color));
}

void setStatusPartColorLv(lv_obj_t* obj, lv_color_t color)
{
  withStatusPart(obj, [&](lv_obj_t* part) {
    etx_remove_bg_color(part);
    lv_obj_set_style_bg_color(part, color, LV_PART_MAIN);
  });
}

void setStatusPartBorderLv(lv_obj_t* obj, lv_color_t color, coord_t width)
{
  withStatusPart(obj, [&](lv_obj_t* part) {
    etx_remove_border_color(part);
    lv_obj_set_style_border_color(part, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(part, width, LV_PART_MAIN);
  });
}

void setStatusLabel(lv_obj_t* label, const char* text, LcdColorIndex color,
                    FontIndex font, coord_t x, coord_t y, coord_t w, coord_t h)
{
  withStatusPart(label, [&](lv_obj_t* label) {
    etx_font(label, font);
    etx_txt_color(label, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(label, x, y); // ds-allow: radio-info status helper — places a status label at an absolute pixel offset computed by the widget; canvas widget, not a DS row/form.
    lv_obj_set_size(label, w, h);
  });
}

void setLvVisible(lv_obj_t* obj, bool visible)
{
  withStatusPart(obj, [&](lv_obj_t* part) {
    if (visible)
      lv_obj_clear_flag(part, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
  });
}

uint8_t activeLinkBars(uint8_t rssi)
{
  uint8_t bars = 0;
  for (uint8_t i = 0; i < LINK_BARS; i += 1) {
    if (rssi >= linkBarThresholds[i]) bars = i + 1;
  }
  return bars;
}

// Two-tier RSSI escalation: Critical below rfAlarms.critical, Warning below
// rfAlarms.warning, matching the audio-alarm bands in telemetry.cpp
// (genericRssiCritical/genericRssiWarning). rssi == 0 means "no reading yet",
// never a false Critical/Warning.
uint8_t linkStateLevel(uint8_t rssi)
{
  if (rssi == 0) return WIDGET_STATE_NORMAL;
  if (rssi < (uint8_t)g_model.rfAlarms.critical) return WIDGET_STATE_CRITICAL;
  if (rssi < (uint8_t)g_model.rfAlarms.warning) return WIDGET_STATE_WARNING;
  return WIDGET_STATE_NORMAL;
}

uint8_t speakerVolumeLevel()
{
#if defined(AUDIO)
  if (requiredSpeakerVolume == 0 || g_eeGeneral.beepMode == e_mode_quiet)
    return 0;
  if (requiredSpeakerVolume < 7) return 1;
  if (requiredSpeakerVolume < 13) return 2;
  if (requiredSpeakerVolume < 19) return 3;
  return 4;
#else
  return 0;
#endif
}

uint8_t speakerVolumePercent()
{
#if defined(AUDIO)
  if (g_eeGeneral.beepMode == e_mode_quiet) return 0;
  return divRoundClosest((uint16_t)requiredSpeakerVolume * 100,
                         VOLUME_LEVEL_MAX);
#else
  return 0;
#endif
}

// Shared single-line date/time text formatting, used by DateTextWidget
// (Clock / Today factories) and by DateTimeWidget's Time-only / Date-only
// Format option so the two never drift apart.
enum class DateTextKind { Clock, Today };

// DateTimeWidget's appended Format option (Time / Date / Both). Plain
// unsigned values (not an enum class) since it is read straight out of the
// persisted WidgetOptionValue's unsignedValue field, matching the convention
// used elsewhere in this file (e.g. mixsrc_t option reads).
enum DateTimeFormat : uint8_t {
  DATETIME_FORMAT_BOTH = 0,
  DATETIME_FORMAT_TIME = 1,
  DATETIME_FORMAT_DATE = 2,
};

void formatDateText(char* text, size_t size, const struct gtm& time,
                    DateTextKind kind)
{
  if (kind == DateTextKind::Clock) {
    const TimerOptions timerOptions = {.options = SHOW_TIME};
    getTimerString(text, getValue(MIXSRC_TX_TIME), timerOptions);
    return;
  }

#if defined(TRANSLATIONS_CN) || defined(TRANSLATIONS_TW)
  snprintf(text, size, "%02d-%02d", time.tm_mon + 1, time.tm_mday);
#else
  snprintf(text, size, "%d %s", time.tm_mday, STR_MONTHS[time.tm_mon]);
#endif
}

bool dateTextShouldRefresh(DateTextKind kind, const struct gtm& time,
                           const struct gtm& lastTime)
{
  if (kind == DateTextKind::Clock) {
    return time.tm_min != lastTime.tm_min || time.tm_hour != lastTime.tm_hour;
  }

  return time.tm_mday != lastTime.tm_mday || time.tm_mon != lastTime.tm_mon ||
         time.tm_year != lastTime.tm_year;
}

}  // namespace

class LinkStatusWidget : public Widget
{
 public:
  LinkStatusWidget(const WidgetFactory* factory, Window* parent,
                   const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    withLive([&](LiveWindow& live) {
      for (uint8_t i = 0; i < LINK_BARS; i += 1) {
        bars[i] = makeStatusPart(live.lvobj());
        if (bars[i]) lv_obj_set_style_radius(bars[i], 1, LV_PART_MAIN);
      }

      title = etx_label_create(live.lvobj(), FONT_XXS_INDEX);
      value = etx_label_create(live.lvobj(), FONT_BOLD_INDEX);
    });

    update();
    foreground();
  }

  void onUpdate() override
  {
    const bool topbar = isCompactTopBarWidget();
    const coord_t pad = topbar ? TOPBAR_CONTENT_PAD : PAD_SMALL; // ds-allow: RSSI link-bars widget — content inset chosen by top-bar vs main-view mode; pixel geometry, not a DS row/form.
    const coord_t labelW = width() > 2 * pad ? width() - 2 * pad : width();

    setLvVisible(title, !topbar && height() >= 54);
    setLvVisible(value, !topbar);

    coord_t graphX = pad;
    coord_t graphY = pad;
    coord_t graphW = width() > 2 * pad ? width() - 2 * pad : width();
    coord_t graphH = height() > 2 * pad ? height() - 2 * pad : height();

    if (topbar) {
      auto box = topbarContentBox(width(), height());
      graphX = box.x;
      graphY = box.y;
      graphW = box.w;
      graphH = box.h;
    } else {
      coord_t textW = maxCoord(labelW / 3, (coord_t)34);
      if (width() > 110) {
        graphW = minCoord((coord_t)(width() - textW - 3 * pad), (coord_t)58);
      } else {
        graphW = minCoord(graphW, (coord_t)58);
      }
      graphH = minCoord(graphH, height() >= 72 ? (coord_t)42 : (coord_t)32);
      graphY = (height() - graphH) / 2;

      coord_t textX = graphX + graphW + pad;
      coord_t textRight = width() > pad ? width() - pad : width();
      if (textX + 22 < textRight) {
        coord_t textAreaW = textRight - textX;
        coord_t titleH = height() >= 54 ? EdgeTxStyles::STD_FONT_HEIGHT / 2 : 0;
        FontIndex valueFont = responsiveTextFont(height() - 2 * pad - titleH);
        coord_t valueH = getFontHeight(LcdFlags(valueFont) << 8u);
        coord_t textBlockH = titleH + valueH;
        coord_t textY = (height() - textBlockH) / 2;

        setStatusLabel(title, "LINK", COLOR_THEME_PRIMARY1_INDEX,
                       FONT_XXS_INDEX, textX, textY, textAreaW, titleH);
        setStatusLabel(value, "--", statusPrimaryColor(false), valueFont, textX,
                       textY + titleH, textAreaW, valueH);
      } else {
        setLvVisible(title, false);
        setLvVisible(value, false);
      }
    }

    coord_t gap = topbar ? TOPBAR_CONTENT_PAD : PAD_THREE; // ds-allow: RSSI link-bars widget — inter-bar gap sized to top-bar vs main-view; pixel geometry, not a DS row/form.
    coord_t barW = (graphW - (LINK_BARS - 1) * gap) / LINK_BARS;
    if (topbar)
      barW = maxCoord(barW, (coord_t)1);
    else
      barW = clampCoord(barW, (coord_t)6, (coord_t)14);

    for (uint8_t i = 0; i < LINK_BARS; i += 1) {
      if (!bars[i]) continue;
      coord_t barH = ((i + 1) * graphH + LINK_BARS - 1) / LINK_BARS;
      if (i == 0) barH = maxCoord(barH, topbar ? (coord_t)8 : (coord_t)9);
      lv_obj_set_pos(bars[i], graphX + i * (barW + gap), // ds-allow: RSSI link-bars widget — each signal bar positioned at an absolute offset within the zone; canvas widget, not a DS row/form.
                     graphY + graphH - barH);
      lv_obj_set_size(bars[i], barW, barH);
    }

    lastRSSI = 255;
  }

  void onForeground() override
  {
    uint8_t rssi = TELEMETRY_RSSI();
    if (rssi == lastRSSI) return;
    lastRSSI = rssi;

    bool topbar = isCompactTopBarWidget();
    uint8_t level = linkStateLevel(rssi);
    uint8_t active = activeLinkBars(rssi);
    // Normal keeps the existing per-context primary colour; Warning/Critical
    // escalate to the contrast-validated palette amber/red.
    lv_color_t activeColor =
        level == WIDGET_STATE_NORMAL
            ? lvColorFromIndex(statusPrimaryColor(topbar))
            : (topbar ? paletteTopbarStateTextColor(level)
                     : paletteStateTextColor(level));
    lv_color_t mutedColor = lvColorFromIndex(statusMutedColor(topbar));

    for (uint8_t i = 0; i < LINK_BARS; i += 1) {
      setStatusPartColorLv(bars[i], i < active ? activeColor : mutedColor);
    }

    if (value && !topbar) {
      char text[8];
      if (rssi == 0)
        snprintf(text, sizeof(text), "--");
      else
        snprintf(text, sizeof(text), "%u", rssi);

      FontIndex font = responsiveTextFont(height() - 2 * PAD_SMALL - // ds-allow: RSSI link-bars widget — value font sized to the zone height minus vertical insets; pixel geometry, not a DS row/form.
                                          EdgeTxStyles::STD_FONT_HEIGHT / 2);
      setStatusLabel(value, text, statusPrimaryColor(false), font,
                     lv_obj_get_x(value), lv_obj_get_y(value),
                     lv_obj_get_width(value), lv_obj_get_height(value));
      lv_color_t valueColor = level == WIDGET_STATE_NORMAL
                                  ? lvColorFromIndex(statusPrimaryColor(false))
                                  : paletteStateTextColor(level);
      lv_obj_set_style_text_color(value, valueColor, LV_PART_MAIN);
    }
  }

 protected:
  lv_obj_t* bars[LINK_BARS] = {};
  lv_obj_t* title = nullptr;
  lv_obj_t* value = nullptr;
  uint8_t lastRSSI = 255;
};

// Persist-name ("Link") kept unchanged for backward compat with saved
// layouts; only the display name changes (it shows receiver RSSI/link, and
// "Link" alone was ambiguous).
BaseWidgetFactory<LinkStatusWidget> linkStatusWidget("Link", nullptr,
                                                     "Signal");

class TxBatteryStatusWidget : public Widget
{
 public:
  TxBatteryStatusWidget(const WidgetFactory* factory, Window* parent,
                        const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    withLive([&](LiveWindow& live) {
      shell = makeStatusPart(live.lvobj());
      fill = makeStatusPart(live.lvobj());
      cap = makeStatusPart(live.lvobj());
      title = etx_label_create(live.lvobj(), FONT_XXS_INDEX);
      value = etx_label_create(live.lvobj(), FONT_BOLD_INDEX);
      pctLabel = etx_label_create(live.lvobj(), FONT_BOLD_INDEX);
    });

    if (shell) {
      lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_radius(shell, 3, LV_PART_MAIN);
    }
    if (fill) lv_obj_set_style_radius(fill, 2, LV_PART_MAIN);
    if (cap) lv_obj_set_style_radius(cap, 1, LV_PART_MAIN);
    if (pctLabel) {
      lv_label_set_text(pctLabel, "");
      lv_obj_set_style_text_align(pctLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    update();
    foreground();
  }

  void onUpdate() override
  {
    const bool topbar = isCompactTopBarWidget();
    const coord_t pad = topbar ? TOPBAR_CONTENT_PAD : PAD_SMALL; // ds-allow: TX-battery HUD widget — content inset chosen by top-bar vs main-view mode; pixel geometry, not a DS row/form.

    setLvVisible(title, false);
    setLvVisible(value, false);

    coord_t capW = PAD_THREE; // ds-allow: TX-battery HUD widget — battery terminal-cap width default; pixel geometry, not a DS row/form.
    coord_t battW = 0;
    coord_t battH = 0;
    coord_t battX = pad;
    coord_t battY = 0;

    if (topbar) {
      auto box = topbarContentBox(width(), height());
      capW = minCoord(PAD_THREE, maxCoord((coord_t)(box.w / 8), (coord_t)1)); // ds-allow: TX-battery HUD widget — cap width clamped to a fraction of the top-bar box; pixel geometry, not a DS row/form.
      battW = maxCoord((coord_t)(box.w - capW), (coord_t)1);
      battH = minCoord(box.h, maxCoord((coord_t)(battW / 2), (coord_t)1));
      battX = box.x;
      battY = box.y + (box.h - battH) / 2;
    } else {
      battH = minCoord((coord_t)30, height() - 2 * pad);
      battH = maxCoord(battH, (coord_t)18);
      battW = minCoord((coord_t)58, width() - 3 * pad);
      battW = maxCoord(battW, (coord_t)36);
      battY = (height() - battH) / 2;

      if (width() < 110) {
        battW = minCoord(battW, (coord_t)(width() - 3 * pad));
      }
    }

    if (shell) {
      lv_obj_set_pos(shell, battX, battY); // ds-allow: TX-battery HUD widget — battery shell positioned at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(shell, battW, battH);
      setStatusPartBorder(shell, statusPrimaryColor(topbar), 2);
    }
    if (cap) {
      lv_obj_set_pos(cap, battX + battW, battY + battH / 4); // ds-allow: TX-battery HUD widget — terminal cap positioned against the shell at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(cap, capW, battH / 2);
    }

    coord_t fillInset = PAD_THREE; // ds-allow: TX-battery HUD widget — inset of the fill inside the battery shell; pixel geometry, not a DS row/form.
    coord_t fillX = battX + fillInset;
    coord_t fillY = battY + fillInset;
    fillMaxW = battW > 2 * fillInset ? battW - 2 * fillInset : battW;
    fillH = battH > 2 * fillInset ? battH - 2 * fillInset : battH;

    if (fill) {
      lv_obj_set_pos(fill, fillX, fillY); // ds-allow: TX-battery HUD widget — fill positioned inside the shell at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(fill, fillMaxW, fillH);
    }

    this->battX = battX;
    this->battY = battY;
    this->battW = battW;
    this->battH = battH;

    if (pctLabel) {
      lv_obj_set_size(pctLabel, battW, battH);
      setLvVisible(pctLabel, true);
    }

    lastBattBars = 255;
    lastBattPct = 255;
  }

  void onForeground() override
  {
    bool topbar = isCompactTopBarWidget();
    // Single source of truth: the same Warning/Critical decision that drives
    // the audio alarm (checkBatteryAlarms) and the state-aware Value widget.
    // TXBATT_ALARM_NONE/WARNING/CRITICAL (0/1/2) matches WidgetStateLevel, so
    // it feeds the palette state-colour functions directly.
    uint8_t level = getTxBatteryAlarm();
    uint8_t bars = GET_TXBATT_BARS(fillMaxW);
    lv_color_t color = topbar ? paletteTopbarStateTextColor(level)
                              : paletteStateTextColor(level);

    setStatusPartBorderLv(shell, color, 2);
    setStatusPartColorLv(cap, color);

    if (bars != lastBattBars) {
      lastBattBars = bars;
      if (fill) {
        lv_obj_set_size(fill, bars, fillH);
      }
    }
    setStatusPartColorLv(fill, color);

    if (pctLabel) {
      uint8_t pct = txBatteryPercent(g_vbat100mV);
      if (pct != lastBattPct) {
        lastBattPct = pct;
        char text[4];
        snprintf(text, sizeof(text), "%u", pct);
        lv_label_set_text(pctLabel, text);
        FontIndex font = chipTextFontForBox(text, battW - 4, battH - 4);
        etx_font(pctLabel, font);
        coord_t fh = getFontHeight(LcdFlags(font) << 8u);
        lv_obj_set_pos(pctLabel, battX, battY + (battH - fh) / 2); // ds-allow: TX-battery HUD widget — percent label centred over the shell at an absolute offset; canvas widget, not a DS row/form.
        lv_obj_set_size(pctLabel, battW, fh);
      }
      etx_txt_color(pctLabel, COLOR_BLACK_INDEX);
    }
  }

 protected:
  lv_obj_t* shell = nullptr;
  lv_obj_t* fill = nullptr;
  lv_obj_t* cap = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* value = nullptr;
  lv_obj_t* pctLabel = nullptr;
  coord_t fillMaxW = 1;
  coord_t fillH = 1;
  coord_t battX = 0;
  coord_t battY = 0;
  coord_t battW = 1;
  coord_t battH = 1;
  uint8_t lastBattBars = 255;
  uint8_t lastBattPct = 255;
};

// Persist-name ("TX Battery") kept unchanged for backward compat with saved
// layouts; only the display name changes.
BaseWidgetFactory<TxBatteryStatusWidget> txBatteryStatusWidget("TX Battery",
                                                               nullptr,
                                                               "Radio Battery");

#if defined(AUDIO)

class VolumeStatusWidget : public Widget
{
 public:
  VolumeStatusWidget(const WidgetFactory* factory, Window* parent,
                     const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    withLive([&](LiveWindow& live) {
      track = makeStatusPart(live.lvobj());
      fill = makeStatusPart(live.lvobj());
    });
    if (track) lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    if (fill) lv_obj_set_style_radius(fill, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    withLive([&](LiveWindow& live) {
      capsuleLabel = etx_label_create(live.lvobj(), FONT_BOLD_INDEX);
    });
    if (capsuleLabel) lv_label_set_long_mode(capsuleLabel, LV_LABEL_LONG_DOT);

    withLive([&](LiveWindow& live) {
      title = etx_label_create(live.lvobj(), FONT_XXS_INDEX);
      value = etx_label_create(live.lvobj(), FONT_BOLD_INDEX);
    });

    update();
    foreground();
  }

  void onUpdate() override
  {
    const bool topbar = isCompactTopBarWidget();
    const coord_t pad = topbar ? TOPBAR_CONTENT_PAD : PAD_SMALL; // ds-allow: volume capsule widget — content inset chosen by top-bar vs main-view mode; pixel geometry, not a DS row/form.

    setLvVisible(title, !topbar && height() >= 54);
    setLvVisible(value, !topbar);
    setLvVisible(capsuleLabel, topbar);
    setLvVisible(track, true);
    setLvVisible(fill, true);

    StatusContentBox box =
        topbar ? topbarContentBox(width(), height())
               : StatusContentBox{
                     pad, pad, width() > 2 * pad ? width() - 2 * pad : width(),
                     height() > 2 * pad ? height() - 2 * pad : height()};

    coord_t trackX = box.x;
    coord_t trackRight = box.x + box.w;
    coord_t textX = 0;

    if (!topbar && width() > 110) {
      coord_t textW = maxCoord((coord_t)(box.w / 3), (coord_t)38);
      trackRight = width() > pad + textW + PAD_SMALL // ds-allow: volume capsule widget — track right edge leaves room for the value text plus small gaps; pixel geometry, not a DS row/form.
                       ? width() - pad - textW - PAD_SMALL // ds-allow: volume capsule widget — track right edge computed from zone width minus text and small gaps; pixel geometry, not a DS row/form.
                       : trackRight;
      textX = trackRight + PAD_SMALL; // ds-allow: volume capsule widget — value text X placed a small gap right of the track; pixel geometry, not a DS row/form.
    }

    trackMaxW = trackRight > trackX ? trackRight - trackX : 1;
    trackH = topbar ? clampCoord((coord_t)(box.h / 4), PAD_THREE, PAD_LARGE) // ds-allow: volume capsule widget — track height clamped to token-derived bounds; pixel geometry, not a DS row/form.
                    : clampCoord((coord_t)(box.h / 5), PAD_LARGE, (coord_t)18); // ds-allow: volume capsule widget — main-view track height clamped to token-derived bounds; pixel geometry, not a DS row/form.
    coord_t trackY =
        topbar ? box.y + box.h - trackH : box.y + (box.h - trackH) / 2;

    if (track) {
      lv_obj_set_pos(track, trackX, trackY); // ds-allow: volume capsule widget — track positioned at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(track, trackMaxW, trackH);
    }

    coord_t fillX = trackX;
    coord_t fillY = trackY;
    fillMaxW = trackMaxW;
    fillH = trackH;

    if (fill) {
      lv_obj_set_pos(fill, fillX, fillY); // ds-allow: volume capsule widget — fill positioned over the track at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(fill, fillMaxW, fillH);
    }
    if (topbar && capsuleLabel) {
      coord_t textW =
          trackMaxW > 2 * PAD_TINY ? trackMaxW - 2 * PAD_TINY : trackMaxW; // ds-allow: volume capsule widget — capsule label width inset from the track by tiny pads; pixel geometry, not a DS row/form.
      coord_t textX = trackX + PAD_TINY; // ds-allow: volume capsule widget — capsule label X inset by a tiny pad; pixel geometry, not a DS row/form.
      coord_t labelAreaH = trackY > box.y ? trackY - box.y : box.h;
      FontIndex font = chipTextFontForBox("VOL", textW, labelAreaH);
      coord_t labelH = getFontHeight(LcdFlags(font) << 8u);
      coord_t labelY = box.y + (labelAreaH - labelH) / 2;

      etx_font(capsuleLabel, font);
      etx_txt_color(capsuleLabel, statusPrimaryColor(topbar));
      lv_obj_set_style_text_align(capsuleLabel, LV_TEXT_ALIGN_CENTER,
                                  LV_PART_MAIN);
      lv_obj_set_pos(capsuleLabel, textX, labelY); // ds-allow: volume capsule widget — capsule label positioned at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(capsuleLabel, textW, labelH);
    }

    if (!topbar && textX > 0) {
      coord_t textRight = width() > pad ? width() - pad : width();
      if (textX + 24 < textRight) {
        coord_t titleH = height() >= 54 ? EdgeTxStyles::STD_FONT_HEIGHT / 2 : 0;
        FontIndex valueFont = responsiveTextFont(height() - 2 * pad - titleH);
        coord_t valueH = getFontHeight(LcdFlags(valueFont) << 8u);
        coord_t textBlockH = titleH + valueH;
        coord_t textY = (height() - textBlockH) / 2;
        coord_t textW = textRight - textX;

        setStatusLabel(title, "VOL", COLOR_THEME_PRIMARY1_INDEX, FONT_XXS_INDEX,
                       textX, textY, textW, titleH);
        setStatusLabel(value, "", statusPrimaryColor(false), valueFont, textX,
                       textY + titleH, textW, valueH);
      } else {
        setLvVisible(title, false);
        setLvVisible(value, false);
      }
    } else if (!topbar) {
      setLvVisible(title, false);
      setLvVisible(value, false);
    }

    lastLevel = 255;
    lastPercent = 255;
  }

  void onForeground() override
  {
    bool topbar = isCompactTopBarWidget();
    uint8_t level = speakerVolumeLevel();
    uint8_t percent = speakerVolumePercent();
    if (level == lastLevel && percent == lastPercent) return;

    lastLevel = level;
    lastPercent = percent;

    setStatusPartColor(track, statusMutedColor(topbar));

    coord_t fillW =
        percent > 0
            ? maxCoord((coord_t)1, (coord_t)divRoundClosest(
                                       (uint16_t)fillMaxW * percent, 100))
            : 0;
    if (fillW > fillMaxW) fillW = fillMaxW;

    setLvVisible(fill, level > 0 && fillW > 0);
    if (fill) {
      lv_obj_set_size(fill, fillW, fillH);
      setStatusPartColor(fill, statusPrimaryColor(topbar));
    }

    if (topbar && capsuleLabel) lv_label_set_text(capsuleLabel, "VOL");

    if (value && !topbar) {
      char text[10];
      if (level == 0)
        snprintf(text, sizeof(text), "MUTE");
      else
        snprintf(text, sizeof(text), "%u%%", percent);

      setStatusLabel(
          value, text,
          level == 0 ? COLOR_THEME_DISABLED_INDEX : statusPrimaryColor(false),
          responsiveTextFont(height() - 2 * PAD_SMALL - // ds-allow: volume capsule widget — value font sized to the zone height minus vertical insets; pixel geometry, not a DS row/form.
                             EdgeTxStyles::STD_FONT_HEIGHT / 2),
          lv_obj_get_x(value), lv_obj_get_y(value), lv_obj_get_width(value),
          lv_obj_get_height(value));
    }
  }

 protected:
  lv_obj_t* track = nullptr;
  lv_obj_t* fill = nullptr;
  lv_obj_t* capsuleLabel = nullptr;
  lv_obj_t* title = nullptr;
  lv_obj_t* value = nullptr;
  coord_t trackMaxW = 1;
  coord_t trackH = 1;
  coord_t fillMaxW = 1;
  coord_t fillH = 1;
  uint8_t lastLevel = 255;
  uint8_t lastPercent = 255;
};

BaseWidgetFactory<VolumeStatusWidget> volumeStatusWidget("Volume", nullptr,
                                                         STR_VOLUME);

#endif

// Combines the "Both" (HeaderDateTime, two lines) rendering this widget has
// always had with single-line Time-only / Date-only modes that reuse the
// exact DateTextWidget (Clock/Today) formatting helpers. The Format option is
// APPENDED at index 1, after the retired index-0 Color placeholder, so
// layouts saved before this option existed keep loading with Both (index 0
// default) unchanged.
class DateTimeWidget : public Widget
{
 public:
  DateTimeWidget(const WidgetFactory* factory, Window* parent,
                 const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    // Both children are always created and just show()/hide() per the
    // current Format (re-read fresh in onUpdate() every time, not cached
    // from construction) so an in-place Format edit via the widget's own
    // Settings dialog -- which calls onUpdate() on this SAME live instance,
    // not a fresh construction -- takes effect immediately.
    dateTime = new (std::nothrow) HeaderDateTime(this, 0, 0);
    initRequiredLvObj(
        label,
        [](lv_obj_t* parent) {
          return etx_label_create(parent, FONT_XS_INDEX);
        },
        [](lv_obj_t* obj) { lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT); });

    update();
    foreground();
  }

  void onForeground() override
  {
    if (format == DATETIME_FORMAT_BOTH) {
      Widget::checkEvents();
      return;
    }

    struct gtm time;
    gettime(&time);
    DateTextKind kind =
        format == DATETIME_FORMAT_TIME ? DateTextKind::Clock : DateTextKind::Today;
    if (textValid && !dateTextShouldRefresh(kind, time, lastTime)) return;

    char text[16];
    formatDateText(text, sizeof(text), time, kind);
    label.with([&](lv_obj_t* obj) {
      layoutSingleLineText(obj, text);
      lv_label_set_text(obj, text);
      lastTime = time;
      textValid = true;
    });
  }

  void onUpdate() override
  {
    auto widgetData = getPersistentData();
    format = DATETIME_FORMAT_BOTH;
    if (widgetData) {
      // WidgetOption::Integer round-trips via signedValue (matches
      // GaugeWidget's Min/Max reads and the settings-UI NumberEdit editor).
      uint8_t stored = (uint8_t)widgetData->options[1].value.signedValue;
      if (stored == DATETIME_FORMAT_TIME || stored == DATETIME_FORMAT_DATE)
        format = stored;
    }

    bool both = format == DATETIME_FORMAT_BOTH;
    if (dateTime) dateTime->show(both);
    label.with([&](lv_obj_t* obj) { setLvVisible(obj, !both); });

    if (!both) {
      // No colour option: use the theme ink (PRIMARY2), matching
      // DateTextWidget's single-line layout.
      const bool topbar = isCompactTopBarWidget();
      const coord_t pad = topbar ? TOPBAR_CONTENT_PAD : PAD_SMALL; // ds-allow: date-time widget — content inset chosen by top-bar vs main-view mode; pixel geometry, not a DS row/form.
      if (topbar) {
        textBox = topbarContentBox(width(), height());
      } else {
        textBox = {pad, pad, width() > 2 * pad ? width() - 2 * pad : width(),
                   height() > 2 * pad ? height() - 2 * pad : height()};
      }
      textValid = false;
      return;
    }

    // No colour option: use the theme ink (PRIMARY2).
    if (!dateTime) return;
    dateTime->setColor(COLOR2FLAGS(COLOR_THEME_PRIMARY2_INDEX));
    bool compact = isCompactTopBarWidget();
    coord_t pad = TOPBAR_CONTENT_PAD;
    coord_t displayWidth =
        compact ? maxCoord((coord_t)(width() - 2 * pad), (coord_t)1)
                : HeaderDateTime::HDR_DATE_WIDTH;
    coord_t x = compact ? pad : width() - displayWidth - DT_XO;
    coord_t y = compact
                    ? maxCoord((height() - dateTime->height()) / 2, (coord_t)0)
                    : PAD_THREE; // ds-allow: date-time widget — non-compact Y offset inset from the top of the zone; pixel geometry, not a DS row/form.
    dateTime->setDisplayWidth(displayWidth);
    dateTime->setTextAlign(LV_TEXT_ALIGN_LEFT);
    dateTime->setPos(x, y);
  }

  HeaderDateTime* dateTime = nullptr;
  int8_t lastMinute = -1;

  static const WidgetOption options[];

  // Adjustment to make main view date/time align with model/radio settings
  // views
  static LAYOUT_VAL_SCALED(DT_XO, 1) // ds-allow: date-time widget — DPI-scaled X nudge aligning main-view date/time with the settings views; canvas geometry, not a DS row/form.

 private:
  void layoutSingleLineText(lv_obj_t* obj, const char* text)
  {
    const bool topbar = isCompactTopBarWidget();
    FontIndex font = topbar ? chipTextFontForBox(text, textBox.w, textBox.h)
                            : textFontForBox(text, textBox.w, textBox.h);
    coord_t labelH = getFontHeight(LcdFlags(font) << 8u);
    coord_t labelY = textBox.y + (textBox.h - labelH) / 2;

    etx_font(obj, font);
    etx_txt_color(obj, topbar ? statusPrimaryColor(topbar)
                              : COLOR_THEME_PRIMARY2_INDEX);
    lv_obj_set_style_text_align(
        obj, topbar ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(obj, textBox.x, labelY); // ds-allow: date-time widget — single-line text positioned at an absolute offset within the content box; canvas widget, not a DS row/form.
    lv_obj_set_size(obj, textBox.w, labelH);
  }

  uint8_t format = DATETIME_FORMAT_BOTH;
  RequiredLvObj label;
  StatusContentBox textBox = {};
  struct gtm lastTime = {};
  bool textValid = false;
};

const WidgetOption DateTimeWidget::options[] = {
    {"", WidgetOption::Deprecated, 0},
    // Format (Both/Time/Date) as a labelled dropdown, appended at the end so
    // layouts persisted before this option existed default to index 0 = Both
    // (unchanged behaviour). choiceValues supplies the labels; the stored
    // value is 0-based (0=Both, 1=Time, 2=Date), matching DateTimeFormat.
    // The value fields MUST be fully-braced WidgetOptionValue (via the macro),
    // never bare ints: WidgetOptionValue's second member is a std::string, so
    // a bare `0, 0, 2` would brace-elide a 0 into deflt.stringValue and build
    // std::string from a null pointer constant, crashing at static-init.
    {"Format", WidgetOption::Choice, WIDGET_OPTION_VALUE_SIGNED(0),
     WIDGET_OPTION_VALUE_SIGNED(0), WIDGET_OPTION_VALUE_SIGNED(2), nullptr, {},
     {"Both", "Time", "Date"}},
    {nullptr, WidgetOption::Bool}};

BaseWidgetFactory<DateTimeWidget> DateTimeWidget("Date Time",
                                                 DateTimeWidget::options,
                                                 STR_DATE_TIME_WIDGET);

class DateTextWidget : public Widget
{
 protected:
  DateTextWidget(const WidgetFactory* factory, Window* parent,
                 const rect_t& rect, WidgetLocation location,
                 DateTextKind kind) :
      Widget(factory, parent, rect, location), kind(kind)
  {
    initRequiredLvObj(
        label,
        [](lv_obj_t* parent) {
          return etx_label_create(parent, FONT_XS_INDEX);
        },
        [](lv_obj_t* obj) { lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT); });

    update();
    foreground();
  }

 public:
  void onForeground() override
  {
    struct gtm time;
    gettime(&time);
    if (textValid && !shouldRefresh(time)) return;

    char text[16];
    formatText(text, sizeof(text), time);
    label.with([&](lv_obj_t* obj) {
      layoutText(obj, text);
      lv_label_set_text(obj, text);
      lastTime = time;
      textValid = true;
    });
  }

  void onUpdate() override
  {
    auto widgetData = getPersistentData();
    (void)widgetData;

    // No colour option: use the theme ink (PRIMARY2).
    color = COLOR2FLAGS(COLOR_THEME_PRIMARY2_INDEX);

    const bool topbar = isCompactTopBarWidget();
    const coord_t pad = topbar ? TOPBAR_CONTENT_PAD : PAD_SMALL; // ds-allow: date/clock text widget — content inset chosen by top-bar vs main-view mode; pixel geometry, not a DS row/form.
    if (topbar) {
      textBox = topbarContentBox(width(), height());
    } else {
      textBox = {pad, pad, width() > 2 * pad ? width() - 2 * pad : width(),
                 height() > 2 * pad ? height() - 2 * pad : height()};
    }

    textValid = false;
  }

  static const WidgetOption options[];

 private:
  bool shouldRefresh(const struct gtm& time) const
  {
    return dateTextShouldRefresh(kind, time, lastTime);
  }

  void formatText(char* text, size_t size, const struct gtm& time) const
  {
    formatDateText(text, size, time, kind);
  }

  void layoutText(lv_obj_t* obj, const char* text)
  {
    const bool topbar = isCompactTopBarWidget();
    FontIndex font = topbar ? chipTextFontForBox(text, textBox.w, textBox.h)
                            : textFontForBox(text, textBox.w, textBox.h);
    coord_t labelH = getFontHeight(LcdFlags(font) << 8u);
    coord_t labelY = textBox.y + (textBox.h - labelH) / 2;

    etx_font(obj, font);
    if (topbar)
      etx_txt_color(obj, statusPrimaryColor(topbar));
    else
      etx_txt_color_from_flags(obj, color);
    lv_obj_set_style_text_align(
        obj, topbar ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(obj, textBox.x, labelY); // ds-allow: date/clock text widget — text label positioned at an absolute offset within the content box; canvas widget, not a DS row/form.
    lv_obj_set_size(obj, textBox.w, labelH);
  }

  DateTextKind kind;
  RequiredLvObj label;
  StatusContentBox textBox = {};
  uint32_t color = COLOR2FLAGS(COLOR_THEME_PRIMARY2_INDEX);
  struct gtm lastTime = {};
  bool textValid = false;
};

const WidgetOption DateTextWidget::options[] = {
    {"", WidgetOption::Deprecated, 0},
    {nullptr, WidgetOption::Bool}};

class ClockWidget : public DateTextWidget
{
 public:
  ClockWidget(const WidgetFactory* factory, Window* parent, const rect_t& rect,
              WidgetLocation location) :
      DateTextWidget(factory, parent, rect, location, DateTextKind::Clock)
  {
  }
};

BaseWidgetFactory<ClockWidget> clockWidget("Clock", DateTextWidget::options,
                                           "Clock");

class TodayWidget : public DateTextWidget
{
 public:
  TodayWidget(const WidgetFactory* factory, Window* parent, const rect_t& rect,
              WidgetLocation location) :
      DateTextWidget(factory, parent, rect, location, DateTextKind::Today)
  {
  }
};

BaseWidgetFactory<TodayWidget> todayWidget("Today", DateTextWidget::options,
                                           "Today");

#if defined(INTERNAL_GPS)

class InternalGPSWidget : public Widget
{
 public:
  InternalGPSWidget(const WidgetFactory* factory, Window* parent,
                    const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    icon = new (std::nothrow)
        StaticIcon(this, width() / 2 - PAD_LARGE - PAD_TINY, ICON_H, // ds-allow: internal-GPS widget — GPS icon centred with a fixed pixel nudge in the zone; canvas widget, not a DS row/form.
                   ICON_TOPMENU_GPS, COLOR_THEME_SECONDARY2_INDEX);

    numSats = new (std::nothrow) DynamicNumber<uint16_t>(
        this, {0, 1, width(), SATS_H}, [=] { return gpsData.numSat; },
        COLOR_THEME_PRIMARY2_INDEX, CENTERED | FONT(XS));
  }

  void onForeground() override
  {
    bool hasGPS = serialGetModePort(UART_MODE_GPS) >= 0;

    if (numSats) numSats->show(hasGPS && (gpsData.numSat > 0));
    if (icon) icon->show(hasGPS);

    if (icon) {
      if (gpsData.fix)
        icon->setColor(COLOR_THEME_PRIMARY2_INDEX);
      else
        icon->setColor(COLOR_THEME_SECONDARY2_INDEX);
    }
  }

 protected:
  StaticIcon* icon = nullptr;
  DynamicNumber<uint16_t>* numSats = nullptr;

  static LAYOUT_VAL_SCALED(ICON_H, 19) static LAYOUT_VAL_SCALED(SATS_H, 12) // ds-allow: internal-GPS widget — DPI-scaled GPS icon Y and satellite-count row height; canvas geometry, not a DS row/form.
};

BaseWidgetFactory<InternalGPSWidget> InternalGPSWidget("Internal GPS", nullptr,
                                                       STR_INT_GPS_LABEL);

#endif

// Test hook: linkStateLevel() is anonymous-namespace-scoped (internal
// linkage), so it is re-exposed here with external linkage for
// widget_state_level_tests.cpp. Behaviour is identical to the private
// helper -- no logic duplicated.
uint8_t linkStateLevelForTest(uint8_t rssi) { return linkStateLevel(rssi); }
