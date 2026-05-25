/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "gtests.h"
#include "telemetry/battery_monitor.h"

static void setFlightBatteryVoltageSensor(uint8_t index, int32_t centivolts)
{
  g_model.telemetrySensors[index].init("VFAS", UNIT_VOLTS, 2);
  telemetryItems[index].value = centivolts;
  telemetryItems[index].setFresh();
  g_model.batteryMonitors[0].sourceIndex = index + 1;
  telemetryStreaming = TELEMETRY_TIMEOUT10ms;
}

static void setFlightBatteryCapacitySensor(uint8_t index, int32_t mah)
{
  g_model.telemetrySensors[index].init("Capa", UNIT_MAH, 0);
  telemetryItems[index].value = mah;
  telemetryItems[index].setFresh();
  g_model.batteryMonitors[0].currentIndex = index + 1;
  telemetryStreaming = TELEMETRY_TIMEOUT10ms;
}

static void setFlightBatteryTelemetryLost()
{
  telemetryStreaming = 0;
  for (auto& item : telemetryItems) {
    if (item.isAvailable()) item.setOld();
  }
}

static void setupCompatibleBatteryMonitor()
{
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;
}

static void updateFlightBatterySessionsForSeconds(uint8_t seconds)
{
  for (uint8_t i = 0; i < seconds; i++) {
    updateFlightBatterySessions();
  }
}

static bool armModelForBatteryTest()
{
  g_model.armingEnabled = true;
  g_model.armingThrottleChannel = 0;
  s_mixer_first_run_done = true;

  int sfIdx = switchLookupIdx("SF", 2);
  int shIdx = switchLookupIdx("SH", 2);
  if (sfIdx < 0 || shIdx < 0) return false;

  simuSetSwitch(sfIdx, 1);
  testUpdateArmingState();
  simuSetSwitch(shIdx, 1);
  testUpdateArmingState();
  simuSetSwitch(shIdx, -1);
  testUpdateArmingState();
  return isModelArmedState();
}

class BatteryMonitorPolicyTest : public EdgeTxTest {};

TEST_F(BatteryMonitorPolicyTest, CapacityThresholdsUseConsumedCapacity)
{
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(1429, 2200, 65));
  EXPECT_TRUE(flightBatteryCapacityThresholdReached(1430, 2200, 65));
  EXPECT_TRUE(flightBatteryCapacityThresholdReached(1760, 2200, 80));
}

TEST_F(BatteryMonitorPolicyTest, PartialStartCapacityAlertsEarlier)
{
  EXPECT_EQ(1100, flightBatteryCapacityThresholdMah(2200, 85, 65));
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(1099, 2200, 65, 85));
  EXPECT_TRUE(flightBatteryCapacityThresholdReached(1100, 2200, 65, 85));
}

TEST_F(BatteryMonitorPolicyTest, CapacityThresholdsAlertWhenStartBelowRemaining)
{
  EXPECT_EQ(0, flightBatteryCapacityThresholdMah(2200, 30, 65));
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(0, 2200, 65, 30));
  EXPECT_TRUE(flightBatteryCapacityThresholdReached(1, 2200, 65, 30));
}

TEST_F(BatteryMonitorPolicyTest, StartCapacityEstimateCurvesSupportLipoAndLiIon)
{
  EXPECT_EQ(100, flightBatteryEstimateStartCapacityPercent(
                     BATTERY_TYPE_LIPO, 420,
                     FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE));
  EXPECT_EQ(85, flightBatteryEstimateStartCapacityPercent(
                    BATTERY_TYPE_LIPO, 410,
                    FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE));
  EXPECT_EQ(90, flightBatteryEstimateStartCapacityPercent(
                    BATTERY_TYPE_LIPO, 410,
                    FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED));
  EXPECT_EQ(93, flightBatteryEstimateStartCapacityPercent(
                    BATTERY_TYPE_LIPO, 410,
                    FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC));
  EXPECT_EQ(85, flightBatteryEstimateStartCapacityPercent(
                    BATTERY_TYPE_LIION, 410,
                    FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE));
  EXPECT_EQ(90, flightBatteryEstimateStartCapacityPercent(
                    BATTERY_TYPE_LIION, 410,
                    FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED));
}

TEST_F(BatteryMonitorPolicyTest, UnsupportedChemistryStartCapacityFallsBackToFull)
{
  EXPECT_EQ(100, flightBatteryEstimateStartCapacityPercent(
                     BATTERY_TYPE_LIFE, 360,
                     FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE));
}

