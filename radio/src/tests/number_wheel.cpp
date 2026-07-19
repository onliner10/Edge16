/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "gtests.h"

#include "mainwindow.h"
#include "numberedit.h"
#include "number_wheel.h"

// Test the static NumberWheel::canOpen threshold and option generation logic
// without creating LVGL objects.

// Thin wrapper so tests use the same return type as before.
static std::vector<std::pair<int, std::string>> buildOptions(NumberEdit* edit)
{
  auto raw = NumberWheel::buildOptionsFor(edit);
  std::vector<std::pair<int, std::string>> result;
  result.reserve(raw.size());
  for (auto& o : raw) result.push_back({o.rawValue, o.label});
  return result;
}

// ---- canOpen threshold ----

TEST(NumberWheel, CanOpenSmallRange)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 10,
                              []() { return 5; }, nullptr);
  EXPECT_TRUE(NumberWheel::canOpen(edit));
}

TEST(NumberWheel, CanOpenTinyRange)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 2,
                              []() { return 0; }, nullptr);
  EXPECT_TRUE(NumberWheel::canOpen(edit));
}

TEST(NumberWheel, CannotOpenHugeRange)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  // 0..35100 exceeds both K=10 (3511 coarse > 350) and K=100 (351 coarse > 350)
  // — too large even for the additive split wheel.
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 35100,
                              []() { return 0; }, nullptr);
  EXPECT_FALSE(NumberWheel::canOpen(edit));
}

// ---- Option generation: small range, no fake padding ----

TEST(NumberWheel, SmallRangeExactOptions)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 2,
                              []() { return 0; }, nullptr);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 3u);
  EXPECT_EQ(opts[0].first, 0);
  EXPECT_EQ(opts[1].first, 1);
  EXPECT_EQ(opts[2].first, 2);
}

TEST(NumberWheel, TinyRangeUID)
{
  // ACCESS UID is 0..2 — must show exactly 0, 1, 2 (no padding)
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 2,
                              []() { return 0; }, nullptr);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 3u);
  EXPECT_EQ(opts[0].first, 0);
  EXPECT_EQ(opts[1].first, 1);
  EXPECT_EQ(opts[2].first, 2);
  // No fake values
  for (auto& o : opts) {
    EXPECT_GE(o.first, 0);
    EXPECT_LE(o.first, 2);
  }
}

TEST(NumberWheel, MixWarningRange)
{
  // Mix warning is 0..3 — must show exactly 0, 1, 2, 3
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 3,
                              []() { return 0; }, nullptr);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 4u);
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(opts[i].first, i);
  }
}

// ---- Step handling ----

TEST(NumberWheel, StepDividesRange)
{
  // Beep pitch: 0..300, step 15
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 300,
                              []() { return 150; }, nullptr);
  edit->setStep(15);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 21u);  // 0, 15, 30, ..., 300
  EXPECT_EQ(opts[0].first, 0);
  EXPECT_EQ(opts[1].first, 15);
  EXPECT_EQ(opts[20].first, 300);
}

TEST(NumberWheel, StepUnalignedMin)
{
  // min=3, max=13, step=5 → should offer 3, 8, 13
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 3, 13,
                              []() { return 3; }, nullptr);
  edit->setStep(5);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 3u);
  EXPECT_EQ(opts[0].first, 3);
  EXPECT_EQ(opts[1].first, 8);
  EXPECT_EQ(opts[2].first, 13);
}

TEST(NumberWheel, NegativeStepRange)
{
  // Vario range: -17..17, step 1
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, -17, 17,
                              []() { return 0; }, nullptr);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 35u);  // -17..17 inclusive
  EXPECT_EQ(opts[0].first, -17);
  EXPECT_EQ(opts[17].first, 0);
  EXPECT_EQ(opts[34].first, 17);
}

// ---- PREC1 formatting ----

