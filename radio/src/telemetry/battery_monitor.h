/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#pragma once

#include <inttypes.h>

#include "datastructs.h"

// Voltage is checked once per second; 5 s filters transient sag while
// warning promptly enough for the pilot to reduce throttle and head for landing.
constexpr uint8_t FLIGHT_BATTERY_VOLTAGE_DEBOUNCE_SECONDS = 5;
constexpr uint8_t FLIGHT_BATTERY_CAPACITY_THRESHOLDS[] = {65, 70, 75, 80};
constexpr uint8_t FLIGHT_BATTERY_CAPACITY_THRESHOLDS_SIZE =
    sizeof(FLIGHT_BATTERY_CAPACITY_THRESHOLDS) / sizeof(FLIGHT_BATTERY_CAPACITY_THRESHOLDS[0]);
// Voltage-only LiPo needs an early landing/pack-health callout (3.60 V/cell).
constexpr uint16_t FLIGHT_BATTERY_LIPO_VOLTAGE_ONLY_LOW_PER_CELL_CV = 360;
// Capacity-present voltage backup: fires when capacity source likely misreports.
// Set slightly below the voltage-only threshold to duplicate the
// "maintain in-flight voltage above ~3.56 V/cell" guidance with debounce margin.
constexpr uint16_t FLIGHT_BATTERY_LIPO_CAPACITY_BACKUP_LOW_PER_CELL_CV = 356;

constexpr uint16_t FLIGHT_BATTERY_NO_BATTERY_MAX_CV = 100;
constexpr uint16_t FLIGHT_BATTERY_LIPO_MATCH_MIN_PER_CELL_CV = 300;
constexpr uint16_t FLIGHT_BATTERY_LIPO_MATCH_MAX_PER_CELL_CV = 435;
constexpr uint16_t FLIGHT_BATTERY_LIION_MATCH_MIN_PER_CELL_CV = 250;
constexpr uint16_t FLIGHT_BATTERY_LIION_MATCH_MAX_PER_CELL_CV = 420;
constexpr uint16_t FLIGHT_BATTERY_LIION_LOW_PER_CELL_CV = 330;
constexpr uint8_t FLIGHT_BATTERY_PRESENT_DEBOUNCE_SECONDS = 2;
constexpr uint8_t FLIGHT_BATTERY_NO_BATTERY_DEBOUNCE_SECONDS = 3;
constexpr uint8_t FLIGHT_BATTERY_TELEMETRY_LOSS_SWAP_SECONDS = 5;
constexpr uint16_t FLIGHT_BATTERY_NEW_PACK_MIN_PER_CELL_CV = 400;

enum class FlightBatterySessionState : uint8_t {
  Unknown,
  WaitingForVoltage,
  NoBatteryObserved,
  NeedsConfirmation,
  Confirmed,
  ConfirmedWaitingForVoltage,
  VoltageMismatch,
  NeedsConfiguration,
};

struct FlightBatteryRuntimeState {
  FlightBatterySessionState state = FlightBatterySessionState::Unknown;
  uint8_t presentSeconds = 0;
  uint8_t absentSeconds = 0;
  uint8_t confirmedPackSlot = 0;
  uint16_t promptPackMask = 0;
  int32_t consumedBaselineMah = 0;
  int32_t consumedLastMah = -1;
  int32_t consumedSessionMah = 0;
  uint8_t startCapacityPercent = 100;
  uint8_t capacityMask = 0;
  uint8_t voltageLowSeconds = 0;
  uint8_t telemetryLostSeconds = 0;
  bool voltageAlerted = false;
  bool promptShown = false;
};

extern FlightBatteryRuntimeState flightBatteryRuntimeState[MAX_BATTERY_MONITORS];

void resetFlightBatteryRuntimeState();
void updateFlightBatterySessions();
bool flightBatteryArmingAllowed();

enum class ArmingBlockReason : uint8_t {
  None,
  BatteryUnknown,
  BatteryVoltageUnavailable,
  BatteryVoltageMismatch,
  BatteryNeedsConfiguration,
};

ArmingBlockReason flightBatteryArmingBlockReason();
ArmingBlockReason consumeArmingBlockReason();
void setArmingBlockReason(ArmingBlockReason reason);
FlightBatterySessionState flightBatterySessionState(uint8_t monitor);
bool flightBatteryNeedsPrompt(uint8_t* monitor);
uint16_t flightBatteryPromptPackMask(uint8_t monitor);
void requestFlightBatteryPrompt(uint8_t monitor);
void requestFlightBatteryBlockedPrompt();
void markFlightBatteryPromptShown(uint8_t monitor);
bool confirmFlightBatteryPack(uint8_t monitor, uint8_t selectedPackSlot);
void invalidateFlightBatteryMonitor(uint8_t monitor);
void invalidateFlightBatteryPackSlot(uint8_t slot);
bool checkFlightBatteryCapacityAlert(uint8_t monitorIndex,
                                     const BatteryMonitorData& config,
                                     int32_t consumed);