TEST_F(BatteryMonitorPolicyTest, InvalidCapacityCurveFallsBackConservative)
{
  EXPECT_FALSE(flightBatteryCapacityEstimateCurveIsValid(3));
  EXPECT_TRUE(flightBatteryCapacityEstimateCurveIsValid(
      FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC));
  EXPECT_EQ(FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE,
            flightBatteryCapacityEstimateCurveFromConfig(3));
}

TEST_F(BatteryMonitorPolicyTest, CapacityThresholdRejectsInvalidInputs)
{
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(100, 0, 60));
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(0, 2200, 60));
  EXPECT_FALSE(flightBatteryCapacityThresholdReached(-1, 2200, 60));
}

TEST_F(BatteryMonitorPolicyTest, CapacityThresholdHandlesLargeTelemetryValues)
{
  EXPECT_TRUE(
      flightBatteryCapacityThresholdReached(INT32_MAX, INT16_MAX, 90));
}

TEST_F(BatteryMonitorPolicyTest, VoltageThresholdsAreChemistrySpecific)
{
  EXPECT_EQ(350, flightBatteryVoltageThresholdPerCellCentivolts(BATTERY_TYPE_LIPO));
  EXPECT_EQ(330, flightBatteryVoltageThresholdPerCellCentivolts(BATTERY_TYPE_LIION));
  EXPECT_EQ(280, flightBatteryVoltageThresholdPerCellCentivolts(BATTERY_TYPE_LIFE));
  EXPECT_EQ(105, flightBatteryVoltageThresholdPerCellCentivolts(BATTERY_TYPE_NIMH));
  EXPECT_EQ(180, flightBatteryVoltageThresholdPerCellCentivolts(BATTERY_TYPE_PB));
}

TEST_F(BatteryMonitorPolicyTest, LipoMatchConstants)
{
  EXPECT_EQ(65, FLIGHT_BATTERY_CAPACITY_THRESHOLDS[0]);
  EXPECT_EQ(80, FLIGHT_BATTERY_CAPACITY_THRESHOLDS[3]);
  EXPECT_EQ(330, FLIGHT_BATTERY_LIPO_BACKUP_MIN_PER_CELL_CV);
  EXPECT_EQ(100, FLIGHT_BATTERY_NO_BATTERY_MAX_CV);
  EXPECT_EQ(300, FLIGHT_BATTERY_LIPO_MATCH_MIN_PER_CELL_CV);
  EXPECT_EQ(435, FLIGHT_BATTERY_LIPO_MATCH_MAX_PER_CELL_CV);
  EXPECT_EQ(250, FLIGHT_BATTERY_LIION_MATCH_MIN_PER_CELL_CV);
  EXPECT_EQ(420, FLIGHT_BATTERY_LIION_MATCH_MAX_PER_CELL_CV);
  EXPECT_EQ(330, FLIGHT_BATTERY_LIION_LOW_PER_CELL_CV);
  EXPECT_EQ(2, FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);
  EXPECT_EQ(3, FLIGHT_BATTERY_NO_BATTERY_DEBOUNCE_SECONDS);
  EXPECT_EQ(5, FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS);
  EXPECT_EQ(400, FLIGHT_BATTERY_NEW_PACK_MIN_PER_CELL_CV);
}

TEST_F(BatteryMonitorPolicyTest, Lipo3SAt1140CvMatches3SNot4S)
{
  uint16_t voltage3s = 1140;
  EXPECT_TRUE(flightBatteryPackMatchesLipo(voltage3s, 3));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(voltage3s, 4));

  uint8_t candidates1[] = {3};
  EXPECT_EQ(BatteryLipoMatchResult::Exact,
            flightBatteryMatchLipoCandidates(voltage3s, candidates1, 1));

  uint8_t candidates2[] = {3, 4};
  EXPECT_EQ(BatteryLipoMatchResult::Exact,
            flightBatteryMatchLipoCandidates(voltage3s, candidates2, 2));
}

TEST_F(BatteryMonitorPolicyTest, Lipo4SAt1520CvMatches4SNot3S)
{
  uint16_t voltage4s = 1520;
  EXPECT_TRUE(flightBatteryPackMatchesLipo(voltage4s, 4));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(voltage4s, 3));

  uint8_t candidates[] = {3, 4};
  EXPECT_EQ(BatteryLipoMatchResult::Exact,
            flightBatteryMatchLipoCandidates(voltage4s, candidates, 2));
}