TEST(NumberWheel, Prec1BatteryWarning)
{
  // Battery warning: 30..120 (3.0V..12.0V), PREC1
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 30, 120,
                              []() { return 70; }, nullptr, PREC1);
  edit->setSuffix("V");
  auto opts = buildOptions(edit);
  // 30..120 = 91 options — exactly the valid range
  ASSERT_EQ(opts.size(), 91u);
  EXPECT_EQ(opts[0].first, 30);
  EXPECT_EQ(opts[90].first, 120);
  // First option should show formatted value "3.0V" (PREC1: 30/10 = 3.0)
  EXPECT_FALSE(opts[0].second.empty());
  EXPECT_NE(opts[0].second.find("3.0"), std::string::npos);
  // Last option should show "12.0V"
  EXPECT_NE(opts[90].second.find("12.0"), std::string::npos);
}

TEST(NumberWheel, Prec1SignedVarioCenter)
{
  // Vario center: -15..15 (PREC1), display -1.5..1.5
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, -15, 15,
                              []() { return 0; }, nullptr, PREC1);
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 31u);  // -15..15 inclusive
  EXPECT_EQ(opts[0].first, -15);
  EXPECT_EQ(opts[15].first, 0);
  EXPECT_EQ(opts[30].first, 15);
  // Verify negative values are present and distinct
  EXPECT_NE(opts[0].second, opts[1].second);
}

// ---- Zero text ----

TEST(NumberWheel, ZeroText)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 3,
                              []() { return 0; }, nullptr);
  edit->setZeroText("OFF");
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 4u);
  // The zero-value option should show "OFF" as its label
  EXPECT_EQ(opts[0].first, 0);
  EXPECT_EQ(opts[0].second, "OFF");
}

// ---- Availability handler ----

TEST(NumberWheel, AvailabilityFiltering)
{
  // Simulate vario min/max constraint: values > 5 are unavailable
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 10,
                              []() { return 3; }, nullptr);
  edit->setAvailableHandler([](int val) { return val <= 5; });
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 6u);  // 0, 1, 2, 3, 4, 5
  for (auto& o : opts) {
    EXPECT_LE(o.first, 5);
  }
}

// ---- Step with availability ----

TEST(NumberWheel, StepWithAvailability)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 100,
                              []() { return 50; }, nullptr);
  edit->setStep(10);
  edit->setAvailableHandler([](int val) { return val != 30; });
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 10u);  // 0,10,20,40,50,60,70,80,90,100 (30 excluded)
  for (auto& o : opts) {
    EXPECT_NE(o.first, 30);
    EXPECT_EQ(o.first % 10, 0);
  }
}

// ---- Backlight delay: stepped with suffix ----

TEST(NumberWheel, SteppedWithSuffix)
{
  // Backlight delay: 5..600, step 5
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 5, 600,
                              []() { return 60; }, nullptr);
  edit->setStep(5);
  edit->setSuffix("s");
  auto opts = buildOptions(edit);
  ASSERT_EQ(opts.size(), 120u);  // 5, 10, 15, ..., 600
  EXPECT_EQ(opts[0].first, 5);
  EXPECT_EQ(opts[119].first, 600);
  EXPECT_NE(opts[0].second.find("5"), std::string::npos);
}

// ---- WheelLayout split logic ----

TEST(NumberWheel, SingleColumnUnchanged)
{
  // 0..10 stays single-column, identical to buildOptionsFor
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 10,
                              []() { return 5; }, nullptr);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.valid());
  EXPECT_FALSE(layout.split());
  ASSERT_EQ(layout.columns.size(), 1u);
  auto opts = buildOptions(edit);
  ASSERT_EQ(layout.columns[0].size(), opts.size());
  for (size_t i = 0; i < opts.size(); i++) {
    EXPECT_EQ(layout.columns[0][i].rawValue, opts[i].first);
  }
}

