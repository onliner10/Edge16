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
  static const FontIndex candidates[] = {
      FONT_XXL_INDEX,  FONT_LXL_INDEX, FONT_XL_INDEX, FONT_L_INDEX,
      FONT_BOLD_INDEX, FONT_STD_INDEX, FONT_XS_INDEX, FONT_XXS_INDEX};

  for (auto font : candidates) {
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

bool linkWarning(uint8_t rssi)
{
  return rssi > 0 && rssi < (uint8_t)g_model.rfAlarms.warning;
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
    bool warning = linkWarning(rssi);
    uint8_t active = activeLinkBars(rssi);
    LcdColorIndex activeColor =
        warning ? COLOR_THEME_WARNING_INDEX : statusPrimaryColor(topbar);
    LcdColorIndex mutedColor = statusMutedColor(topbar);

    for (uint8_t i = 0; i < LINK_BARS; i += 1) {
      setStatusPartColor(bars[i], i < active ? activeColor : mutedColor);
    }

    if (value && !topbar) {
      char text[8];
      if (rssi == 0)
        snprintf(text, sizeof(text), "--");
      else
        snprintf(text, sizeof(text), "%u", rssi);

      FontIndex font = responsiveTextFont(height() - 2 * PAD_SMALL - // ds-allow: RSSI link-bars widget — value font sized to the zone height minus vertical insets; pixel geometry, not a DS row/form.
                                          EdgeTxStyles::STD_FONT_HEIGHT / 2);
      setStatusLabel(
          value, text,
          warning ? COLOR_THEME_WARNING_INDEX : statusPrimaryColor(false), font,
          lv_obj_get_x(value), lv_obj_get_y(value), lv_obj_get_width(value),
          lv_obj_get_height(value));
    }
  }

 protected:
  lv_obj_t* bars[LINK_BARS] = {};
  lv_obj_t* title = nullptr;
  lv_obj_t* value = nullptr;
  uint8_t lastRSSI = 255;
};

BaseWidgetFactory<LinkStatusWidget> linkStatusWidget("Link", nullptr, "Link");

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
    bool warning = IS_TXBATT_WARNING() || txBatteryPercent(g_vbat100mV) < 30;
    uint8_t bars = GET_TXBATT_BARS(fillMaxW);
    LcdColorIndex color =
        warning ? COLOR_THEME_WARNING_INDEX : statusPrimaryColor(topbar);

    setStatusPartBorder(shell, color, 2);
    setStatusPartColor(cap, color);

    if (bars != lastBattBars) {
      lastBattBars = bars;
      if (fill) {
        lv_obj_set_size(fill, bars, fillH);
      }
    }
    setStatusPartColor(fill, color);

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

BaseWidgetFactory<TxBatteryStatusWidget> txBatteryStatusWidget("TX Battery",
                                                               nullptr,
                                                               "TX Battery");

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