TEST_F(BatteryMonitorPolicyTest, LipoBoundaryMinPerCell)
{
  EXPECT_TRUE(flightBatteryPackMatchesLipo(300, 1));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(299, 1));

  EXPECT_TRUE(flightBatteryPackMatchesLipo(900, 3));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(899, 3));
}

TEST_F(BatteryMonitorPolicyTest, LipoBoundaryMaxPerCell)
{
  EXPECT_TRUE(flightBatteryPackMatchesLipo(435, 1));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(436, 1));

  EXPECT_TRUE(flightBatteryPackMatchesLipo(1305, 3));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(1306, 3));
}

TEST_F(BatteryMonitorPolicyTest, LiIonBoundariesAreChemistrySpecific)
{
  EXPECT_TRUE(flightBatteryPackMatchesChemistry(250, 1, BATTERY_TYPE_LIION));
  EXPECT_FALSE(flightBatteryPackMatchesChemistry(249, 1, BATTERY_TYPE_LIION));

  EXPECT_TRUE(flightBatteryPackMatchesChemistry(420, 1, BATTERY_TYPE_LIION));
  EXPECT_FALSE(flightBatteryPackMatchesChemistry(421, 1, BATTERY_TYPE_LIION));

  EXPECT_TRUE(flightBatteryPackMatchesChemistry(425, 1, BATTERY_TYPE_LIPO));
  EXPECT_FALSE(flightBatteryPackMatchesChemistry(425, 1, BATTERY_TYPE_LIION));
}

TEST_F(BatteryMonitorPolicyTest, NoBatteryBelow100Cv)
{
  EXPECT_FALSE(flightBatteryPackMatchesLipo(0, 3));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(99, 3));
  EXPECT_FALSE(flightBatteryPackMatchesLipo(100, 3));

  uint8_t candidates[] = {3, 4};
  EXPECT_EQ(BatteryLipoMatchResult::None,
            flightBatteryMatchLipoCandidates(99, candidates, 2));
}

TEST_F(BatteryMonitorPolicyTest, InvalidCellCountReturnsFalse)
{
  EXPECT_FALSE(flightBatteryPackMatchesLipo(1200, 0));

  uint8_t candidates[] = {0, 3};
  EXPECT_EQ(BatteryLipoMatchResult::Exact,
            flightBatteryMatchLipoCandidates(1200, candidates, 2));
}

TEST_F(BatteryMonitorPolicyTest, AmbiguousCandidatesReturnsAmbiguous)
{
  uint8_t candidates[] = {2, 3, 4};
  EXPECT_EQ(BatteryLipoMatchResult::Exact,
            flightBatteryMatchLipoCandidates(900, candidates, 3));

  uint8_t candidates2[] = {3, 4};
  EXPECT_EQ(BatteryLipoMatchResult::Ambiguous,
            flightBatteryMatchLipoCandidates(1200, candidates2, 2));
}

TEST_F(BatteryMonitorPolicyTest, EmptyCandidatesReturnsNone)
{
  uint8_t empty[] = {};
  EXPECT_EQ(BatteryLipoMatchResult::None,
            flightBatteryMatchLipoCandidates(1200, empty, 0));
}

class BatteryRuntimeTest : public EdgeTxTest {};

TEST_F(BatteryRuntimeTest, SessionStateEnumValues)
{
  EXPECT_EQ(0, (uint8_t)FlightBatterySessionState::Unknown);
  EXPECT_EQ(1, (uint8_t)FlightBatterySessionState::WaitingForVoltage);
  EXPECT_EQ(2, (uint8_t)FlightBatterySessionState::NoBatteryObserved);
  EXPECT_EQ(3, (uint8_t)FlightBatterySessionState::NeedsConfirmation);
  EXPECT_EQ(4, (uint8_t)FlightBatterySessionState::Confirmed);
  EXPECT_EQ(5, (uint8_t)FlightBatterySessionState::ConfirmedWaitingForVoltage);
  EXPECT_EQ(6, (uint8_t)FlightBatterySessionState::VoltageMismatch);
  EXPECT_EQ(7, (uint8_t)FlightBatterySessionState::NeedsConfiguration);
}

TEST_F(BatteryRuntimeTest, InitialStateIsUnknown)
{
  resetFlightBatteryRuntimeState();
  EXPECT_EQ(FlightBatterySessionState::Unknown, flightBatterySessionState(0));
  EXPECT_EQ(FlightBatterySessionState::Unknown, flightBatterySessionState(1));
}

