/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Locks the shared 35%/20% warning bands and the per-cell charge helper that
 * are the SINGLE source of truth for both the Battery Monitor widget and the
 * state-aware telemetry-voltage Value widget.
 */

#include "gtests.h"
#include "telemetry/battery_monitor.h"

TEST(BatteryWarningBands, remainingWarningLevelThresholds)
{
  // No usable reading -> neutral (no guessing).
  EXPECT_EQ(flightBatteryRemainingWarningLevel(-1), 0);
  // Normal band.
  EXPECT_EQ(flightBatteryRemainingWarningLevel(100), 0);
  EXPECT_EQ(flightBatteryRemainingWarningLevel(36), 0);
  // Warning band: <= 35%.
  EXPECT_EQ(flightBatteryRemainingWarningLevel(35), 1);
  EXPECT_EQ(flightBatteryRemainingWarningLevel(21), 1);
  // Critical band: <= 20%.
  EXPECT_EQ(flightBatteryRemainingWarningLevel(20), 2);
  EXPECT_EQ(flightBatteryRemainingWarningLevel(0), 2);
}

TEST(BatteryWarningBands, voltageRemainingPercentRejectsUnusable)
{
  // Non-positive per-cell reading is unusable -> -1 (caller renders neutral).
  EXPECT_EQ(flightBatteryVoltageRemainingPercent(0, BATTERY_TYPE_LIPO), -1);
  EXPECT_EQ(flightBatteryVoltageRemainingPercent(-5, BATTERY_TYPE_LIPO), -1);
}

TEST(BatteryWarningBands, voltageRemainingPercentInterpolatesLipo)
{
  // LiPo match band is 300..435 cV/cell -> 0..100%.
  EXPECT_EQ(flightBatteryVoltageRemainingPercent(300, BATTERY_TYPE_LIPO), 0);
  EXPECT_EQ(flightBatteryVoltageRemainingPercent(435, BATTERY_TYPE_LIPO), 100);
  // A mid reading lands in a sensible band and clamps.
  EXPECT_EQ(flightBatteryVoltageRemainingPercent(500, BATTERY_TYPE_LIPO), 100);
  const int mid = flightBatteryVoltageRemainingPercent(360, BATTERY_TYPE_LIPO);
  EXPECT_GT(mid, 20);
  EXPECT_LT(mid, 60);
}
