#include "gtests.h"
#include "battery_math.h"

class BatteryMathTest : public EdgeTxTest {};

TEST_F(BatteryMathTest, FullCharge)
{
  // 8.4 V → 420 centivolts/cell → 100 %
  EXPECT_EQ(100, txBatteryPercent(84));
}

TEST_F(BatteryMathTest, NominalVoltage)
{
  // 7.4 V → 370 centivolts/cell → 51 %
  EXPECT_EQ(51, txBatteryPercent(74));
}

TEST_F(BatteryMathTest, LowVoltage)
{
  // 6.6 V → 330 centivolts/cell → 14 %
  EXPECT_EQ(14, txBatteryPercent(66));
}

TEST_F(BatteryMathTest, ZeroVoltageClamp)
{
  // txBatteryPercent returns 0 immediately when input is zero
  EXPECT_EQ(0, txBatteryPercent(0));
}

TEST_F(BatteryMathTest, AboveTableClamps)
{
  // Above 8.4 V pack — clamp to 100 %
  EXPECT_EQ(100, txBatteryPercent(85));
  EXPECT_EQ(100, txBatteryPercent(90));
}

TEST_F(BatteryMathTest, OcvPackExactPoints)
{
  // Each entry is (pack decivolts, expected %):
  //   perCellCv = decivolts * 10 / 2
  EXPECT_EQ(14, txBatteryPercent(66));   // 3.30 V/cell
  EXPECT_EQ(28, txBatteryPercent(70));   // 3.50 V/cell
  EXPECT_EQ(39, txBatteryPercent(72));   // 3.60 V/cell
  EXPECT_EQ(51, txBatteryPercent(74));   // 3.70 V/cell
  EXPECT_EQ(62, txBatteryPercent(76));   // 3.80 V/cell
  EXPECT_EQ(74, txBatteryPercent(78));   // 3.90 V/cell
  EXPECT_EQ(82, txBatteryPercent(80));   // 4.00 V/cell
  EXPECT_EQ(96, txBatteryPercent(82));   // 4.10 V/cell
  EXPECT_EQ(100, txBatteryPercent(84));  // 4.20 V/cell
}

TEST_F(BatteryMathTest, InterpolationBetweenPackPoints)
{
  // 7.5 V → 375 centivolts/cell → between 370 (51) and 380 (62)
  // offset 5 / range 10 * delta 11 = 5 → 51 + 5 = 56
  EXPECT_EQ(56, txBatteryPercent(75));

  // 7.3 V → 365 centivolts/cell → between 360 (39) and 370 (51)
  // offset 5 / range 10 * delta 12 = 6 → 39 + 6 = 45
  EXPECT_EQ(45, txBatteryPercent(73));
}

TEST_F(BatteryMathTest, PerCellExact)
{
  EXPECT_EQ(5, batteryPctFromPerCellCv(300));
  EXPECT_EQ(8, batteryPctFromPerCellCv(310));
  EXPECT_EQ(11, batteryPctFromPerCellCv(320));
  EXPECT_EQ(14, batteryPctFromPerCellCv(330));
  EXPECT_EQ(19, batteryPctFromPerCellCv(340));
  EXPECT_EQ(28, batteryPctFromPerCellCv(350));
  EXPECT_EQ(39, batteryPctFromPerCellCv(360));
  EXPECT_EQ(51, batteryPctFromPerCellCv(370));
  EXPECT_EQ(62, batteryPctFromPerCellCv(380));
  EXPECT_EQ(74, batteryPctFromPerCellCv(390));
  EXPECT_EQ(82, batteryPctFromPerCellCv(400));
  EXPECT_EQ(96, batteryPctFromPerCellCv(410));
  EXPECT_EQ(100, batteryPctFromPerCellCv(420));
}

TEST_F(BatteryMathTest, PerCellInterpolation)
{
  // 365 cv → between 360 (39) and 370 (51): offset 5 / 10 * 12 = 6 → 45
  EXPECT_EQ(45, batteryPctFromPerCellCv(365));

  // 375 cv → between 370 (51) and 380 (62): offset 5 / 10 * 11 = 5 → 56
  EXPECT_EQ(56, batteryPctFromPerCellCv(375));
}

TEST_F(BatteryMathTest, PerCellClampLow)
{
  // Below 3.00 V/cell (300 centivolts) → clamp to table minimum
  EXPECT_EQ(5, batteryPctFromPerCellCv(299));
  EXPECT_EQ(5, batteryPctFromPerCellCv(0));
}

TEST_F(BatteryMathTest, PerCellClampHigh)
{
  // Above 4.20 V/cell (420 centivolts) → clamp to 100 %
  EXPECT_EQ(100, batteryPctFromPerCellCv(421));
  EXPECT_EQ(100, batteryPctFromPerCellCv(500));
}