bool checkFlightBatteryAlerts();
int32_t updateFlightBatterySessionConsumed(uint8_t monitorIndex,
                                           int32_t consumed);

enum class BatteryLipoMatchResult { None, Exact, Ambiguous };

inline uint16_t batteryTypeMatchMinPerCellCv(BatteryType type)
{
  switch (type) {
    case BATTERY_TYPE_LIION: return FLIGHT_BATTERY_LIION_MATCH_MIN_PER_CELL_CV;
    case BATTERY_TYPE_LIFE:  return 270;
    case BATTERY_TYPE_NIMH:  return 100;
    case BATTERY_TYPE_PB:    return 170;
    case BATTERY_TYPE_LIPO:
    default:                  return FLIGHT_BATTERY_LIPO_MATCH_MIN_PER_CELL_CV;
  }
}

inline uint16_t batteryTypeMatchMaxPerCellCv(BatteryType type)
{
  switch (type) {
    case BATTERY_TYPE_LIION: return FLIGHT_BATTERY_LIION_MATCH_MAX_PER_CELL_CV;
    case BATTERY_TYPE_LIFE:  return 365;
    case BATTERY_TYPE_NIMH:  return 160;
    case BATTERY_TYPE_PB:    return 240;
    case BATTERY_TYPE_LIPO:
    default:                  return FLIGHT_BATTERY_LIPO_MATCH_MAX_PER_CELL_CV;
  }
}

