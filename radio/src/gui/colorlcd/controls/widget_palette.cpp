/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * See widget_palette.h.
 */

#include "widget_palette.h"

#include <math.h>

namespace widget_palette {

// Widget cards are a fixed light surface (NativeWidget::cardColor()), so value
// roles are always shaded darker; the top bar is the theme SECONDARY1 dark
// surface, so its variants are tinted lighter. Keep in sync with widget.cpp.
static constexpr uint32_t kCardBg = 0xF8FBFF;  // (248, 251, 255)

static inline double srgbChannel(double c)
{
  c /= 255.0;
  return (c <= 0.03928) ? (c / 12.92) : pow((c + 0.055) / 1.055, 2.4);
}

double relativeLuminance(uint32_t rgb)
{
  const double r = srgbChannel((rgb >> 16) & 0xFF);
  const double g = srgbChannel((rgb >> 8) & 0xFF);
  const double b = srgbChannel(rgb & 0xFF);
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrastRatio(uint32_t a, uint32_t b)
{
  const double la = relativeLuminance(a);
  const double lb = relativeLuminance(b);
  const double hi = la > lb ? la : lb;
  const double lo = la > lb ? lb : la;
  return (hi + 0.05) / (lo + 0.05);
}

// Blend `color` toward `endpoint` by t in [0,1] (0 = color, 1 = endpoint).
static uint32_t mixToward(uint32_t color, uint32_t endpoint, double t)
{
  auto lerp = [&](int shift) {
    const int c = (color >> shift) & 0xFF;
    const int e = (endpoint >> shift) & 0xFF;
    int v = (int)lround(c + (e - c) * t);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint32_t)v;
  };
  return (lerp(16) << 16) | (lerp(8) << 8) | lerp(0);
}

uint32_t correctForContrast(uint32_t color, uint32_t bg1, uint32_t bg2,
                            double target, bool darken)
{
  auto ok = [&](uint32_t c) {
    return contrastRatio(c, bg1) >= target && contrastRatio(c, bg2) >= target;
  };
  if (ok(color)) return color;

  // Shading toward black preserves hue exactly (a multiplicative scale);
  // tinting toward white is the accepted correction for the dark surface.
  // Luminance is monotonic in t, so contrast against the opposite-luminance
  // surface increases monotonically and the endpoint always clears target.
  const uint32_t endpoint = darken ? 0x000000u : 0xFFFFFFu;
  const int steps = 256;
  for (int i = 1; i <= steps; i++) {
    const uint32_t c = mixToward(color, endpoint, (double)i / steps);
    if (ok(c)) return c;
  }
  return endpoint;
}

// A pale, near-white surface tint that carries `seed`'s hue.
static uint32_t paleTint(uint32_t seed, uint32_t textRole, uint32_t mutedRole)
{
  for (int pct = 86; pct <= 98; pct += 2) {
    const uint32_t tint = mixToward(seed, 0xFFFFFFu, pct / 100.0);
    if (contrastRatio(textRole, tint) >= kValueContrast &&
        contrastRatio(mutedRole, tint) >= kMutedContrast)
      return tint;
  }
  return kCardBg;  // never darker than the plain card
}

void derive(const uint32_t* themeColors, uint32_t cardBg, uint32_t mainBg,
            uint32_t topbarBg, PaletteSet& out)
{
  out.cardBg = cardBg;
  out.mainBg = mainBg;
  out.topbarBg = topbarBg;

  uint32_t seed[PAL_ROLE_COUNT];
  seed[PAL_DEFAULT] = themeColors[COLOR_THEME_PRIMARY3_INDEX];
  seed[PAL_MUTED] = themeColors[COLOR_THEME_SECONDARY1_INDEX];
  seed[PAL_EMPHASIS] = themeColors[COLOR_THEME_FOCUS_INDEX];
  seed[PAL_POSITIVE] = themeColors[COLOR_THEME_EDIT_INDEX];
  seed[PAL_WARNING] = themeColors[COLOR_THEME_ACTIVE_INDEX];   // amber base
  seed[PAL_CRITICAL] = themeColors[COLOR_THEME_WARNING_INDEX]; // red base

  for (uint8_t r = 0; r < PAL_ROLE_COUNT; r++) {
    const double target = (r == PAL_MUTED) ? kMutedContrast : kValueContrast;
    out.roles[r] =
        correctForContrast(seed[r], cardBg, mainBg, target, /*darken=*/true);
    out.topbarRoles[r] = correctForContrast(seed[r], topbarBg, topbarBg, target,
                                            /*darken=*/false);
  }
  // Top-bar Default ink is the light surface colour (PRIMARY2), not a shaded
  // PRIMARY3, matching the existing top-bar text treatment.
  out.topbarRoles[PAL_DEFAULT] =
      correctForContrast(themeColors[COLOR_THEME_PRIMARY2_INDEX], topbarBg,
                         topbarBg, kValueContrast, /*darken=*/false);

  out.tints[WIDGET_STATE_NORMAL] = cardBg;
  out.tints[WIDGET_STATE_WARNING] =
      paleTint(seed[PAL_WARNING], out.roles[PAL_WARNING], out.roles[PAL_MUTED]);
  out.tints[WIDGET_STATE_CRITICAL] = paleTint(
      seed[PAL_CRITICAL], out.roles[PAL_CRITICAL], out.roles[PAL_MUTED]);

  out.borders[WIDGET_STATE_NORMAL] = themeColors[COLOR_THEME_SECONDARY2_INDEX];
  out.borders[WIDGET_STATE_WARNING] = out.roles[PAL_WARNING];
  out.borders[WIDGET_STATE_CRITICAL] = out.roles[PAL_CRITICAL];
}

// Expand an RGB565 table entry to RGB888.
static inline uint32_t rgb565to888(uint16_t c)
{
  return ((uint32_t)GET_RED(c) << 16) | ((uint32_t)GET_GREEN(c) << 8) |
         GET_BLUE(c);
}

uint32_t liveThemeSignature()
{
  uint32_t h = 2166136261u;  // FNV-1a
  for (int i = 0; i < COLOR_THEME_QM_FG_INDEX + 1; i++) {
    h = (h ^ lcdColorTable[i]) * 16777619u;
  }
  return h;
}

const PaletteSet& active()
{
  static PaletteSet cached;
  static uint32_t cachedSig = 0;
  static bool valid = false;

  const uint32_t sig = liveThemeSignature();
  if (!valid || sig != cachedSig) {
    uint32_t themeColors[COLOR_THEME_QM_FG_INDEX + 1];
    for (int i = 0; i <= COLOR_THEME_QM_FG_INDEX; i++)
      themeColors[i] = rgb565to888(lcdColorTable[i]);

    const uint32_t mainBg = rgb565to888(lcdColorTable[COLOR_THEME_SECONDARY3_INDEX]);
    const uint32_t topbarBg = rgb565to888(lcdColorTable[COLOR_THEME_SECONDARY1_INDEX]);
    derive(themeColors, kCardBg, mainBg, topbarBg, cached);
    cachedSig = sig;
    valid = true;
  }
  return cached;
}

}  // namespace widget_palette