TEST_F(BatteryRuntimeTest, ArmingAllowedWithNoMonitorsEnabled)
{
  resetFlightBatteryRuntimeState();
  EXPECT_TRUE(flightBatteryArmingAllowed());
}

TEST_F(BatteryRuntimeTest, ArmingDisallowedWhenMonitorNotConfirmed)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  invalidateFlightBatteryMonitor(0);
  EXPECT_FALSE(flightBatteryArmingAllowed());
}

TEST_F(BatteryRuntimeTest, ArmingDisallowedWithoutCompatiblePack)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 2200;
  invalidateFlightBatteryMonitor(0);
  EXPECT_FALSE(flightBatteryArmingAllowed());
  EXPECT_EQ(ArmingBlockReason::BatteryNeedsConfiguration,
            flightBatteryArmingBlockReason());
}

TEST_F(BatteryRuntimeTest, ArmingDisallowedWithStaleCompatiblePackMask)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  g_eeGeneral.batteryPacks[0].active = false;
  invalidateFlightBatteryMonitor(0);
  EXPECT_FALSE(flightBatteryArmingAllowed());
  EXPECT_EQ(ArmingBlockReason::BatteryNeedsConfiguration,
            flightBatteryArmingBlockReason());
}

TEST_F(BatteryRuntimeTest, StaleCompatiblePackMaskNeedsConfiguration)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  g_eeGeneral.batteryPacks[0].active = false;
  setFlightBatteryVoltageSensor(0, 1140);

  updateFlightBatterySessions();
  updateFlightBatterySessions();

  EXPECT_EQ(FlightBatterySessionState::NeedsConfiguration,
            flightBatterySessionState(0));
  EXPECT_FALSE(flightBatteryArmingAllowed());
  EXPECT_EQ(ArmingBlockReason::BatteryNeedsConfiguration,
            flightBatteryArmingBlockReason());
}

TEST_F(BatteryRuntimeTest, ArmingAllowedWithFreshZeroVoltageSensor)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  setFlightBatteryVoltageSensor(0, 0);

  for (uint8_t i = 0; i < FLIGHT_BATTERY_NO_BATTERY_DEBOUNCE_SECONDS; i++) {
    updateFlightBatterySessions();
  }

  EXPECT_EQ(FlightBatterySessionState::NoBatteryObserved,
            flightBatterySessionState(0));
  EXPECT_TRUE(flightBatteryArmingAllowed());
}

TEST_F(BatteryRuntimeTest, ArmingDisallowedOnVoltageMismatch)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  setFlightBatteryVoltageSensor(0, 1600);

  updateFlightBatterySessions();
  updateFlightBatterySessions();

  EXPECT_EQ(FlightBatterySessionState::VoltageMismatch,
            flightBatterySessionState(0));
  EXPECT_FALSE(flightBatteryArmingAllowed());
  EXPECT_EQ(ArmingBlockReason::BatteryVoltageMismatch,
            flightBatteryArmingBlockReason());
}

TEST_F(BatteryRuntimeTest, ArmingDisallowedWhenAllMonitorsDisabled)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = false;
  EXPECT_TRUE(flightBatteryArmingAllowed());
}

TEST_F(BatteryRuntimeTest, NeedsPromptReturnsFalseWhenNoMonitorNeedsIt)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = false;
  uint8_t monitor = 99;
  EXPECT_FALSE(flightBatteryNeedsPrompt(&monitor));
}

TEST_F(BatteryRuntimeTest, PromptPackMaskReturnsZeroWhenNoCompatiblePacks)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].compatiblePackMask = 0;
  EXPECT_EQ(0, flightBatteryPromptPackMask(0));
}

TEST_F(BatteryRuntimeTest, PromptPackMaskReturnsConfiguredMask)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_eeGeneral.batteryPacks[1].active = true;
  g_eeGeneral.batteryPacks[1].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[1].cellCount = 4;
  g_eeGeneral.batteryPacks[1].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x03;
  setFlightBatteryVoltageSensor(0, 1200);
  updateFlightBatterySessions();
  updateFlightBatterySessions();
  EXPECT_EQ(0x03, flightBatteryPromptPackMask(0));
}