TEST(NumberWheel, NoSplitWithDisplayFunction)
{
  // A huge range with a custom display handler must not produce a split layout.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 1000, 2000,
                              []() { return 1500; }, nullptr);
  edit->setDisplayHandler([](int v) { return std::to_string(v) + "us"; });
  auto layout = NumberWheel::buildLayoutFor(edit);
  EXPECT_FALSE(layout.valid());
}

TEST(NumberWheel, NoSplitWithAvailability)
{
  // A huge range with an availability handler must not produce a split layout.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 1000, 2000,
                              []() { return 1500; }, nullptr);
  edit->setAvailableHandler([](int v) { return v % 3 == 0; });
  auto layout = NumberWheel::buildLayoutFor(edit);
  EXPECT_FALSE(layout.valid());
}

TEST(NumberWheel, SplitPpmCenterRange)
{
  // PPM-style 1000..2000, step 1, no precision.
  // fineUnit=1, K=10 → coarse bases 1000,1010,…,1990 (100 entries) + appended
  // grid-aligned top base 2000 (vmax is on the coarse grid) = 101 coarse
  // entries.  The top base is 2000 (not the off-grid 1991 overlap base) so every
  // coarse label stays on the fine grid and a single detent is a uniform step.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 1000, 2000,
                              []() { return 1500; }, nullptr);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.valid());
  ASSERT_TRUE(layout.split());
  const auto& coarse = layout.columns[0];
  const auto& fine = layout.columns[1];

  EXPECT_EQ(coarse.size(), 101u);
  EXPECT_EQ(coarse.front().rawValue, 1000);
  EXPECT_EQ(coarse[99].rawValue, 1990);    // last regular base
  EXPECT_EQ(coarse.back().rawValue, 2000); // appended grid-aligned top base

  EXPECT_EQ(fine.size(), 10u);
  EXPECT_EQ(fine.front().label, "+0");
  EXPECT_EQ(fine.back().label, "+9");

  // vmax is reached cleanly as top base + fine 0 (no off-grid residue).
  EXPECT_EQ(NumberWheel::composeValue(layout, {100, 0}), 2000);
}

TEST(NumberWheel, SplitTopBaseIsGridAligned)
{
  // Regression for the two-column detent bug: when vmax lands on the coarse
  // grid, the top coarse base must equal vmax (fine offset 0) so the highlighted
  // row after Reset reads cleanly (e.g. "100.0", not the old off-grid "99.1")
  // and every coarse base is a whole multiple of the coarse span.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  // Output Max field: 0..1000 PREC1 → display 0.0 .. 100.0.
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              []() { return 1000; }, nullptr, PREC1);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());
  const auto& coarse = layout.columns[0];

  // Every coarse base is on the 10-raw (=1.0 display) grid.
  for (const auto& c : coarse) EXPECT_EQ(c.rawValue % 10, 0);
  EXPECT_EQ(coarse.back().rawValue, 1000);

  // vmax decomposes to the top base with fine offset 0 -> clean highlight.
  auto idxs = NumberWheel::decomposeValue(layout, 1000);
  ASSERT_EQ(idxs.size(), 2u);
  EXPECT_EQ(idxs[1], 0);  // fine column highlights "+.0"
  EXPECT_EQ(coarse[idxs[0]].rawValue, 1000);
  EXPECT_NE(coarse[idxs[0]].label.find("100.0"), std::string::npos);
}

TEST(NumberWheel, SplitNegativePrec1Limits)
{
  // Extended limits -1500..1500, PREC1.
  // fineUnit = getPrecisionScale()/10 = 10/10 = 1, K=10.
  // Loop: bases -1500,-1490,...,1490 (300 entries) + appended 1491 = 301 total.
  // compose(-1500, +7) = -1493 → displays "-149.3"
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, -1500, 1500,
                              []() { return 0; }, nullptr, PREC1);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.valid());
  ASSERT_TRUE(layout.split());
  const auto& coarse = layout.columns[0];
  const auto& fine = layout.columns[1];

  EXPECT_EQ(coarse.size(), 301u);
  EXPECT_EQ(coarse.front().rawValue, -1500);

  // Fine labels for PREC1 fields use "+.N" form
  ASSERT_EQ(fine.size(), 10u);
  EXPECT_EQ(fine[7].label, "+.7");

  // THE negative test: no sign bug
  int composed = NumberWheel::composeValue(layout, {0, 7});  // base=-1500, offset=+7
  EXPECT_EQ(composed, -1493);
  EXPECT_NE(edit->getDisplayValFor(composed).find("-149.3"), std::string::npos);

  // Coarse label of base -1500 should contain "-150.0"
  EXPECT_NE(coarse.front().label.find("-150.0"), std::string::npos);
}