// ---- lv_color_t accessors -------------------------------------------------

static inline lv_color_t rgbToLv(uint32_t rgb)
{
  return lv_color_make((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

lv_color_t paletteLvColor(uint8_t role)
{
  if (role >= PAL_ROLE_COUNT) role = PAL_DEFAULT;
  return rgbToLv(widget_palette::active().roles[role]);
}

lv_color_t paletteStateTextColor(uint8_t level)
{
  switch (level) {
    case WIDGET_STATE_CRITICAL: return paletteLvColor(PAL_CRITICAL);
    case WIDGET_STATE_WARNING: return paletteLvColor(PAL_WARNING);
    default: return paletteLvColor(PAL_DEFAULT);
  }
}

lv_color_t paletteStateCardTint(uint8_t level)
{
  if (level > WIDGET_STATE_CRITICAL) level = WIDGET_STATE_NORMAL;
  return rgbToLv(widget_palette::active().tints[level]);
}

lv_color_t paletteStateBorderColor(uint8_t level)
{
  if (level > WIDGET_STATE_CRITICAL) level = WIDGET_STATE_NORMAL;
  return rgbToLv(widget_palette::active().borders[level]);
}

lv_color_t paletteTopbarLvColor(uint8_t role)
{
  if (role >= PAL_ROLE_COUNT) role = PAL_DEFAULT;
  return rgbToLv(widget_palette::active().topbarRoles[role]);
}

lv_color_t paletteTopbarStateTextColor(uint8_t level)
{
  switch (level) {
    case WIDGET_STATE_CRITICAL: return paletteTopbarLvColor(PAL_CRITICAL);
    case WIDGET_STATE_WARNING: return paletteTopbarLvColor(PAL_WARNING);
    default: return paletteTopbarLvColor(PAL_DEFAULT);
  }
}
