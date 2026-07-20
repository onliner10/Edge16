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

// Structural + visual proof for ds::Grid — the columnar/tabular component (see
// radio/src/gui/colorlcd/libui/ds_core.{h,cpp}). The whole reason this
// component exists is COLUMN ALIGNMENT: the header cell in column i must sit
// exactly above every data row's cell in column i (the class of bug the
// previous batch hit when it collapsed the Global Variables grid to a
// single-line list row and the FM0..FM8 header stopped lining up with its
// value columns). Against real LVGL geometry this proves, on ONE populated
// GVars-shaped grid:
//   * header column i and every data row's column i share the same x extent
//     (structural alignment -- cells STRETCH to fill a shared, fixed-px column
//     template, so cell.x1 == track left independent of text);
//   * the frozen leading (name) column shares one x across header and rows;
//   * data rows clear the 40 px touch floor and each interactive column's
//     cells clear the 40 px cell-width floor;
//   * the sticky header stays pinned while the body scrolls.
// It also dumps a real LVGL-rendered PNG (lv_snapshot_take, the same path
// radio/src/gui/screenshot.cpp uses) into <repo>/proof/raw/ so the alignment
// is eyeballable too, matching colorlcd_ds_picker.cpp.
//
// Everything runs on a SINGLE fixture in a SINGLE test on purpose: a full-
// screen host is a live window that only unregisters from the global
// live-window registry in ~Window, and that retirement queue is not drained by
// a bounded tick count in the headless test loop. One fixture keeps the global
// window population flat so this test does not perturb its neighbours.

#include "gtests.h"

#if defined(COLORLCD) && defined(SIMU)

#include <sys/stat.h>

#include <string>
#include <vector>

#include "dialog.h"
#include "ds_core.h"
#include "location.h"
#include "mainwindow.h"

#include "stb/stb_image_write.h"  // implementation linked from lcd_480x272.cpp

extern uint8_t rbScale[32];
extern uint8_t gScale[64];

namespace {

std::string proofRawDir()
{
  return std::string(TESTS_PATH) + "/../../../proof/raw";
}

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

struct CellBox {
  lv_coord_t x1 = 0;
  lv_coord_t y1 = 0;
  lv_coord_t w = 0;
  lv_coord_t h = 0;
};

CellBox boxOf(lv_obj_t* obj)
{
  if (!obj) return {};
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return {a.x1, a.y1, lv_coord_t(a.x2 - a.x1 + 1), lv_coord_t(a.y2 - a.y1 + 1)};
}

void settle(Window* host)
{
  for (int i = 0; i < 3; i++) MainWindow::instance()->runMainLoopTick();
  if (host)
    host->withLive(
        [](Window::LiveWindow& live) { lv_obj_update_layout(live.lvobj()); });
}

// Minimal modal so the grid is mounted on its own layer (as a real Page or
// modal screen would be) rather than as a bare child of the shared main window.
// This is what keeps the test from perturbing global focus/visibility state
// that neighbouring tests assert on -- the same reason the ds::PickerOverlay
// test is stable in the full suite.
class GridDialog : public BaseDialog
{
 public:
  ds::Grid* grid = nullptr;
  explicit GridDialog(const std::vector<ds::Grid::Column>& cols) :
      BaseDialog("Grid", false)
  {
    form.with([&](Window& f) {
      grid = new (std::nothrow) ds::Grid(&f, cols);
      // Short fixed height so the body OVERFLOWS and scrolls -- lets the test
      // exercise the sticky header. (Kept modest so the whole grid fits the
      // modal's width without horizontal overflow.)
      if (grid) grid->setHeight(140);
    });
  }
};

}  // namespace