TEST(NumberWheel, SplitRoundTrip)
{
  // For every test value, decompose then compose should give the original.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 1000, 2000,
                              []() { return 1500; }, nullptr);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());

  for (int v : {1000, 1001, 1009, 1010, 1500, 1999, 2000}) {
    auto idxs = NumberWheel::decomposeValue(layout, v);
    EXPECT_EQ(NumberWheel::composeValue(layout, idxs), v)
        << "round-trip failed for v=" << v;
  }

  // Also test the negative PREC1 range
  auto* edit2 = new NumberEdit(w, rect_t{0, 0, 100, 30}, -1500, 1500,
                               []() { return 0; }, nullptr, PREC1);
  auto layout2 = NumberWheel::buildLayoutFor(edit2);
  ASSERT_TRUE(layout2.split());

  for (int v : {-1500, -1499, -1, 0, 1, 999, 1499, 1500}) {
    auto idxs = NumberWheel::decomposeValue(layout2, v);
    EXPECT_EQ(NumberWheel::composeValue(layout2, idxs), v)
        << "round-trip failed for v=" << v;
  }
}

TEST(NumberWheel, SplitPrec2)
{
  // PREC2 field 0..5000: fineUnit = getPrecisionScale()/10 = 100/10 = 10.
  // K=10: coarse bases 0,100,200,...  fine offsets 0,10,20,...,90.
  // Fine label at i=3: "+.3"; compose(base=0, i=3) = 0+30 = 30 raw → displays "0.3".
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 5000,
                              []() { return 0; }, nullptr, PREC2);
  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.valid());
  ASSERT_TRUE(layout.split());

  const auto& fine = layout.columns[1];
  ASSERT_GE(fine.size(), 4u);
  EXPECT_EQ(fine[3].label, "+.3");

  int composed = NumberWheel::composeValue(layout, {0, 3});  // base=0, offset=30
  EXPECT_EQ(composed, 30);
  // getDisplayValFor(30) with PREC2: displayValue = (30+5)/10 = 3, PREC1 → "0.3"
  EXPECT_NE(edit->getDisplayValFor(composed).find("0.3"), std::string::npos);
}

// ---- PREC2 deduplication ----

TEST(NumberWheel, Prec2DuplicatesDeduped)
{
  // Delay field with PREC2 step=1: display collapses PREC2 → PREC1 via
  // displayValue = (raw + 5) / 10. So raw 0..4 → "0.0s", raw 5..14 → "0.1s", etc.
  // The wheel must deduplicate to one entry per distinct displayed value.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 250,
                              []() { return 0; }, nullptr, PREC2);
  edit->setSuffix("s");
  auto opts = buildOptions(edit);
  // Groups: "0.0" (raw 0-4), "0.1" (raw 5-14), ..., "25.0" (raw 245-250) = 26
  // Ceiling-median selection: "0.0" group → raw (0+5)/2=2; "0.1" group → raw (5+15)/2=10.
  ASSERT_EQ(opts.size(), 26u);
  EXPECT_EQ(opts[0].first, 2);   // median of [0..4]
  EXPECT_EQ(opts[1].first, 10);  // median of [5..14] — represents exactly 0.10 s
  // All labels must be unique
  for (size_t i = 1; i < opts.size(); i++) {
    EXPECT_NE(opts[i].second, opts[i - 1].second);
  }
}

