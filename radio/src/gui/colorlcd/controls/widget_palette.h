/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Internal design-token layer for stock data widgets.
 *
 * Widgets expose NO user-facing colour choice. Every widget draws through six
 * semantic roles (Default / Muted / Emphasis / Positive / Warning / Critical)
 * that are auto-derived from the ACTIVE theme's 13 colours. Each role is
 * contrast-validated against the widget-card background AND the main-screen
 * background: value-capable roles keep >= 7:1 (WCAG), the Muted role (labels
 * only) keeps >= 4.5:1. Entries that fail are auto-corrected by shading them
 * along their own hue toward black (light surfaces) or white (the dark top
 * bar) until they pass, so the radio always boots legible and a theme is never
 * rejected. The dark top bar gets its own validated role variants.
 *
 * State-aware data widgets consume these tokens through their STATE: a normal
 * value renders in Default and escalates to Warning / Critical from thresholds
 * the pilot already configured elsewhere (timer countdown window, radio
 * battery warning/critical voltage, an enabled battery monitor's charge
 * curve). Colour is backed by a redundant, non-colour structural cue on the
 * card (see NativeWidget::setCardStateCue): a 3px border + surface tint.
 */

#pragma once

#include <inttypes.h>

#include <lvgl/lvgl.h>

#include "colors.h"

// Semantic design-token roles.
enum PaletteRole : uint8_t {
  PAL_DEFAULT = 0,  // primary value text
  PAL_MUTED,        // labels / secondary text only
  PAL_EMPHASIS,     // accent, draws the eye
  PAL_POSITIVE,     // "all good" telemetry
  PAL_WARNING,      // needs attention soon
  PAL_CRITICAL,     // act now
  PAL_ROLE_COUNT
};

// State escalation used by state-aware data widgets.
enum WidgetStateLevel : uint8_t {
  WIDGET_STATE_NORMAL = 0,
  WIDGET_STATE_WARNING = 1,
  WIDGET_STATE_CRITICAL = 2,
};

namespace widget_palette {

// WCAG relative luminance of an RGB888 colour (0..1).
double relativeLuminance(uint32_t rgb);

// WCAG contrast ratio between two RGB888 colours (1..21).
double contrastRatio(uint32_t a, uint32_t b);

// Contrast floors.
constexpr double kValueContrast = 7.0;  // value-capable roles
constexpr double kMutedContrast = 4.5;  // Muted (labels only)

// Shade `color` toward black (darken=true, for light surfaces) or tint it
// toward white (darken=false, for the dark top bar) until it clears `target`
// against BOTH backgrounds, or an endpoint is reached. Luminance is monotonic
// along the shade/tint path, so this always converges (the endpoint gives
// maximum contrast against the opposite-luminance surface). Returns the
// corrected RGB888 colour.
uint32_t correctForContrast(uint32_t color, uint32_t bg1, uint32_t bg2,
                            double target, bool darken);

struct PaletteSet {
  uint32_t roles[PAL_ROLE_COUNT];        // text roles for widget cards
  uint32_t topbarRoles[PAL_ROLE_COUNT];  // text roles validated for the top bar
  uint32_t tints[3];                     // card surface: normal/warning/critical
  uint32_t borders[3];                   // card border: normal/warning/critical
  uint32_t cardBg;                       // reference backgrounds used
  uint32_t mainBg;
  uint32_t topbarBg;
};

// Auto-derive a full palette from a theme's 13 role colours. The theme is not
// required to declare a palette; roles are seeded from existing theme colours
// and every entry is contrast-corrected. `themeColors` is indexed by
// LcdColorIndex (PRIMARY1..QM_FG).
void derive(const uint32_t* themeColors, uint32_t cardBg, uint32_t mainBg,
            uint32_t topbarBg, PaletteSet& out);

// Signature (hash of the 13 live theme role colours) so the runtime palette is
// rebuilt only when the theme actually changes.
uint32_t liveThemeSignature();

// Palette derived from the ACTIVE theme, rebuilt lazily on theme change.
const PaletteSet& active();

}  // namespace widget_palette

// ---- lv_color_t convenience accessors used by widgets --------------------

lv_color_t paletteLvColor(uint8_t role);
lv_color_t paletteStateTextColor(uint8_t level);  // Default/Warning/Critical
lv_color_t paletteStateCardTint(uint8_t level);
lv_color_t paletteStateBorderColor(uint8_t level);
lv_color_t paletteTopbarLvColor(uint8_t role);
lv_color_t paletteTopbarStateTextColor(uint8_t level);