TEST_F(BatteryRuntimeTest, BlockedArmingRequestsPromptAgain)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_eeGeneral.batteryPacks[1].active = true;
  g_eeGeneral.batteryPacks[1].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[1].cellCount = 4;
  g_eeGeneral.batteryPacks[1].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x03;
  setFlightBatteryVoltageSensor(0, 1200);
  updateFlightBatterySessions();
  updateFlightBatterySessions();

  uint8_t monitor = 99;
  EXPECT_TRUE(flightBatteryNeedsPrompt(&monitor));
  EXPECT_EQ(0, monitor);
  markFlightBatteryPromptShown(0);
  EXPECT_FALSE(flightBatteryNeedsPrompt(nullptr));

  requestFlightBatteryBlockedPrompt();
  EXPECT_TRUE(flightBatteryNeedsPrompt(&monitor));
  EXPECT_EQ(0, monitor);
}

TEST_F(BatteryRuntimeTest, ConfirmFlightBatteryPackSetsConfirmedState)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1140);
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmFlightBatteryPackFromGlobalSlot)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1140);
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(1, g_model.batteryMonitors[0].selectedPackSlot);
  EXPECT_EQ(BATTERY_TYPE_LIPO, g_model.batteryMonitors[0].batteryType);
}

TEST_F(BatteryRuntimeTest, ArmedStatePreventsDemotionToUnknown)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1140);
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmationCopiesGlobalPackIntoMonitor)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = 1;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;

  g_model.batteryMonitors[0].enabled = true;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1140);
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));

  EXPECT_EQ(BATTERY_TYPE_LIPO, g_model.batteryMonitors[0].batteryType);
  EXPECT_EQ(3, g_model.batteryMonitors[0].cellCount);
  EXPECT_EQ(2200, g_model.batteryMonitors[0].capacity);
  EXPECT_EQ(1, g_model.batteryMonitors[0].selectedPackSlot);
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmationRejectsUnconfiguredManual)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].cellCount = 0;

  confirmFlightBatteryPack(0, 0);

  EXPECT_NE(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmationRejectsZeroCapacityManual)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 0;

  confirmFlightBatteryPack(0, 0);

  EXPECT_NE(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmationRejectsZeroCellCountManual)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 0;
  g_model.batteryMonitors[0].capacity = 2200;

  confirmFlightBatteryPack(0, 0);

  EXPECT_NE(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ConfirmationRejectsModelSettingsWithoutPack)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].capacity = 2200;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  setFlightBatteryVoltageSensor(0, 1140);

  EXPECT_FALSE(confirmFlightBatteryPack(0, 0));

  EXPECT_NE(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, DisarmedTelemetryLossWaitsForHighVoltageReplug)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  setFlightBatteryVoltageSensor(0, 1140);
  setFlightBatteryCapacitySensor(1, 500);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));

  flightBatteryRuntimeState[0].consumedSessionMah = 300;
  flightBatteryRuntimeState[0].consumedLastMah = 800;
  flightBatteryRuntimeState[0].capacityMask = 0x0f;
  flightBatteryRuntimeState[0].voltageAlerted = true;
  setFlightBatteryTelemetryLost();

  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::ConfirmedWaitingForVoltage,
            flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(300, flightBatteryRuntimeState[0].consumedSessionMah);
  EXPECT_EQ(0x0f, flightBatteryRuntimeState[0].capacityMask);
  EXPECT_TRUE(flightBatteryRuntimeState[0].voltageAlerted);

  setFlightBatteryVoltageSensor(0, 1200);
  setFlightBatteryCapacitySensor(1, 20);
  updateFlightBatterySessions();
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(300, updateFlightBatterySessionConsumed(0, 20));
  EXPECT_EQ(0x0f, flightBatteryRuntimeState[0].capacityMask);
  EXPECT_TRUE(flightBatteryRuntimeState[0].voltageAlerted);
}

TEST_F(BatteryRuntimeTest, TelemetryReturnBeforeVoltagePreservesReplugEvidence)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1000);
  setFlightBatteryCapacitySensor(1, 500);

  updateFlightBatterySessionsForSeconds(FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);

  setFlightBatteryTelemetryLost();
  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::ConfirmedWaitingForVoltage,
            flightBatterySessionState(0));
  EXPECT_EQ(FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS,
            flightBatteryRuntimeState[0].telemetryLostSeconds);

  telemetryStreaming = TELEMETRY_TIMEOUT10ms;
  updateFlightBatterySessions();
  EXPECT_EQ(FlightBatterySessionState::ConfirmedWaitingForVoltage,
            flightBatterySessionState(0));
  EXPECT_EQ(FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS,
            flightBatteryRuntimeState[0].telemetryLostSeconds);

  setFlightBatteryVoltageSensor(0, 1260);
  setFlightBatteryCapacitySensor(1, 20);
  updateFlightBatterySessions();

  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(1, g_model.batteryMonitors[0].selectedPackSlot);
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].consumedSessionMah);
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedLastMah);
}

