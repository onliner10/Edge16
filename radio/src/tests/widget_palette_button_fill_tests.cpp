/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Unit tests for the D2 refinement: button-FILL contrast validation. The
 * text-role tests (widget_palette_tests.cpp) validate INK against a fixed
 * surface; these validate a filled button's SURFACE (fill) against a fixed
 * label, so a ds::DSButton's label is always legible on its fill in every
 * theme -- same >= 7:1 / AAA approach.
 */

#include "gtests.h"

#if defined(COLORLCD)

#include "widget_palette.h"

using namespace widget_palette;

// Default EdgeTX theme, 13 role colours as RGB888 (from defaultColors[]).
static const uint32_t kDefaultThemeColors[] = {
    0x000000,  // PRIMARY1  (ink)
    0xFFFFFF,  // PRIMARY2  (surface)
    0x0C3F66,  // PRIMARY3
    0x125E99,  // SECONDARY1
    0xB6E0F2,  // SECONDARY2
    0xE4EEF2,  // SECONDARY3
    0x14A1E5,  // FOCUS
    0x009909,  // EDIT
    0xFFDE00,  // ACTIVE   (amber -> primary button fill seed)
    0xE00000,  // WARNING  (red   -> destructive button fill seed)
    0x8C8C8C,  // DISABLED
    0x000000,  // QM_BG
    0xFFFFFF,  // QM_FG
};

static constexpr uint32_t kCardBg = 0xF8FBFF;
static constexpr uint32_t kMainBg = 0xE4EEF2;
static constexpr uint32_t kTopbarBg = 0x125E99;

TEST(ButtonFill, alreadyLegibleFillIsUnchanged)
{
  // Amber fill with a black label already clears AAA -- a fixed point, returned
  // verbatim (so the correction converges and never over-darkens).
  const uint32_t amber = 0xFFDE00;
  const uint32_t black = 0x000000;
  ASSERT_GE(contrastRatio(amber, black), kButtonContrast);
  EXPECT_EQ(correctFillForLabel(amber, black, kButtonContrast), amber);
}

TEST(ButtonFill, darkLabelLightensFillUntilLegible)
{
  // A dark fill under a dark (black) label is illegible; the fill must be
  // pushed toward white until the label clears AAA on it.
  const uint32_t fill = 0x102A44;   // dark navy
  const uint32_t label = 0x000000;  // black ink
  ASSERT_LT(contrastRatio(fill, label), kButtonContrast);

  const uint32_t fixed = correctFillForLabel(fill, label, kButtonContrast);
  EXPECT_GE(contrastRatio(fixed, label), kButtonContrast);
  // Moved toward white (lighter than the seed).
  EXPECT_GT(relativeLuminance(fixed), relativeLuminance(fill));
  // Idempotent.
  EXPECT_EQ(correctFillForLabel(fixed, label, kButtonContrast), fixed);
}

TEST(ButtonFill, lightLabelDarkensFillUntilLegible)
{
  // Stock red destructive fill with a WHITE label only reaches ~5:1 -- it FAILS
  // AAA and must be darkened until white clears 7:1. Proves the validator does
  // real work even on the default theme.
  const uint32_t red = 0xE00000;
  const uint32_t white = 0xFFFFFF;
  ASSERT_LT(contrastRatio(red, white), kButtonContrast);

  const uint32_t fixed = correctFillForLabel(red, white, kButtonContrast);
  EXPECT_GE(contrastRatio(fixed, white), kButtonContrast);
  EXPECT_LT(relativeLuminance(fixed), relativeLuminance(red));  // darker red
}

TEST(ButtonFill, correctionAlwaysConvergesEvenIfImpossible)
{
  // A mid-grey label cannot reach 7:1 against ANY fill; correction must still
  // terminate at the max-contrast endpoint rather than loop.
  const uint32_t grey = 0x7F7F7F;
  const uint32_t fixed = correctFillForLabel(0x808080, grey, kButtonContrast);
  EXPECT_TRUE(fixed == 0x000000u || fixed == 0xFFFFFFu);
}

TEST(ButtonFill, deriveDefaultThemeButtonsAreLegible)
{
  PaletteSet p;
  derive(kDefaultThemeColors, kCardBg, kMainBg, kTopbarBg, p);

  for (int r = 0; r < BTN_FILL_COUNT; r++) {
    EXPECT_GE(contrastRatio(p.buttonFill[r], p.buttonLabel[r]), kButtonContrast)
        << "button fill role " << r << " label not legible on fill";
  }
  // Primary (amber/black) already passed -> untouched; destructive (red/white)
  // failed the raw seed -> was corrected darker.
  EXPECT_EQ(p.buttonFill[BTN_FILL_PRIMARY], 0xFFDE00u);
  EXPECT_NE(p.buttonFill[BTN_FILL_DESTRUCTIVE], 0xE00000u);
}

TEST(ButtonFill, derivePathologicalThemeIsNeverIllegible)
{
  // A theme whose ACTIVE (primary fill) is near-black and WARNING is near-white
  // would leave the fixed ink/surface labels illegible; derivation must correct
  // both fills, never ship an unreadable button.
  uint32_t t[13];
  for (int i = 0; i < 13; i++) t[i] = 0x808080;
  t[COLOR_THEME_PRIMARY1_INDEX] = 0x000000;   // ink label
  t[COLOR_THEME_PRIMARY2_INDEX] = 0xFFFFFF;   // surface label
  t[COLOR_THEME_ACTIVE_INDEX] = 0x111111;     // near-black primary fill seed
  t[COLOR_THEME_WARNING_INDEX] = 0xF4F4F4;    // near-white destructive fill seed

  PaletteSet p;
  derive(t, kCardBg, kMainBg, kTopbarBg, p);
  for (int r = 0; r < BTN_FILL_COUNT; r++)
    EXPECT_GE(contrastRatio(p.buttonFill[r], p.buttonLabel[r]), kButtonContrast)
        << "pathological button fill role " << r;
}

TEST(ButtonFill, activePaletteButtonsAreLegible)
{
  const PaletteSet& p = active();
  for (int r = 0; r < BTN_FILL_COUNT; r++)
    EXPECT_GE(contrastRatio(p.buttonFill[r], p.buttonLabel[r]), kButtonContrast)
        << "live theme button fill role " << r;
}

#endif  // COLORLCD