// ---- Touch-path roller normalization (dead positions + stale-offset jump) --
//
// buildLayoutFor() appends a grid-aligned top coarse base (alignedTop = vmin
// + ((vmax-vmin)/span)*span) whenever (vmax-vmin) is an exact multiple of the
// coarse span -- true for this 0..1000 PREC1 field (same one exercised by
// SplitTopBaseIsGridAligned above).  alignedTop == vmax there, so at that top
// coarse row every nonzero fine offset composes PAST vmax.  The rotary path
// (onWheelEncoder) is immune because it always decomposes its clamped target
// and re-selects every roller; these tests prove the touch path
// (onRollerChanged) now does the same normalization.

TEST(NumberWheel, TouchFineOvershootAtTopNormalizesToZero)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 1000;
  // Output Max field: 0..1000 PREC1 -> display 0.0..100.0.
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());
  int topCoarseIdx = (int)layout.columns[0].size() - 1;
  ASSERT_EQ(layout.columns[0][topCoarseIdx].rawValue, 1000);

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);

  // Opens on vmax (modelVal == 1000): coarse starts on the top base, fine on
  // offset 0.
  EXPECT_EQ(wheel->getRollerSelectedForTest(0), topCoarseIdx);
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), 0);

  // Touch-drag the fine roller alone to a nonzero offset ("+.5", raw 5).
  // Composed = 1000 (top base) + 5 = 1005, past vmax -- pre-fix this was a
  // dead position: setValue() clamped the stored value back to vmax but
  // nothing corrected the fine roller's own selection, so it stayed pinned
  // on "+.5" forever (every touch there silently no-oped, still played a
  // confirm click).
  wheel->touchRollerForTest(1, 5);

  // setValue() clamps the stored value to vmax...
  EXPECT_EQ(modelVal, 1000);
  // ...and the fine roller must normalize back to offset 0 instead of
  // staying stuck on the unreachable "+.5" position.
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), 0);
  // The untouched coarse roller is left exactly where it was.
  EXPECT_EQ(wheel->getRollerSelectedForTest(0), topCoarseIdx);

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

TEST(NumberWheel, TouchCoarseAfterOvershootClampHasNoStaleFineAddend)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 1000;
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());
  int topCoarseIdx = (int)layout.columns[0].size() - 1;
  int prevCoarseIdx = topCoarseIdx - 1;
  ASSERT_EQ(layout.columns[0][prevCoarseIdx].rawValue, 990);

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);

  // Reproduce the overshoot clamp from the previous test: drag the fine
  // roller to a nonzero offset while coarse sits on the grid-aligned top
  // base (== vmax).  The fix normalizes fine back to offset 0 here -- that
  // normalization is exactly what the rest of this test proves prevents the
  // stale-offset jump below.
  wheel->touchRollerForTest(1, 5);
  ASSERT_EQ(modelVal, 1000);
  ASSERT_EQ(wheel->getRollerSelectedForTest(1), 0);

  // Now touch-drag ONLY the coarse roller down one step (100.0 -> 99.0).
  // Pre-fix, the fine roller's selection was never re-synced after the
  // overshoot clamp above, so it stayed at index 5 ("+.5") and this compose
  // silently became 990 + 5 = 995 ("99.5") -- a discontinuous jump the user
  // never asked for from a single coarse-only drag.
  wheel->touchRollerForTest(0, prevCoarseIdx);

  // Exact coarse value, no stale fine addend.
  EXPECT_EQ(modelVal, 990);
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), 0);

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