TEST_F(BatteryRuntimeTest, TemporarySensorLossWhileTelemetryStreamsDoesNotResetSession)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  setFlightBatteryVoltageSensor(0, 1140);
  setFlightBatteryCapacitySensor(1, 500);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));

  telemetryItems[0].setOld();
  telemetryStreaming = TELEMETRY_TIMEOUT10ms;
  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS + 1);

  EXPECT_EQ(FlightBatterySessionState::ConfirmedWaitingForVoltage,
            flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].telemetryLostSeconds);

  setFlightBatteryVoltageSensor(0, 1140);
  updateFlightBatterySessions();
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
}

TEST_F(BatteryRuntimeTest, ArmedTelemetryLossDoesNotResetConfirmedSession)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  setFlightBatteryVoltageSensor(0, 1140);
  setFlightBatteryCapacitySensor(1, 500);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  ASSERT_TRUE(armModelForBatteryTest());

  setFlightBatteryTelemetryLost();
  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS + 1);

  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].telemetryLostSeconds);
}

TEST_F(BatteryRuntimeTest, HighVoltageReplugAfterTelemetryLossAutoConfirmsNewPack)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  setFlightBatteryVoltageSensor(0, 1140);
  setFlightBatteryCapacitySensor(1, 500);

  updateFlightBatterySessionsForSeconds(FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);

  setFlightBatteryTelemetryLost();
  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::ConfirmedWaitingForVoltage,
            flightBatterySessionState(0));
  EXPECT_EQ(500, flightBatteryRuntimeState[0].consumedBaselineMah);

  setFlightBatteryVoltageSensor(0, 1240);
  setFlightBatteryCapacitySensor(1, 20);
  updateFlightBatterySessions();

  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(1, g_model.batteryMonitors[0].selectedPackSlot);
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].consumedSessionMah);
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedLastMah);
}

TEST_F(BatteryRuntimeTest, FullLowerCellPackReplugDoesNotKeepOldHigherCellSession)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 3;
  g_eeGeneral.batteryPacks[0].capacity = 2200;
  g_eeGeneral.batteryPacks[1].active = true;
  g_eeGeneral.batteryPacks[1].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[1].cellCount = 4;
  g_eeGeneral.batteryPacks[1].capacity = 2200;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x03;
  setFlightBatteryVoltageSensor(0, 1520);
  setFlightBatteryCapacitySensor(1, 500);

  updateFlightBatterySessionsForSeconds(FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);
  EXPECT_EQ(FlightBatterySessionState::Confirmed, flightBatterySessionState(0));
  EXPECT_EQ(2, g_model.batteryMonitors[0].selectedPackSlot);

  setFlightBatteryTelemetryLost();
  updateFlightBatterySessionsForSeconds(
      FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS);

  setFlightBatteryVoltageSensor(0, 1240);
  setFlightBatteryCapacitySensor(1, 20);
  updateFlightBatterySessions();

  EXPECT_EQ(FlightBatterySessionState::NeedsConfirmation,
            flightBatterySessionState(0));
  EXPECT_EQ(0x03, flightBatteryPromptPackMask(0));
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedBaselineMah);
  EXPECT_EQ(20, flightBatteryRuntimeState[0].consumedLastMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].consumedSessionMah);
  EXPECT_EQ(0, updateFlightBatterySessionConsumed(0, 20));
}

TEST_F(BatteryRuntimeTest, SameCapacityAndCellsDifferentChemistryPromptSeparately)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[0].cellCount = 2;
  g_eeGeneral.batteryPacks[0].capacity = 3000;
  g_eeGeneral.batteryPacks[1].active = true;
  g_eeGeneral.batteryPacks[1].batteryType = BATTERY_TYPE_LIION;
  g_eeGeneral.batteryPacks[1].cellCount = 2;
  g_eeGeneral.batteryPacks[1].capacity = 3000;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x03;
  setFlightBatteryVoltageSensor(0, 800);

  updateFlightBatterySessionsForSeconds(FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);

  EXPECT_EQ(FlightBatterySessionState::NeedsConfirmation,
            flightBatterySessionState(0));
  EXPECT_EQ(0x03, flightBatteryPromptPackMask(0));
}