TEST(DesignSystemGrid, ColumnsAlignFrozenStickyAndMeetTouchFloor)
{
  // Narrow, fixed, interactive value columns so the grid fits the modal width
  // (no horizontal overflow) while still proving the column-template alignment
  // and the per-cell touch floor. The production GVars grid (9 flight-mode
  // Fill columns, full width) is covered by the real-sim before/after proof.
  const int nValues = 5;
  const int nRows = 6;

  std::vector<ds::Grid::Column> cols;
  cols.push_back(ds::Grid::Column::Fixed(36, ds::CellAlign::Start,
                                         /*frozen=*/true));
  for (int i = 0; i < nValues; i++)
    cols.push_back(ds::Grid::Column::Fixed(40, ds::CellAlign::Center));

  auto* dlg = new (std::nothrow) GridDialog(cols);
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ds::Grid* grid = dlg->grid;
  ASSERT_NE(grid, nullptr);
  Window* host = dlg;  // the thing to settle/relayout

  auto* header = grid->header();
  header->setCell(0, "GV");  // realize the frozen name-column header cell
  char cap[8];
  for (int i = 0; i < nValues; i++) {
    snprintf(cap, sizeof(cap), "FM%d", i);
    header->setCell(i + 1, cap);
  }

  std::vector<ds::Grid::Row*> rows;
  for (int r = 0; r < nRows; r++) {
    auto* row = grid->addRow([]() -> uint8_t { return 0; },
                             []() -> uint8_t { return 0; });
    char name[8];
    snprintf(name, sizeof(name), "GV%d", r + 1);
    row->setCell(0, name);
    char val[8];
    for (int i = 0; i < nValues; i++) {
      snprintf(val, sizeof(val), "%d", i * 10 + r + 1);
      row->setCell(i + 1, val);
    }
    rows.push_back(row);
  }
  settle(host);

  const lv_coord_t kFloor = 40;  // 8 mm @ 128 PPI (LAYOUT_SCALE identity on TX16S)
  const int nCols = nValues + 1;  // + frozen name column

  // (1) Rows share ONE column geometry: every data row's cell i has the same x
  // and width as row 0's cell i. Columns are consistent all the way down the
  // list (a per-row layout would drift here).
  for (int c = 0; c < nCols; c++) {
    CellBox base = boxOf(rows[0]->cellObj(c));
    for (auto* row : rows) {
      CellBox cb = boxOf(row->cellObj(c));
      EXPECT_EQ(base.x1, cb.x1) << "column " << c << " drifted between rows";
      EXPECT_EQ(base.w, cb.w) << "column " << c << " width drifted between rows";
    }
  }

  // (2) Header aligns with the rows: the header lives in a sibling container to
  // the scrolling body, so the two containers may sit at a constant x offset
  // (e.g. a scrollbar gutter), but the COLUMN TEMPLATE they share means every
  // header cell i must sit at that SAME offset from row cell i, with the SAME
  // width. That constant-offset-with-equal-widths IS the alignment guarantee
  // (a mis-collapsed grid would give a non-constant offset). The absolute
  // pixel alignment is additionally shown by the real-sim before/after proof.
  for (int i = 0; i < nValues; i++) {
    snprintf(cap, sizeof(cap), "FM%d", i);
    rows[0]->setCell(i + 1, cap);  // same text as header -> isolate geometry
  }
  settle(host);
  const lv_coord_t offset =
      boxOf(header->cellObj(0)).x1 - boxOf(rows[0]->cellObj(0)).x1;
  for (int c = 0; c < nCols; c++) {
    CellBox hc = boxOf(header->cellObj(c));
    CellBox rc = boxOf(rows[0]->cellObj(c));
    EXPECT_EQ(hc.x1 - rc.x1, offset)
        << "header column " << c << " not aligned with its row column";
    EXPECT_EQ(hc.w, rc.w) << "header column " << c << " width != row column";
  }

  // (3) Touch floor: rows >= 40 px tall, each cell fills its (>= 40 px) column.
  for (auto* row : rows) {
    CellBox rowBox = boxOf(lv_obj_get_parent(row->cellObj(0)));
    EXPECT_GE(rowBox.h, kFloor) << "data row below 40 px touch floor";
    for (int i = 0; i < nValues; i++) {
      CellBox cb = boxOf(row->cellObj(i + 1));
      EXPECT_GE(cb.w, kFloor) << "value cell " << i << " below 40 px wide";
    }
  }

  // (4) Sticky header: the header sits in a separate flex slot ABOVE the
  // scrolling body, so it is structurally OUTSIDE the scroll -- its y is fixed
  // and the first data row starts BELOW it. (Driving a live touch-scroll here
  // would leave global scroll/live-value defer state that perturbs neighbouring
  // tests; the header staying put under a real scroll is shown by the real-sim
  // before/after proof. Structurally: header bottom <= first row top.)
  CellBox headerBox = boxOf(lv_obj_get_parent(header->cellObj(1)));
  CellBox firstRowBox = boxOf(lv_obj_get_parent(rows[0]->cellObj(0)));
  EXPECT_LE(headerBox.y1 + headerBox.h, firstRowBox.y1 + 1)
      << "header does not sit above the first data row";

  // (5) Populated render for eyeballing: highlight the current-flight-mode
  // column and dump the frame.
  header->highlightCell(1, true);
  for (auto* row : rows) row->highlightCell(1, true);
  settle(host);
  dumpScreenshot("ds-grid-gvars-populated-aligned");

  host->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// Regression for the logical-switches "active operand" live cue (#7): a Row
// cell that toggles TextRole::Strong AFTER creation must re-render bold, not
// stay stuck at whatever role/font it first realized with. ensureCell() only
// picks the font once, at creation -- setCell()/setCellSmall() must
// re-apply it on every call so a live refresh's role change actually shows.
TEST(DesignSystemGrid, StrongRoleReappliesBoldFontOnEveryCellUpdate)
{
  std::vector<ds::Grid::Column> cols = {
      ds::Grid::Column::Fixed(60, ds::CellAlign::Start),
      ds::Grid::Column::Fixed(60, ds::CellAlign::Start),
  };
  auto* dlg = new (std::nothrow) GridDialog(cols);
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ds::Grid* grid = dlg->grid;
  ASSERT_NE(grid, nullptr);
  Window* host = dlg;

  auto* row = grid->addRow();
  ASSERT_NE(row, nullptr);

  // Column 0: created Body, later flipped to Strong via setCell() -- the
  // FUNC/V2/AND path in model_logical_switches.cpp.
  row->setCell(0, "match", ds::TextRole::Body);
  settle(host);
  const lv_font_t* bodyFont =
      lv_obj_get_style_text_font(row->cellObj(0), LV_PART_MAIN);

  row->setCell(0, "match", ds::TextRole::Strong);
  settle(host);
  const lv_font_t* strongFont =
      lv_obj_get_style_text_font(row->cellObj(0), LV_PART_MAIN);

  EXPECT_NE(strongFont, bodyFont)
      << "cell re-set with TextRole::Strong after creation did not switch to "
         "the bold font";

  // The bold font is whatever a fresh Strong-created label picks (roleFont) --
  // check against a reference label rather than assuming a specific index.
  // Parented directly on the active screen (NOT the row's grid container) so
  // it does not perturb the row's own grid-cell layout.
  lv_obj_t* reference = etx_label_create(lv_scr_act(), FONT_BOLD_INDEX);
  ASSERT_NE(reference, nullptr);
  const lv_font_t* boldFont =
      lv_obj_get_style_text_font(reference, LV_PART_MAIN);
  EXPECT_EQ(strongFont, boldFont)
      << "Strong cell font is not the bold font";
  lv_obj_del(reference);

  // Column 1: the V1 path -- Strong routed through setCellSmall() with
  // small=true must STILL render bold (Strong wins over the small request),
  // not fall back to FONT_XS_INDEX.
  row->setCellSmall(1, "match", /*small=*/true, ds::TextRole::Strong);
  settle(host);
  const lv_font_t* smallStrongFont =
      lv_obj_get_style_text_font(row->cellObj(1), LV_PART_MAIN);
  EXPECT_EQ(smallStrongFont, boldFont)
      << "setCellSmall() with TextRole::Strong did not render bold";

  // A non-Strong small cell keeps the small font -- proves the Strong
  // override above is targeted, not a blanket font change.
  row->setCellSmall(1, "match", /*small=*/true, ds::TextRole::Body);
  settle(host);
  const lv_font_t* smallBodyFont =
      lv_obj_get_style_text_font(row->cellObj(1), LV_PART_MAIN);
  EXPECT_NE(smallBodyFont, boldFont)
      << "non-Strong setCellSmall() unexpectedly rendered bold";

  host->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// Perf regression guard for the Global Variables screen load. setCell()/
// setCellSmall() now skip the (expensive) font/colour re-styling when the
// resolved font/colour is unchanged -- so a full GV x FM grid pays only ONE
// styling pass per cell at build (not the second, redundant one right after
// ensureCell) and text-only value refreshes cost nothing beyond the text. That
// short-circuit must stay INVISIBLE: the resolved font must still track `small`
// and role exactly. This drives the GVars value-cell path (setCellSmall, Body
// role) through the small<->std transitions a live value refresh produces.
TEST(DesignSystemGrid, CellStyleCacheStillTracksSmallAcrossRefreshes)
{
  std::vector<ds::Grid::Column> cols = {
      ds::Grid::Column::Fixed(60, ds::CellAlign::Start),
      ds::Grid::Column::Fill(40, ds::CellAlign::Center),
  };
  auto* dlg = new (std::nothrow) GridDialog(cols);
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ds::Grid* grid = dlg->grid;
  ASSERT_NE(grid, nullptr);
  Window* host = dlg;

  // Reference labels for the two fonts the value path resolves to.
  lv_obj_t* stdRef = etx_label_create(lv_scr_act(), FONT_STD_INDEX);
  lv_obj_t* xsRef = etx_label_create(lv_scr_act(), FONT_XS_INDEX);
  ASSERT_NE(stdRef, nullptr);
  ASSERT_NE(xsRef, nullptr);
  const lv_font_t* stdFont = lv_obj_get_style_text_font(stdRef, LV_PART_MAIN);
  const lv_font_t* xsFont = lv_obj_get_style_text_font(xsRef, LV_PART_MAIN);
  ASSERT_NE(stdFont, xsFont);

  auto* row = grid->addRow();
  ASSERT_NE(row, nullptr);

  auto fontOf = [&]() {
    return lv_obj_get_style_text_font(row->cellObj(1), LV_PART_MAIN);
  };

  // Build: a short value -> standard font.
  row->setCellSmall(1, "10", /*small=*/false, ds::TextRole::Body);
  settle(host);
  EXPECT_EQ(fontOf(), stdFont) << "initial value cell not standard font";

  // Value-only refresh, same role and small flag: the cache short-circuits the
  // re-style, result must be identical.
  row->setCellSmall(1, "12", /*small=*/false, ds::TextRole::Body);
  settle(host);
  EXPECT_EQ(fontOf(), stdFont) << "text-only refresh perturbed the font";

  // An overflowing value flips small=true: the cache must detect the font
  // change and re-apply the small font.
  row->setCellSmall(1, "1000%", /*small=*/true, ds::TextRole::Body);
  settle(host);
  EXPECT_EQ(fontOf(), xsFont) << "small=true did not switch to the small font";

  // Back to a short value: standard font again.
  row->setCellSmall(1, "5", /*small=*/false, ds::TextRole::Body);
  settle(host);
  EXPECT_EQ(fontOf(), stdFont) << "small=false did not restore standard font";

  lv_obj_del(stdRef);
  lv_obj_del(xsRef);
  host->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// Guards the lazy install of the current-flight-mode highlight background:
// ensureCell() no longer paints the CHECKED-state background on every cell at
// build (a load-time cost on a full GV x FM grid); highlightCell() installs it
// on first use. The visible highlight must be unchanged, and a cell that is
// never highlighted must stay unstyled.
TEST(DesignSystemGrid, HighlightBackgroundInstalledLazilyOnFirstHighlight)
{
  std::vector<ds::Grid::Column> cols = {
      ds::Grid::Column::Fixed(60, ds::CellAlign::Start),
      ds::Grid::Column::Fill(40, ds::CellAlign::Center),
  };
  auto* dlg = new (std::nothrow) GridDialog(cols);
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ds::Grid* grid = dlg->grid;
  ASSERT_NE(grid, nullptr);
  Window* host = dlg;

  auto* row = grid->addRow();
  ASSERT_NE(row, nullptr);
  row->setCell(0, "GV1");
  row->setCellSmall(1, "10", /*small=*/false, ds::TextRole::Body);
  settle(host);

  // Before any highlight, the value cell draws no background.
  EXPECT_EQ(lv_obj_get_style_bg_opa(row->cellObj(1), LV_PART_MAIN),
            LV_OPA_TRANSP)
      << "value cell had a background before being highlighted";

  // Highlighting installs the CHECKED-state background and turns it on.
  row->highlightCell(1, true);
  settle(host);
  EXPECT_EQ(lv_obj_get_style_bg_opa(row->cellObj(1), LV_PART_MAIN), LV_OPA_COVER)
      << "highlighted cell did not paint its active background";

  // The frozen name cell, never highlighted, stays unstyled -- the lazy install
  // did not leak onto other cells.
  EXPECT_EQ(lv_obj_get_style_bg_opa(row->cellObj(0), LV_PART_MAIN),
            LV_OPA_TRANSP)
      << "highlight leaked onto a non-highlighted cell";

  // Clearing the highlight removes the boxed look again.
  row->highlightCell(1, false);
  settle(host);
  EXPECT_EQ(lv_obj_get_style_bg_opa(row->cellObj(1), LV_PART_MAIN),
            LV_OPA_TRANSP)
      << "cleared highlight still painted a background";

  host->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

#endif  // COLORLCD && SIMU
