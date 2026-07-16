/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Unit tests for the internal design-token palette layer: WCAG contrast
 * validation, auto-correction convergence, and palette derivation from a
 * theme that does NOT declare a palette (auto-derived from its 13 colours).
 */

#include "gtests.h"

#if defined(COLORLCD)

#include "widget_palette.h"

using namespace widget_palette;

// Reference light surfaces used by widget cards.
static constexpr uint32_t kCardBg = 0xF8FBFF;
static constexpr uint32_t kMainBg = 0xE4EEF2;  // theme SECONDARY3

// Default EdgeTX theme, 13 role colours as RGB888 (from defaultColors[]).
static const uint32_t kDefaultThemeColors[] = {
    0x000000,  // PRIMARY1
    0xFFFFFF,  // PRIMARY2
    0x0C3F66,  // PRIMARY3
    0x125E99,  // SECONDARY1
    0xB6E0F2,  // SECONDARY2
    0xE4EEF2,  // SECONDARY3
    0x14A1E5,  // FOCUS
    0x009909,  // EDIT
    0xFFDE00,  // ACTIVE
    0xE00000,  // WARNING
    0x8C8C8C,  // DISABLED
    0x000000,  // QM_BG
    0xFFFFFF,  // QM_FG
};

TEST(WidgetPalette, luminanceEndpoints)
{
  EXPECT_NEAR(relativeLuminance(0x000000), 0.0, 1e-9);
  EXPECT_NEAR(relativeLuminance(0xFFFFFF), 1.0, 1e-9);
  // Black vs white is the maximum WCAG contrast ratio.
  EXPECT_NEAR(contrastRatio(0x000000, 0xFFFFFF), 21.0, 1e-6);
  // Contrast is symmetric.
  EXPECT_DOUBLE_EQ(contrastRatio(0x123456, 0xFEDCBA),
                   contrastRatio(0xFEDCBA, 0x123456));
}

TEST(WidgetPalette, correctionReachesTargetAndConverges)
{
  // A pale yellow fails 7:1 on white; correction must shade it until it clears
  // the target against BOTH light backgrounds.
  const uint32_t seed = 0xFFF0A0;
  ASSERT_LT(contrastRatio(seed, kCardBg), kValueContrast);

  const uint32_t fixed =
      correctForContrast(seed, kCardBg, kMainBg, kValueContrast, /*darken=*/true);
  EXPECT_GE(contrastRatio(fixed, kCardBg), kValueContrast);
  EXPECT_GE(contrastRatio(fixed, kMainBg), kValueContrast);

  // Idempotent: a colour that already passes is returned unchanged, so the
  // correction converges (a fixed point).
  const uint32_t again =
      correctForContrast(fixed, kCardBg, kMainBg, kValueContrast, true);
  EXPECT_EQ(again, fixed);
}

TEST(WidgetPalette, correctionAlwaysConvergesEvenIfImpossible)
{
  // 7:1 against a near-black background AND a near-white background is
  // impossible for any single colour; darkening must still terminate (at the
  // black endpoint) rather than loop — the radio always boots legible.
  const uint32_t fixed = correctForContrast(0x808080, 0x101010, 0xF8FBFF,
                                            kValueContrast, /*darken=*/true);
  EXPECT_EQ(fixed, 0x000000u);
}

TEST(WidgetPalette, deriveFromThemeWithoutPaletteMeetsContrast)
{
  PaletteSet p;
  derive(kDefaultThemeColors, kCardBg, kMainBg, /*topbarBg=*/0x125E99, p);

  for (int r = 0; r < PAL_ROLE_COUNT; r++) {
    const double target = (r == PAL_MUTED) ? kMutedContrast : kValueContrast;
    EXPECT_GE(contrastRatio(p.roles[r], kCardBg), target)
        << "role " << r << " fails card contrast";
    EXPECT_GE(contrastRatio(p.roles[r], kMainBg), target)
        << "role " << r << " fails main contrast";
  }

  // State card tints keep >= 7:1 with their state text and >= 4.5:1 with Muted.
  EXPECT_GE(contrastRatio(p.roles[PAL_WARNING], p.tints[WIDGET_STATE_WARNING]),
            kValueContrast);
  EXPECT_GE(contrastRatio(p.roles[PAL_MUTED], p.tints[WIDGET_STATE_WARNING]),
            kMutedContrast);
  EXPECT_GE(contrastRatio(p.roles[PAL_CRITICAL], p.tints[WIDGET_STATE_CRITICAL]),
            kValueContrast);
  EXPECT_GE(contrastRatio(p.roles[PAL_MUTED], p.tints[WIDGET_STATE_CRITICAL]),
            kMutedContrast);
}

TEST(WidgetPalette, derivePathologicalThemeIsNeverRejected)
{
  // A theme whose roles are all near-white would be illegible on a light card;
  // derivation must auto-correct (never reject) so every value role still
  // clears 7:1.
  uint32_t washed[13];
  for (int i = 0; i < 13; i++) washed[i] = 0xF2F2F2;
  washed[COLOR_THEME_PRIMARY2_INDEX] = 0xFFFFFF;

  PaletteSet p;
  derive(washed, kCardBg, kMainBg, 0x125E99, p);
  for (int r = 0; r < PAL_ROLE_COUNT; r++) {
    const double target = (r == PAL_MUTED) ? kMutedContrast : kValueContrast;
    EXPECT_GE(contrastRatio(p.roles[r], kCardBg), target) << "role " << r;
    EXPECT_GE(contrastRatio(p.roles[r], kMainBg), target) << "role " << r;
  }
}

TEST(WidgetPalette, activePaletteFromLiveThemeIsLegible)
{
  // The palette derived from the live (default) theme table must be legible.
  const PaletteSet& p = active();
  for (int r = 0; r < PAL_ROLE_COUNT; r++) {
    const double target = (r == PAL_MUTED) ? kMutedContrast : kValueContrast;
    EXPECT_GE(contrastRatio(p.roles[r], p.cardBg), target) << "role " << r;
    EXPECT_GE(contrastRatio(p.roles[r], p.mainBg), target) << "role " << r;
  }
}

#endif  // COLORLCD