TEST_F(BatteryRuntimeTest, LiIonCompatiblePackAutoSelectsSingleVoltageMatch)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIION;
  g_eeGeneral.batteryPacks[0].cellCount = 2;
  g_eeGeneral.batteryPacks[0].capacity = 3000;
  g_eeGeneral.batteryPacks[1].active = true;
  g_eeGeneral.batteryPacks[1].batteryType = BATTERY_TYPE_LIPO;
  g_eeGeneral.batteryPacks[1].cellCount = 3;
  g_eeGeneral.batteryPacks[1].capacity = 3000;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].compatiblePackMask = 0x03;
  setFlightBatteryVoltageSensor(0, 800);

  updateFlightBatterySessionsForSeconds(FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS);

  EXPECT_EQ(FlightBatterySessionState::Confirmed,
            flightBatterySessionState(0));
  EXPECT_EQ(1, g_model.batteryMonitors[0].selectedPackSlot);
  EXPECT_EQ(BATTERY_TYPE_LIION, g_model.batteryMonitors[0].batteryType);
  EXPECT_EQ(2, g_model.batteryMonitors[0].cellCount);
  EXPECT_EQ(3000, g_model.batteryMonitors[0].capacity);
  EXPECT_EQ(0, flightBatteryPromptPackMask(0));
  EXPECT_FALSE(flightBatteryNeedsPrompt(nullptr));
}

TEST_F(BatteryRuntimeTest, LiIonVoltageAlertFiresAfterDebounce)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIION;
  g_model.batteryMonitors[0].cellCount = 2;
  g_model.batteryMonitors[0].capacity = 3000;
  g_model.batteryMonitors[0].voltAlertEnabled = 1;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  setFlightBatteryVoltageSensor(
      0, uint16_t(FLIGHT_BATTERY_LIION_LOW_PER_CELL_CV * 2 - 1));

  for (uint8_t i = 0; i < FLIGHT_BATTERY_VOLTAGE_DEBOUNCE_SECONDS - 1; i++) {
    EXPECT_FALSE(checkFlightBatteryAlerts());
    EXPECT_FALSE(flightBatteryRuntimeState[0].voltageAlerted);
  }

  EXPECT_TRUE(checkFlightBatteryAlerts());
  EXPECT_TRUE(flightBatteryRuntimeState[0].voltageAlerted);
  EXPECT_FALSE(checkFlightBatteryAlerts());
}

TEST_F(BatteryRuntimeTest, LiIonCapacityAlertFiresAtFirstThreshold)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIION;
  g_model.batteryMonitors[0].cellCount = 2;
  g_model.batteryMonitors[0].capacity = 3000;
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;

  setFlightBatteryCapacitySensor(1, 1949);
  EXPECT_FALSE(checkFlightBatteryAlerts());
  EXPECT_EQ(0, flightBatteryRuntimeState[0].capacityMask & 1);

  setFlightBatteryCapacitySensor(1, 1950);
  EXPECT_TRUE(checkFlightBatteryAlerts());
  EXPECT_EQ(1, flightBatteryRuntimeState[0].capacityMask & 1);
  EXPECT_FALSE(checkFlightBatteryAlerts());
}

TEST_F(BatteryRuntimeTest, ConfirmedLipoStartVoltageSetsConservativeCapacity)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  g_model.batteryMonitors[0].capacityEstimateCurve =
      FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE;
  setFlightBatteryVoltageSensor(0, 1230);
  setFlightBatteryCapacitySensor(1, 0);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;

  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(85, flightBatteryRuntimeState[0].startCapacityPercent);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1099));
  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1100));
}

TEST_F(BatteryRuntimeTest, ConfirmedFullLipoKeepsLegacyCapacityThreshold)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  setFlightBatteryVoltageSensor(0, 1260);
  setFlightBatteryCapacitySensor(1, 0);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;

  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(100, flightBatteryRuntimeState[0].startCapacityPercent);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1429));
  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1430));
}

TEST_F(BatteryRuntimeTest, ConfirmedLiIonStartVoltageAdjustsCapacityThreshold)
{
  resetFlightBatteryRuntimeState();
  g_eeGeneral.batteryPacks[0].active = true;
  g_eeGeneral.batteryPacks[0].batteryType = BATTERY_TYPE_LIION;
  g_eeGeneral.batteryPacks[0].cellCount = 2;
  g_eeGeneral.batteryPacks[0].capacity = 3000;
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIION;
  g_model.batteryMonitors[0].cellCount = 2;
  g_model.batteryMonitors[0].capacity = 3000;
  g_model.batteryMonitors[0].compatiblePackMask = 0x01;
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  setFlightBatteryVoltageSensor(0, 820);
  setFlightBatteryCapacitySensor(1, 0);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;
  flightBatteryRuntimeState[0].promptPackMask = 0x01;

  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(85, flightBatteryRuntimeState[0].startCapacityPercent);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1499));
  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1500));
}