// ---- Touch-path coarse-drag fine-preservation (precision-loss fix) ------
//
// The two tests above prove the touch path re-derives EVERY column's
// selection from the canonical decomposition of the clamped stored value
// whenever a drag overshoots.  That is correct when the FINE roller is the
// one that overshot (canonical decomposition of vmax always has fine offset
// 0, so resetting fine to 0 is the right answer -- TouchFineOvershoot...
// above).  But applied the same way to a COARSE overshoot it has a bad side
// effect: dragging the coarse roller up onto the grid-aligned top base
// (== vmax) while the fine roller sits on a nonzero offset also composes
// past vmax, and re-deriving from scratch silently wipes that fine offset --
// then dragging coarse back down one step lands on "99.0" instead of the
// user's "99.6", permanently losing the fine digit.  onRollerChanged() now
// slides only the column that just moved (identified by the event's own
// target) to the furthest index that keeps the composed value in range,
// leaving every other column's selection untouched.  These tests prove the
// coarse roller now behaves the same way: its reachable range is implicitly
// capped by the current fine offset instead of resetting that offset.

TEST(NumberWheel, TouchCoarseDragUpThroughCeilingPreservesFineOffset)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 936;  // 93.6
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());
  const auto& coarse = layout.columns[0];
  const auto& fine = layout.columns[1];
  int topCoarseIdx = (int)coarse.size() - 1;
  int prevCoarseIdx = topCoarseIdx - 1;
  ASSERT_EQ(coarse[topCoarseIdx].rawValue, 1000);
  ASSERT_EQ(coarse[prevCoarseIdx].rawValue, 990);

  auto startIdxs = NumberWheel::decomposeValue(layout, 936);
  ASSERT_EQ(startIdxs.size(), 2u);
  int fineIdx = startIdxs[1];
  ASSERT_EQ(fine[fineIdx].rawValue, 6);  // fine roller starts on "+.6"

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);
  EXPECT_EQ(wheel->getRollerSelectedForTest(0), startIdxs[0]);
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), fineIdx);

  // Drag the coarse roller up one step at a time, all the way to (and one
  // past) the ceiling.  At every step: the stored value must never exceed
  // vmax, and the fine roller's own selection -- the user's ".6" -- must
  // never move, since only the coarse roller was touched.
  for (int idx = startIdxs[0] + 1; idx <= topCoarseIdx; idx++) {
    wheel->touchRollerForTest(0, idx);
    EXPECT_LE(modelVal, 1000) << "coarse idx " << idx;
    EXPECT_EQ(wheel->getRollerSelectedForTest(1), fineIdx)
        << "fine offset lost at coarse idx " << idx;
  }

  // The coarse roller could not reach the top base (1000 + ".6" would
  // overshoot vmax), so it must have been clamped one row below it: composed
  // value 99.6 (raw 996) -- not reset to 100.0, and not dropped to 99.0.
  EXPECT_EQ(modelVal, 996);
  EXPECT_EQ(wheel->getRollerSelectedForTest(0), prevCoarseIdx);
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), fineIdx);

  // Explicitly repeat the exact steps from the bug report: drag coarse to
  // the ceiling, then back down one -- must land on 99.6, not 99.0.
  wheel->touchRollerForTest(0, topCoarseIdx);
  EXPECT_EQ(modelVal, 996);
  wheel->touchRollerForTest(0, prevCoarseIdx);
  EXPECT_EQ(modelVal, 996);
  EXPECT_EQ(wheel->getRollerSelectedForTest(1), fineIdx);

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

TEST(NumberWheel, TouchFineNoDeadPositionsBelowCeiling)
{
  // One coarse row below the vmax ceiling: every fine offset here is fully
  // reachable (990 + up to +.9 = 999.9 <= vmax), so none of them should be
  // clamped away -- unlike the top row, where only "+.0" is reachable.
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 990;  // 99.0
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto layout = NumberWheel::buildLayoutFor(edit);
  ASSERT_TRUE(layout.split());
  const auto& coarse = layout.columns[0];
  const auto& fine = layout.columns[1];
  int prevCoarseIdx = (int)coarse.size() - 2;
  ASSERT_EQ(coarse[prevCoarseIdx].rawValue, 990);

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);
  ASSERT_EQ(wheel->getRollerSelectedForTest(0), prevCoarseIdx);

  std::vector<int> seen;
  for (int i = 0; i < (int)fine.size(); i++) {
    wheel->touchRollerForTest(1, i);
    EXPECT_EQ(modelVal, 990 + i) << "fine idx " << i;
    EXPECT_GE(modelVal, edit->getMin());
    EXPECT_LE(modelVal, edit->getMax());
    // Only the fine roller was touched -- coarse must stay put.
    EXPECT_EQ(wheel->getRollerSelectedForTest(0), prevCoarseIdx);
    for (int prior : seen) EXPECT_NE(prior, modelVal);
    seen.push_back(modelVal);
  }

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

