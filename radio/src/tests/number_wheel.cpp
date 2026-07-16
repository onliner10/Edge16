/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "gtests.h"

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