TEST_F(BatteryRuntimeTest, InFlightVoltageDoesNotReEstimateStartCapacity)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  setFlightBatteryVoltageSensor(0, 1230);
  setFlightBatteryCapacitySensor(1, 0);
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::NeedsConfirmation;

  EXPECT_TRUE(confirmFlightBatteryPack(0, 1));
  EXPECT_EQ(85, flightBatteryRuntimeState[0].startCapacityPercent);

  setFlightBatteryVoltageSensor(0, 1260);
  setFlightBatteryCapacitySensor(1, 100);
  EXPECT_FALSE(checkFlightBatteryAlerts());
  EXPECT_EQ(85, flightBatteryRuntimeState[0].startCapacityPercent);
}

TEST_F(BatteryRuntimeTest, CapacityAlertOnlyFiresAfterConfirmation)
{
  resetFlightBatteryRuntimeState();
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].capAlertEnabled = 1;

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1429));

  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;

  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1430));
}

TEST_F(BatteryRuntimeTest, CapacityAlertUsesSessionDelta)
{
  resetFlightBatteryRuntimeState();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  flightBatteryRuntimeState[0].consumedBaselineMah = 500;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].capAlertEnabled = 1;

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1929));

  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1930));
}

TEST_F(BatteryRuntimeTest, CapacityAlertWithSensorResetUsesCurrentAsBaseline)
{
  resetFlightBatteryRuntimeState();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  flightBatteryRuntimeState[0].consumedBaselineMah = 500;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].capAlertEnabled = 1;

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 300));

  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1930));
}

TEST_F(BatteryRuntimeTest, CapacityAccountingPreservesSessionAcrossSensorReset)
{
  resetFlightBatteryRuntimeState();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  flightBatteryRuntimeState[0].consumedBaselineMah = 500;
  flightBatteryRuntimeState[0].consumedLastMah = 500;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].capAlertEnabled = 1;

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 800));
  EXPECT_EQ(300, flightBatteryRuntimeState[0].consumedSessionMah);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 20));
  EXPECT_EQ(300, flightBatteryRuntimeState[0].consumedSessionMah);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1149));
  EXPECT_EQ(1429, flightBatteryRuntimeState[0].consumedSessionMah);

  EXPECT_TRUE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 1150));
  EXPECT_EQ(1430, flightBatteryRuntimeState[0].consumedSessionMah);
}

TEST_F(BatteryRuntimeTest, CapacityAccountingUpdatesWhenAlertsDisabled)
{
  resetFlightBatteryRuntimeState();
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  flightBatteryRuntimeState[0].consumedBaselineMah = 500;
  flightBatteryRuntimeState[0].consumedLastMah = 500;
  g_model.batteryMonitors[0].capacity = 2200;
  g_model.batteryMonitors[0].capAlertEnabled = 0;

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 800));
  EXPECT_EQ(300, flightBatteryRuntimeState[0].consumedSessionMah);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 20));
  EXPECT_EQ(300, flightBatteryRuntimeState[0].consumedSessionMah);

  EXPECT_FALSE(checkFlightBatteryCapacityAlert(0, g_model.batteryMonitors[0], 50));
  EXPECT_EQ(330, flightBatteryRuntimeState[0].consumedSessionMah);
  EXPECT_EQ(0, flightBatteryRuntimeState[0].capacityMask);
}

TEST_F(BatteryRuntimeTest, BatteryAlertsIgnoreTelemetryWarningDisable)
{
  resetFlightBatteryRuntimeState();
  setupCompatibleBatteryMonitor();
  g_model.batteryMonitors[0].capAlertEnabled = 1;
  flightBatteryRuntimeState[0].state = FlightBatterySessionState::Confirmed;
  setFlightBatteryCapacitySensor(1, 1430);
  g_model.disableTelemetryWarning = 1;

  EXPECT_TRUE(checkFlightBatteryAlerts());
  EXPECT_EQ(1, flightBatteryRuntimeState[0].capacityMask & 1);
}