// ---- Rotary path: unaffected by the touch-path change above -------------
//
// onWheelEncoder() drives the hardware-encoder path.  It never goes through
// onRollerChanged() -- lv_roller_set_selected() never emits
// LV_EVENT_VALUE_CHANGED -- so it always computed its own clamped target and
// re-derived every column's selection from it, independently of the touch-
// path fix above.  These tests simulate a real encoder detent exactly the
// way LVGL delivers it (see NumberWheel::rotateEncoderForTest) to confirm
// that path is untouched: uniform per-detent steps, correct carry between
// the coarse and fine columns, and exact clamping at the true vmax/vmin.

TEST(NumberWheel, RotaryEncoderStepsCarryAndClampAtVmax)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 990;  // 99.0
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);
  ASSERT_EQ(modelVal, 990);

  // One detent moves the composed value by exactly one fineStepRaw (here 1
  // raw unit, "0.1"), carrying from the fine column into the coarse column
  // at the row boundary (99.9 -> 100.0), and clamps exactly at the true
  // vmax.
  for (int i = 1; i <= 10; i++) {
    wheel->rotateEncoderForTest(1, LV_KEY_RIGHT);
    EXPECT_EQ(modelVal, 990 + i) << "detent " << i;
  }
  EXPECT_EQ(modelVal, 1000);  // reached vmax exactly via the carry

  // Further RIGHT detents at the ceiling (from either column's roller) must
  // never push the stored value past vmax.
  wheel->rotateEncoderForTest(1, LV_KEY_RIGHT);
  EXPECT_EQ(modelVal, 1000);
  wheel->rotateEncoderForTest(0, LV_KEY_RIGHT);
  EXPECT_EQ(modelVal, 1000);

  // LEFT detents step back down by the same fineStepRaw, carrying the other
  // way at the row boundary.
  for (int i = 1; i <= 10; i++) {
    wheel->rotateEncoderForTest(1, LV_KEY_LEFT);
    EXPECT_EQ(modelVal, 1000 - i) << "detent " << i;
  }
  EXPECT_EQ(modelVal, 990);

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}

TEST(NumberWheel, RotaryEncoderClampsAtVmin)
{
  auto w = Window::makeLive<Window>(nullptr, rect_t{});
  int modelVal = 4;  // 0.4
  auto* edit = new NumberEdit(w, rect_t{0, 0, 100, 30}, 0, 1000,
                              [&]() { return modelVal; },
                              [&](int v) { modelVal = v; }, PREC1);

  auto* wheel = new NumberWheel(edit);
  ASSERT_NE(wheel, nullptr);
  ASSERT_EQ(modelVal, 4);

  for (int i = 1; i <= 4; i++) {
    wheel->rotateEncoderForTest(1, LV_KEY_LEFT);
    EXPECT_EQ(modelVal, 4 - i) << "detent " << i;
  }
  EXPECT_EQ(modelVal, 0);  // reached the true vmin exactly

  // Further LEFT detents must never push the stored value below vmin.
  wheel->rotateEncoderForTest(1, LV_KEY_LEFT);
  EXPECT_EQ(modelVal, 0);
  wheel->rotateEncoderForTest(0, LV_KEY_LEFT);
  EXPECT_EQ(modelVal, 0);

  wheel->deleteLater();
  MainWindow::instance()->runMainLoopTick();
}