class RadioInfoWidget : public Widget
{
 public:
  RadioInfoWidget(const WidgetFactory* factory, Window* parent,
                  const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    bool compact = isCompactTopBarWidget();

    // Logs
    logsIcon = new (std::nothrow) StaticIcon(this, W_LOG_X, PAD_THREE, ICON_DOT, // ds-allow: radio-info top-bar widget — logs icon placed at a fixed pixel offset in the top bar; canvas widget, not a DS row/form.
                                             COLOR_THEME_PRIMARY2_INDEX);
    if (logsIcon) logsIcon->hide();

    usbIcon = new (std::nothrow) StaticIcon(
        this, W_USB_X, W_USB_Y, ICON_TOPMENU_USB, COLOR_THEME_PRIMARY2_INDEX);
    if (usbIcon) usbIcon->hide();

#if defined(AUDIO)
    if (!compact) {
      audioScale = new (std::nothrow)
          StaticIcon(this, W_AUDIO_SCALE_X, PAD_TINY, ICON_TOPMENU_VOLUME_SCALE, // ds-allow: radio-info top-bar widget — audio-scale icon placed at a fixed pixel offset in the top bar; canvas widget, not a DS row/form.
                     COLOR_THEME_SECONDARY2_INDEX);

      for (int i = 0; i < 5; i += 1) {
        audioVol[i] = new (std::nothrow) StaticIcon(
            this, W_AUDIO_X, PAD_TINY, (EdgeTxIcon)(ICON_TOPMENU_VOLUME_0 + i), // ds-allow: radio-info top-bar widget — volume-level icon placed at a fixed pixel offset in the top bar; canvas widget, not a DS row/form.
            COLOR_THEME_PRIMARY2_INDEX);
        if (audioVol[i]) audioVol[i]->hide();
      }
      if (audioVol[0]) audioVol[0]->show();
    }
#endif

    if (!compact) {
      batteryIcon = new (std::nothrow)
          StaticIcon(this, W_AUDIO_X, W_BATT_Y, ICON_TOPMENU_TXBATT,
                     COLOR_THEME_PRIMARY2_INDEX);
    }
#if defined(USB_CHARGER)
    if (!compact) {
      batteryChargeIcon = new (std::nothrow)
          StaticIcon(this, W_BATT_CHG_X, W_BATT_CHG_Y,
                     ICON_TOPMENU_TXBATT_CHARGE, COLOR_THEME_PRIMARY2_INDEX);
      if (batteryChargeIcon) batteryChargeIcon->hide();
    }
#endif

    if (compact) {
      withLive([&](LiveWindow& live) {
        batteryShell = lv_obj_create(live.lvobj());
        if (batteryShell) {
          lv_obj_clear_flag(batteryShell, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_set_style_bg_opa(batteryShell, LV_OPA_TRANSP, LV_PART_MAIN);
          lv_obj_set_style_border_width(batteryShell, 2, LV_PART_MAIN);
          lv_obj_set_style_border_opa(batteryShell, LV_OPA_COVER, LV_PART_MAIN);
          lv_obj_set_style_border_color(
              batteryShell, makeLvColor(COLOR_THEME_PRIMARY2), LV_PART_MAIN);
          lv_obj_set_style_radius(batteryShell, 2, LV_PART_MAIN);
        }
        batteryCap = lv_obj_create(live.lvobj());
        if (batteryCap) {
          lv_obj_clear_flag(batteryCap, LV_OBJ_FLAG_CLICKABLE);
          etx_solid_bg(batteryCap, COLOR_THEME_PRIMARY2_INDEX);
        }
      });
    }

#if defined(INTERNAL_MODULE_PXX1) && defined(EXTERNAL_ANTENNA)
    if (!compact) {
      extAntenna = new (std::nothrow)
          StaticIcon(this, W_RSSI_X - PAD_SMALL, 1, ICON_TOPMENU_ANTENNA, // ds-allow: radio-info top-bar widget — external-antenna icon placed just left of the RSSI cluster at a fixed offset; canvas widget, not a DS row/form.
                     COLOR_THEME_PRIMARY2_INDEX);
      if (extAntenna) extAntenna->hide();
    }
#endif

    withLive([&](LiveWindow& live) {
      batteryFill = lv_obj_create(live.lvobj());
      if (batteryFill) {
        lv_obj_set_style_bg_opa(batteryFill, LV_OPA_COVER, LV_PART_MAIN);
      }
    });
    update();

    // RSSI bars
    withLive([&](LiveWindow& live) {
      for (unsigned int i = 0; i < DIM(rssiBars); i++) {
        rssiBars[i] = lv_obj_create(live.lvobj());
        if (rssiBars[i]) {
          etx_solid_bg(rssiBars[i], COLOR_THEME_SECONDARY2_INDEX);
          etx_bg_color(rssiBars[i], COLOR_THEME_PRIMARY2_INDEX,
                       LV_STATE_USER_1);
        }
      }
    });

    layoutStatus();
    foreground();
  }

  void onUpdate() override
  {
    auto widgetData = getPersistentData();
    if (!batteryFill) return;

    layoutStatus();

    // No colour options: fixed battery-level fills (high/mid/low).
    (void)widgetData;
    etx_bg_color_from_flags(batteryFill, RGB2FLAGS(0x4C, 0xAF, 0x50),
                            LV_PART_MAIN);
    etx_bg_color_from_flags(batteryFill, RGB2FLAGS(0xFF, 0xC1, 0x07),
                            LV_STATE_USER_1);
    etx_bg_color_from_flags(batteryFill, RGB2FLAGS(0xF4, 0x43, 0x36),
                            LV_STATE_USER_2);
  }

  void onForeground() override
  {
    bool compact = isCompactTopBarWidget();

    if (usbIcon) usbIcon->show(!compact && usbPlugged());
    if (getSelectedUsbMode() == USB_UNSELECTED_MODE) {
      if (usbIcon) usbIcon->setColor(COLOR_THEME_SECONDARY2_INDEX);
    } else {
      if (usbIcon) usbIcon->setColor(COLOR_THEME_PRIMARY2_INDEX);
    }

    if (logsIcon) {
      logsIcon->show(!compact && !usbPlugged() &&
                     isFunctionActive(FUNCTION_LOGS) && BLINK_ON_PHASE);
    }

#if defined(AUDIO)
    /* Audio volume */
    uint8_t vol = 4;
    if (requiredSpeakerVolume == 0 || g_eeGeneral.beepMode == e_mode_quiet)
      vol = 0;
    else if (requiredSpeakerVolume < 7)
      vol = 1;
    else if (requiredSpeakerVolume < 13)
      vol = 2;
    else if (requiredSpeakerVolume < 19)
      vol = 3;
    if (vol != lastVol) {
      if (audioVol[vol]) audioVol[vol]->show();
      if (audioVol[lastVol]) audioVol[lastVol]->hide();
      lastVol = vol;
    }
#endif

#if defined(USB_CHARGER)
    if (batteryChargeIcon) batteryChargeIcon->show(!compact && usbChargerLed());
#endif

#if defined(INTERNAL_MODULE_PXX1) && defined(EXTERNAL_ANTENNA)
    if (extAntenna) {
      extAntenna->show(!compact && isModuleXJT(INTERNAL_MODULE) &&
                       isExternalAntennaEnabled());
    }
#endif

    // Battery level
    uint8_t bars = GET_TXBATT_BARS(batteryFillWidth());
    if (bars != lastBatt) {
      lastBatt = bars;
      lv_obj_set_size(batteryFill, bars, batteryFillHeight());
    }
    uint8_t pct = txBatteryPercent(g_vbat100mV);
    if (pct >= 60) {
      lv_obj_clear_state(batteryFill, static_cast<lv_state_t>(LV_STATE_USER_1 | LV_STATE_USER_2));
    } else if (pct >= 30) {
      lv_obj_add_state(batteryFill, LV_STATE_USER_1);
      lv_obj_clear_state(batteryFill, LV_STATE_USER_2);
    } else {
      lv_obj_clear_state(batteryFill, LV_STATE_USER_1);
      lv_obj_add_state(batteryFill, LV_STATE_USER_2);
    }

    // RSSI
    const uint8_t rssiBarsValue[] = {30, 40, 50, 60, 80};
    uint8_t rssi = TELEMETRY_RSSI();
    if (rssi != lastRSSI) {
      lastRSSI = rssi;
      for (unsigned int i = 0; i < DIM(rssiBarsValue); i++) {
        if (!rssiBars[i]) continue;
        if (rssi >= rssiBarsValue[i])
          lv_obj_add_state(rssiBars[i], LV_STATE_USER_1);
        else
          lv_obj_clear_state(rssiBars[i], LV_STATE_USER_1);
      }
    }
  }

  static const WidgetOption options[];

  static constexpr coord_t W_AUDIO_X = 0;
  static LAYOUT_VAL_SCALED(W_AUDIO_SCALE_X, 15) static LAYOUT_VAL_SCALED( // ds-allow: radio-info top-bar widget — DPI-scaled audio-scale icon X for the top bar; canvas geometry, not a DS row/form.
      W_USB_X, 32) static LAYOUT_VAL_SCALED(W_USB_Y, 5) static constexpr coord_t // ds-allow: radio-info top-bar widget — DPI-scaled USB icon X/Y (logs icon shares X) for the top bar; canvas geometry, not a DS row/form.
      W_LOG_X = W_USB_X;
  static LAYOUT_VAL_SCALED(W_RSSI_X, 37) static LAYOUT_VAL_SCALED(W_RSSI_BAR_W, // ds-allow: radio-info top-bar widget — DPI-scaled RSSI cluster X and main-view RSSI bar width; canvas geometry, not a DS row/form.
                                                                  5) static LAYOUT_VAL_SCALED(W_RSSI_BAR_H, 36) static LAYOUT_VAL_SCALED(W_RSSI_BAR_SZ, 7) static LAYOUT_VAL_SCALED(W_BATT_Y, 25) static LAYOUT_VAL_SCALED(W_BATT_FILL_W, 20) static LAYOUT_VAL_SCALED(W_BATT_FILL_H, 10) static LAYOUT_VAL_SCALED(W_BATT_FILL_GRN, // ds-allow: radio-info top-bar widget — DPI-scaled main-view RSSI bar dims, battery Y and battery-fill width/height; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                   12) static LAYOUT_VAL_SCALED(W_BATT_FILL_ORA, 5) static LAYOUT_VAL_SCALED(W_BATT_HUD_X, // ds-allow: radio-info top-bar widget — DPI-scaled main-view battery fill green/orange thresholds and compact HUD X; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                             2) static LAYOUT_VAL_SCALED(W_BATT_HUD_W, // ds-allow: radio-info top-bar widget — DPI-scaled compact battery HUD shell width; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                                                         29) static LAYOUT_VAL_SCALED(W_BATT_HUD_H, 13) static LAYOUT_VAL_SCALED(W_BATT_HUD_FILL_W, 25) static LAYOUT_VAL_SCALED(W_BATT_HUD_FILL_H, 9) static LAYOUT_VAL_SCALED(W_BATT_HUD_FILL_GRN, 14) static LAYOUT_VAL_SCALED(W_BATT_HUD_FILL_ORA, // ds-allow: radio-info top-bar widget — DPI-scaled compact battery HUD shell height and fill dims/green threshold; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  6) static LAYOUT_VAL_SCALED(W_BATT_CHG_X, // ds-allow: radio-info top-bar widget — DPI-scaled compact HUD orange fill threshold and battery-charge icon X; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              25) static LAYOUT_VAL_SCALED(W_BATT_CHG_Y, 23) static LAYOUT_VAL_SCALED(W_RSSI_HUD_BAR_W, // ds-allow: radio-info top-bar widget — DPI-scaled battery-charge icon Y and compact HUD RSSI bar width; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      5) static LAYOUT_VAL_SCALED(W_RSSI_HUD_BAR_SZ, // ds-allow: radio-info top-bar widget — DPI-scaled compact HUD RSSI bar step/size; canvas geometry, not a DS row/form.
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  7)

      coord_t batteryFillX() const
  {
    return isCompactTopBarWidget() ? W_BATT_HUD_X + 2 : W_AUDIO_X + 1;
  }

  coord_t batteryFillY() const
  {
    return isCompactTopBarWidget() ? batteryShellY() + 2 : W_BATT_Y + 1;
  }

  uint8_t batteryFillWidth() const
  {
    return isCompactTopBarWidget() ? W_BATT_HUD_FILL_W : W_BATT_FILL_W;
  }

  coord_t batteryFillHeight() const
  {
    return isCompactTopBarWidget() ? W_BATT_HUD_FILL_H : W_BATT_FILL_H;
  }

  uint8_t batteryGreenThreshold() const
  {
    return isCompactTopBarWidget() ? W_BATT_HUD_FILL_GRN : W_BATT_FILL_GRN;
  }

  uint8_t batteryOrangeThreshold() const
  {
    return isCompactTopBarWidget() ? W_BATT_HUD_FILL_ORA : W_BATT_FILL_ORA;
  }

  coord_t rssiX() const
  {
    if (!isCompactTopBarWidget()) return W_RSSI_X;

    coord_t minX = W_BATT_HUD_X + W_BATT_HUD_W + PAD_MEDIUM; // ds-allow: radio-info top-bar widget — RSSI cluster min X kept clear of the battery HUD by a medium gap; pixel geometry, not a DS row/form.
    coord_t alignX = width() - rssiClusterWidth();
    return alignX > minX ? alignX : minX;
  }

  coord_t rssiBarWidth() const
  {
    return isCompactTopBarWidget() ? W_RSSI_HUD_BAR_W : W_RSSI_BAR_W;
  }

  coord_t rssiBarStep() const
  {
    return isCompactTopBarWidget() ? W_RSSI_HUD_BAR_SZ : W_RSSI_BAR_SZ;
  }

  coord_t rssiBarHeight() const
  {
    return isCompactTopBarWidget() ? height() - 2 * PAD_TINY : (coord_t)31; // ds-allow: radio-info top-bar widget — RSSI bar height in the compact top bar from zone height minus tiny insets; pixel geometry, not a DS row/form.
  }

  coord_t rssiBarBottom() const
  {
    return isCompactTopBarWidget() ? height() - PAD_TINY : W_RSSI_BAR_H; // ds-allow: radio-info top-bar widget — RSSI bar baseline in the compact top bar from zone height minus a tiny inset; pixel geometry, not a DS row/form.
  }

 protected:
  uint8_t lastVol = 0;
  uint8_t lastBatt = 0;
  uint8_t lastRSSI = 0;
  StaticIcon* logsIcon = nullptr;
  StaticIcon* usbIcon = nullptr;
#if defined(AUDIO)
  StaticIcon* audioScale = nullptr;
  StaticIcon* audioVol[5] = {};
#endif
  StaticIcon* batteryIcon = nullptr;
  lv_obj_t* batteryShell = nullptr;
  lv_obj_t* batteryCap = nullptr;
  lv_obj_t* batteryFill = nullptr;
  lv_obj_t* rssiBars[5] = {nullptr};
#if defined(USB_CHARGER)
  StaticIcon* batteryChargeIcon = nullptr;
#endif
#if defined(INTERNAL_MODULE_PXX1) && defined(EXTERNAL_ANTENNA)
  StaticIcon* extAntenna = nullptr;
#endif

  coord_t batteryShellY() const { return height() - W_BATT_HUD_H - PAD_TINY; } // ds-allow: radio-info top-bar widget — battery HUD shell Y anchored to the zone bottom minus a tiny inset; pixel geometry, not a DS row/form.

  coord_t rssiClusterWidth() const
  {
    return 4 * rssiBarStep() + rssiBarWidth();
  }

  void layoutStatus()
  {
    if (batteryShell) {
      lv_obj_set_pos(batteryShell, W_BATT_HUD_X, batteryShellY()); // ds-allow: radio-info top-bar widget — battery HUD shell positioned at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(batteryShell, W_BATT_HUD_W, W_BATT_HUD_H);
    }
    if (batteryCap) {
      lv_obj_set_pos(batteryCap, W_BATT_HUD_X + W_BATT_HUD_W, // ds-allow: radio-info top-bar widget — battery HUD cap positioned against the shell at an absolute offset; canvas widget, not a DS row/form.
                     batteryShellY() + 3);
      lv_obj_set_size(batteryCap, 3, W_BATT_HUD_H - 6);
    }
    if (batteryFill) {
      lv_obj_set_pos(batteryFill, batteryFillX(), batteryFillY()); // ds-allow: radio-info top-bar widget — battery HUD fill positioned inside the shell at an absolute offset; canvas widget, not a DS row/form.
      lv_obj_set_size(batteryFill, batteryFillWidth(), batteryFillHeight());
      lastBatt = 255;
    }

    coord_t rssiBh = rssiBarHeight();
    const uint8_t rssiBarsHeight[] = {
        (uint8_t)((rssiBh * 5 + 15) / 31), (uint8_t)((rssiBh * 10 + 15) / 31),
        (uint8_t)((rssiBh * 15 + 15) / 31), (uint8_t)((rssiBh * 21 + 15) / 31),
        (uint8_t)rssiBh};
    for (unsigned int i = 0; i < DIM(rssiBars); i++) {
      if (!rssiBars[i]) continue;
      uint8_t height = rssiBarsHeight[i];
      lv_obj_set_pos(rssiBars[i], rssiX() + i * rssiBarStep(), // ds-allow: radio-info top-bar widget — each RSSI bar positioned at an absolute offset within the cluster; canvas widget, not a DS row/form.
                     rssiBarBottom() - height);
      lv_obj_set_size(rssiBars[i], rssiBarWidth(), height);
    }
  }
};

const WidgetOption RadioInfoWidget::options[] = {
    // Retired Color options kept as hidden placeholders for positional YAML compat.
    {"", WidgetOption::Deprecated, 0},
    {"", WidgetOption::Deprecated, 0},
    {"", WidgetOption::Deprecated, 0},
    {nullptr, WidgetOption::Bool}};

class DateTimeWidget : public Widget
{
 public:
  DateTimeWidget(const WidgetFactory* factory, Window* parent,
                 const rect_t& rect, WidgetLocation location) :
      Widget(factory, parent, rect, location)
  {
    coord_t x = isCompactTopBarWidget()
                    ? TOPBAR_CONTENT_PAD
                    : rect.w - HeaderDateTime::HDR_DATE_WIDTH - DT_XO;
    coord_t dateTimeHeight =
        HeaderDateTime::HDR_DATE_LINE2 + HeaderDateTime::HDR_DATE_HEIGHT + 2;
    coord_t y = isCompactTopBarWidget()
                    ? maxCoord((rect.h - dateTimeHeight) / 2, (coord_t)0)
                    : PAD_THREE; // ds-allow: date-time widget — non-compact Y offset inset from the top of the zone; pixel geometry, not a DS row/form.
    dateTime = new (std::nothrow) HeaderDateTime(this, x, y);
    update();
  }

  void onForeground() override { Widget::checkEvents(); }

  void onUpdate() override
  {
    auto widgetData = getPersistentData();

    // No colour option: use the theme ink (PRIMARY2).
    (void)widgetData;
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
};

const WidgetOption DateTimeWidget::options[] = {
    {"", WidgetOption::Deprecated, 0},
    {nullptr, WidgetOption::Bool}};

BaseWidgetFactory<DateTimeWidget> DateTimeWidget("Date Time",
                                                 DateTimeWidget::options,
                                                 STR_DATE_TIME_WIDGET);

enum class DateTextKind { Clock, Today };

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
    if (kind == DateTextKind::Clock) {
      return time.tm_min != lastTime.tm_min || time.tm_hour != lastTime.tm_hour;
    }

    return time.tm_mday != lastTime.tm_mday || time.tm_mon != lastTime.tm_mon ||
           time.tm_year != lastTime.tm_year;
  }

  void formatText(char* text, size_t size, const struct gtm& time) const
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
