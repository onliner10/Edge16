/*
 * Copyright (C) EdgeTX
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

// Structural + visual proof for the DESIGN_SYSTEM.md rule: "A lone section
// header never renders." (see ds::PickerOverlay::section() in
// radio/src/gui/colorlcd/libui/ds_core.cpp). There is no production
// PickerOverlay call site on this branch to reach interactively (the
// "Select role" screen that motivated the rule lives in a proposal
// worktree), so this test exercises the real DS widget directly -- same
// technique already used by the ModelNameDialog/StaticText SIMU test hooks
// in libui/dialog.cpp and libui/static.cpp -- and additionally dumps real
// LVGL-rendered PNGs (via lv_snapshot_take, same API radio/src/gui/
// screenshot.cpp uses for the on-device screenshot feature) into
// <repo>/proof/raw/ so the before/after can be eyeballed, not just asserted.

#include "gtests.h"

#if defined(COLORLCD) && defined(SIMU)

#include <sys/stat.h>

#include <string>
#include <vector>

#include "ds_core.h"
#include "location.h"
#include "mainwindow.h"

#include "stb/stb_image_write.h"  // implementation linked from lcd_480x272.cpp

// rbScale/gScale are the same RGB565->RGB888 lookup tables
// radio/src/gui/screenshot.cpp defines (external linkage, file scope) for
// COLORLCD builds -- declared here, not inside the anonymous namespace
// below, so this refers to that same global rather than declaring a
// distinct namespace-local symbol.
extern uint8_t rbScale[32];
extern uint8_t gScale[64];

namespace {

std::string proofRawDir()
{
  return std::string(TESTS_PATH) + "/../../../proof/raw";
}

// Renders the current screen (including any open modal) to a PNG under
// proof/raw/. Mirrors radio/src/gui/screenshot.cpp's on-device screenshot
// path (lv_snapshot_take + RGB565->RGB888) so this is a genuine LVGL render,
// not a synthetic mock.
void dumpScreenshot(const char* name)
{
  mkdir(proofRawDir().c_str(), 0755);

  lv_draw_buf_t* snapshot =
      lv_snapshot_take(lv_scr_act(), LV_COLOR_FORMAT_RGB565);
  ASSERT_NE(snapshot, nullptr) << "lv_snapshot_take failed for " << name;

  const int w = LCD_W;
  const int h = LCD_H;
  std::vector<uint8_t> img(size_t(w) * h * 3);
  uint8_t* dst = img.data();
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      lv_color16_t px = *(lv_color16_t*)lv_draw_buf_goto_xy(snapshot, x, y);
      *(dst++) = rbScale[px.red];
      *(dst++) = gScale[px.green];
      *(dst++) = rbScale[px.blue];
    }
  }
  lv_draw_buf_destroy(snapshot);

  std::string path = proofRawDir() + "/" + name + ".png";
  stbi_write_png(path.c_str(), w, h, 3, img.data(), w * 3);
}

}  // namespace

// A picker with exactly one section must not materialize any header widget
// -- this is the exact "Select role" / "ROLES" bug, reproduced structurally.
TEST(DesignSystemPicker, LoneSectionRendersHeaderless)
{
  auto* dlg = new (std::nothrow) ds::PickerOverlay("Select role");
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());

  dlg->section("ROLES");
  dlg->option({"Pilot"});
  dlg->option({"Copilot"});
  dlg->option({"Instructor"});
  MainWindow::instance()->runMainLoopTick();

  EXPECT_EQ(dlg->materializedHeaderCountForTest(), 0)
      << "lone section must not realize a header widget";

  dumpScreenshot("ds-picker-one-section-headerless");
  dlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

// A second section proves the picker genuinely needs to differentiate
// groups -- both headers (including the deferred first one) must render.
TEST(DesignSystemPicker, TwoSectionsRenderBothHeaders)
{
  auto* dlg = new (std::nothrow) ds::PickerOverlay("Select role");
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());

  dlg->section("COMMON");
  dlg->option({"Pilot"});
  dlg->option({"Copilot"});
  dlg->section("ADVANCED");
  dlg->option({"Instructor"});
  MainWindow::instance()->runMainLoopTick();

  EXPECT_EQ(dlg->materializedHeaderCountForTest(), 2)
      << "a second section proves both groups need differentiating headers";

  dumpScreenshot("ds-picker-two-sections-both-headers");
  dlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

// Even in a multi-section list, a header that just echoes the overlay title
// (case/plural-insensitive) is suppressed -- it still restates rather than
// differentiates.
TEST(DesignSystemPicker, TitleEchoingHeaderSuppressedAmongMultipleSections)
{
  auto* dlg = new (std::nothrow) ds::PickerOverlay("Battery");
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());

  dlg->section("Batteries");  // plural echo of the title -- suppressed
  dlg->option({"6S 5000mAh"});
  dlg->section("Cells");      // genuinely differentiates -- kept
  dlg->option({"3S"});
  MainWindow::instance()->runMainLoopTick();

  EXPECT_EQ(dlg->materializedHeaderCountForTest(), 1)
      << "title-echoing header must be suppressed even among multiple "
         "sections; the differentiating one must still render";

  dumpScreenshot("ds-picker-title-echo-suppressed");
  dlg->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

#endif  // COLORLCD && SIMU
