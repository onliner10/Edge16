/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Structural invariants of the design-system row geometry (see
 * DESIGN_SYSTEM.md). These are pure (no LVGL) so they run everywhere and lock
 * the D1 Compact-density contract: a Compact row sits on the same 40 px touch
 * floor as OneLine, yet a dense list of them surfaces strictly more rows per
 * screenful than the 52 px two-line row it replaces.
 */

#include "gtests.h"

#if defined(COLORLCD)

#include "ds_core.h"
#include "etx_lv_theme.h"  // LAYOUT_SCALE

using namespace ds;

TEST(DsRow, VariantHeightsAndFloor)
{
  // Both one-line variants share the 40 px (8 mm) touch floor; picker and
  // two-line are the comfortable/detailed steps above it.
  EXPECT_EQ(rowHeight(RowSize::OneLine), LAYOUT_SCALE(40));
  EXPECT_EQ(rowHeight(RowSize::Compact), LAYOUT_SCALE(40));
  EXPECT_EQ(rowHeight(RowSize::Compact), rowHeight(RowSize::OneLine));
  EXPECT_EQ(rowHeight(RowSize::Picker), LAYOUT_SCALE(48));
  EXPECT_EQ(rowHeight(RowSize::TwoLine), LAYOUT_SCALE(52));
  // Every variant clears the floor.
  EXPECT_GE(rowHeight(RowSize::Compact), LAYOUT_SCALE(40));
  EXPECT_GE(rowHeight(RowSize::TwoLine), LAYOUT_SCALE(40));
}

TEST(DsRow, CompactFitsMoreRowsPerScreenThanTwoLine)
{
  // 227 px scrollable content band (DESIGN_SYSTEM.md §3 fit check).
  const int band = LAYOUT_SCALE(227);
  const int compactPerScreen = band / rowHeight(RowSize::Compact);
  const int twoLinePerScreen = band / rowHeight(RowSize::TwoLine);
  EXPECT_GT(compactPerScreen, twoLinePerScreen);
  EXPECT_GE(compactPerScreen, 5);  // >= 5 dense rows visible at once
  EXPECT_LE(twoLinePerScreen, 4);  // vs <= 4 two-line rows
}

TEST(DsRow, EmbeddedControlWithinFloor)
{
  // A slot-embedded control is small but its hit area is expanded to the floor
  // by the DS; the control art itself must fit inside a 40 px row.
  EXPECT_GT(embeddedControlSize(), 0);
  EXPECT_LE(embeddedControlSize(), rowHeight(RowSize::OneLine));
}

#endif  // COLORLCD