// Remaining-charge percent (0..100) for a per-cell reading, using the same
// linear interpolation between the chemistry's match-min and match-max per-cell
// voltage that the Battery Monitor widget's voltage-fallback path uses. Returns
// -1 when the reading is unusable (no cells / bad curve / non-positive volts) so
// callers can render a neutral state rather than guess.
inline int flightBatteryVoltageRemainingPercent(int32_t perCellCv,
                                                 BatteryType chem)
{
  if (perCellCv <= 0) return -1;
  const uint16_t minCv = batteryTypeMatchMinPerCellCv(chem);
  const uint16_t maxCv = batteryTypeMatchMaxPerCellCv(chem);
  if (maxCv <= minCv) return -1;
  int pct = (int)(perCellCv - (int32_t)minCv) * 100 / (maxCv - minCv);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// Single source of truth for the 35%/20% warning bands shared by the Battery
// Monitor widget and any telemetry-voltage widget that mirrors a monitor:
// 0 = normal, 1 = warning (<= 35% remaining), 2 = critical (<= 20% remaining).
inline uint8_t flightBatteryRemainingWarningLevel(int remainingPercent)
{
  if (remainingPercent < 0) return 0;
  if (remainingPercent <= 20) return 2;
  if (remainingPercent <= 35) return 1;
  return 0;
}

inline bool flightBatteryPackMatchesChemistry(uint16_t packVoltageCv,
                                               uint8_t cellCount,
                                               BatteryType type)
{
  if (cellCount == 0 || packVoltageCv < FLIGHT_BATTERY_NO_BATTERY_MAX_CV) {
    return false;
  }

  return packVoltageCv >=
             uint16_t(batteryTypeMatchMinPerCellCv(type) * cellCount) &&
         packVoltageCv <=
             uint16_t(batteryTypeMatchMaxPerCellCv(type) * cellCount);
}

inline bool flightBatteryPackMatchesLipo(uint16_t packVoltageCv,
                                          uint8_t cellCount,
                                          BatteryType type)
{
  return flightBatteryPackMatchesChemistry(packVoltageCv, cellCount, type);
}

inline bool flightBatteryPackMatchesLipo(uint16_t packVoltageCv, uint8_t cellCount)
{
  return flightBatteryPackMatchesChemistry(packVoltageCv, cellCount,
                                           BATTERY_TYPE_LIPO);
}

constexpr BatteryLipoMatchResult flightBatteryMatchLipoCandidates(
    uint16_t packVoltageCv, const uint8_t* candidates, uint8_t candidateCount)
{
  if (candidateCount == 0 || packVoltageCv < FLIGHT_BATTERY_NO_BATTERY_MAX_CV) {
    return BatteryLipoMatchResult::None;
  }

  bool hasMatch = false;
  uint8_t matchCount = 0;

  for (uint8_t i = 0; i < candidateCount; ++i) {
    if (flightBatteryPackMatchesLipo(packVoltageCv, candidates[i])) {
      hasMatch = true;
      ++matchCount;
      if (matchCount > 1) {
        return BatteryLipoMatchResult::Ambiguous;
      }
    }
  }

  return hasMatch ? BatteryLipoMatchResult::Exact : BatteryLipoMatchResult::None;
}

inline uint16_t flightBatteryVoltageThresholdPerCellCentivolts(BatteryType type)
{
  switch (type) {
    case BATTERY_TYPE_LIION:
      return FLIGHT_BATTERY_LIION_LOW_PER_CELL_CV;
    case BATTERY_TYPE_LIFE:
      return 280;
    case BATTERY_TYPE_NIMH:
      return 105;
    case BATTERY_TYPE_PB:
      return 180;
    case BATTERY_TYPE_LIPO:
    default:
      return FLIGHT_BATTERY_LIPO_VOLTAGE_ONLY_LOW_PER_CELL_CV;
  }
}

inline uint16_t flightBatteryBackupVoltageThresholdPerCellCentivolts(
    BatteryType type)
{
  switch (type) {
    case BATTERY_TYPE_LIPO:
      return FLIGHT_BATTERY_LIPO_CAPACITY_BACKUP_LOW_PER_CELL_CV;
    default:
      return flightBatteryVoltageThresholdPerCellCentivolts(type);
  }
}

inline const char* batteryTypeToString(BatteryType type)
{
  switch (type) {
    case BATTERY_TYPE_LIPO: return "LiPo";
    case BATTERY_TYPE_LIION: return "Li-Ion";
    case BATTERY_TYPE_LIFE: return "LiFe";
    case BATTERY_TYPE_NIMH: return "NiMH";
    case BATTERY_TYPE_PB: return "Pb";
    default: return "?";
  }
}

inline bool batterySpecEquals(const BatteryPackData& a, const BatteryPackData& b)
{
  // Pack identity is chemistry + capacity + cell count.
  return a.batteryType == b.batteryType &&
         a.capacity == b.capacity &&
         a.cellCount == b.cellCount;
}

struct FlightBatteryCapacityEstimatePoint {
  uint16_t centivolts;
  uint8_t percent;
};

inline bool flightBatteryCapacityEstimateCurveIsValid(uint8_t curve)
{
  return curve <= FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC;
}

inline FlightBatteryCapacityEstimateCurve
flightBatteryCapacityEstimateCurveFromConfig(uint8_t curve)
{
  switch (curve) {
    case FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED:
      return FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED;
    case FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC:
      return FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC;
    case FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE:
    default:
      return FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE;
  }
}

inline const FlightBatteryCapacityEstimatePoint* flightBatteryCapacityCurvePoints(
    BatteryType type, FlightBatteryCapacityEstimateCurve curve, uint8_t& count)
{
  static constexpr FlightBatteryCapacityEstimatePoint lipoConservative[] = {
      {420, 100}, {415, 90}, {410, 85}, {405, 75}, {400, 70},
      {395, 65},  {390, 60}, {385, 50}, {380, 40}, {375, 30},
      {370, 20},  {360, 5},  {330, 0}};
  static constexpr FlightBatteryCapacityEstimatePoint lipoBalanced[] = {
      {420, 100}, {415, 95}, {410, 90}, {405, 82}, {400, 75},
      {395, 68},  {390, 62}, {385, 55}, {380, 45}, {375, 35},
      {370, 25},  {360, 10}, {330, 0}};
  static constexpr FlightBatteryCapacityEstimatePoint lipoOptimistic[] = {
      {420, 100}, {415, 96}, {410, 93}, {405, 87}, {400, 80},
      {395, 72},  {390, 65}, {385, 58}, {380, 50}, {375, 40},
      {370, 30},  {360, 15}, {330, 0}};
  static constexpr FlightBatteryCapacityEstimatePoint liionConservative[] = {
      {420, 100}, {415, 90}, {410, 85}, {405, 78}, {400, 70},
      {395, 62},  {390, 55}, {385, 48}, {380, 40}, {375, 32},
      {370, 25},  {360, 12}, {330, 0}};
  static constexpr FlightBatteryCapacityEstimatePoint liionBalanced[] = {
      {420, 100}, {415, 95}, {410, 90}, {405, 84}, {400, 78},
      {395, 70},  {390, 62}, {385, 55}, {380, 48}, {375, 40},
      {370, 32},  {360, 18}, {330, 0}};
  static constexpr FlightBatteryCapacityEstimatePoint liionOptimistic[] = {
      {420, 100}, {415, 97}, {410, 93}, {405, 88}, {400, 82},
      {395, 75},  {390, 68}, {385, 60}, {380, 52}, {375, 45},
      {370, 38},  {360, 25}, {330, 0}};

  switch (type) {
    case BATTERY_TYPE_LIPO:
      switch (curve) {
        case FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED:
          count = sizeof(lipoBalanced) / sizeof(lipoBalanced[0]);
          return lipoBalanced;
        case FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC:
          count = sizeof(lipoOptimistic) / sizeof(lipoOptimistic[0]);
          return lipoOptimistic;
        case FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE:
        default:
          count = sizeof(lipoConservative) / sizeof(lipoConservative[0]);
          return lipoConservative;
      }
    case BATTERY_TYPE_LIION:
      switch (curve) {
        case FLIGHT_BATTERY_CAPACITY_CURVE_BALANCED:
          count = sizeof(liionBalanced) / sizeof(liionBalanced[0]);
          return liionBalanced;
        case FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC:
          count = sizeof(liionOptimistic) / sizeof(liionOptimistic[0]);
          return liionOptimistic;
        case FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE:
        default:
          count = sizeof(liionConservative) / sizeof(liionConservative[0]);
          return liionConservative;
      }
    default:
      count = 0;
      return nullptr;
  }
}

inline uint8_t flightBatteryEstimateStartCapacityPercent(
    BatteryType type, uint16_t perCellCv,
    FlightBatteryCapacityEstimateCurve curve)
{
  uint8_t count = 0;
  const FlightBatteryCapacityEstimatePoint* points =
      flightBatteryCapacityCurvePoints(type, curve, count);
  if (!points || count == 0) return 100;

  if (perCellCv >= points[0].centivolts) return points[0].percent;

  for (uint8_t i = 1; i < count; i++) {
    const auto& high = points[i - 1];
    const auto& low = points[i];
    if (perCellCv >= low.centivolts) {
      const uint16_t voltageRange = high.centivolts - low.centivolts;
      if (voltageRange == 0) return low.percent;
      const uint16_t voltageOffset = perCellCv - low.centivolts;
      const uint8_t percentRange = high.percent - low.percent;
      return low.percent +
             uint8_t((uint16_t(percentRange) * voltageOffset) / voltageRange);
    }
  }

  return points[count - 1].percent;
}

inline int32_t flightBatteryCapacityThresholdMah(int16_t capacity,
                                                 uint8_t startPercent,
                                                 uint8_t thresholdPercent)
{
  if (capacity <= 0) return INT32_MAX;

  const uint8_t remainingPercent = thresholdPercent >= 100
                                       ? 0
                                       : uint8_t(100 - thresholdPercent);
  if (startPercent > 100) startPercent = 100;
  if (startPercent <= remainingPercent) return 0;

  const uint8_t consumedPercent = startPercent - remainingPercent;
  return int32_t((int64_t(capacity) * consumedPercent + 99) / 100);
}

inline bool flightBatteryCapacityThresholdReached(int32_t consumed,
                                                  int16_t capacity,
                                                  uint8_t thresholdPercent,
                                                  uint8_t startPercent = 100)
{
  if (capacity <= 0 || consumed <= 0) return false;

  return consumed >= flightBatteryCapacityThresholdMah(
                         capacity, startPercent, thresholdPercent);
}

inline int32_t flightBatterySessionConsumedFromRaw(
    const FlightBatteryRuntimeState& runtime, int32_t consumed)
{
  int32_t sessionConsumed = runtime.consumedSessionMah;

  if (runtime.consumedLastMah >= 0) {
    if (consumed > runtime.consumedLastMah) {
      const int64_t next = int64_t(sessionConsumed) +
                           (int64_t(consumed) - runtime.consumedLastMah);
      sessionConsumed = next > INT32_MAX ? INT32_MAX : int32_t(next);
    }
  } else {
    int32_t initial = consumed;
    if (runtime.consumedBaselineMah > 0 &&
        consumed > runtime.consumedBaselineMah) {
      initial = consumed - runtime.consumedBaselineMah;
    }
    if (initial > sessionConsumed) sessionConsumed = initial;
  }

  return sessionConsumed < 0 ? 0 : sessionConsumed;
}
