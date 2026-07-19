/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Locks the state-escalation rules newly shared across the colorlcd stock
 * widgets (radio_info.cpp's Link/Signal widget and the Gauge/Value widgets'
 * telemetry-voltage rule), so a future change to one can't silently diverge
 * from the others.
 */

#include "gtests.h"

#if defined(COLORLCD)

#include "widget_palette.h"

// linkStateLevel() is anonymous-namespace-scoped in radio_info.cpp; this
// thin external-linkage wrapper (linkStateLevelForTest) is exported there
// purely for this test.
uint8_t linkStateLevelForTest(uint8_t rssi);

// External linkage in value.cpp; reused as-is by GaugeWidget (gauge.cpp) so
// a gauge fed by a battery-monitor-sourced sensor escalates identically to
// the Value widget. See the doc comment at its definition in value.cpp.
uint8_t telemetryVoltageStateLevel(int sensorIdx);

namespace {

// Restores the touched globals so this fixture never leaks state into other
// tests that read g_model.rfAlarms (mirrors TxBattAlarmGuard in
// tx_battery_alarm.cpp).
struct RfAlarmsGuard {
  int8_t warning, critical;
  RfAlarmsGuard() :
      warning(g_model.rfAlarms.warning), critical(g_model.rfAlarms.critical)
  {
  }
  ~RfAlarmsGuard()
  {
    g_model.rfAlarms.warning = warning;
    g_model.rfAlarms.critical = critical;
  }
};

}  // namespace

// Two-tier RSSI escalation: rfAlarms.critical/.warning are the same fields
// telemetry.cpp's audio alarm (genericRssiCritical/genericRssiWarning) uses,
// with the same strictly-less-than comparison, so the Signal widget's colour
// and the audio alert trip at the exact same RSSI values.
TEST(LinkStateLevel, twoTierEscalationMatchesRfAlarmBands)
{
  RfAlarmsGuard guard;
  g_model.rfAlarms.warning = 45;
  g_model.rfAlarms.critical = 42;

  // No reading yet -> never a false Warning/Critical.
  EXPECT_EQ(linkStateLevelForTest(0), WIDGET_STATE_NORMAL);

  // Above the warning threshold -> Normal.
  EXPECT_EQ(linkStateLevelForTest(50), WIDGET_STATE_NORMAL);
  EXPECT_EQ(linkStateLevelForTest(45), WIDGET_STATE_NORMAL);  // not strictly below

  // [critical, warning) -> Warning.
  EXPECT_EQ(linkStateLevelForTest(44), WIDGET_STATE_WARNING);
  EXPECT_EQ(linkStateLevelForTest(42), WIDGET_STATE_WARNING);  // not strictly below critical

  // Strictly below critical -> Critical, replacing Warning (never both).
  EXPECT_EQ(linkStateLevelForTest(41), WIDGET_STATE_CRITICAL);
  EXPECT_EQ(linkStateLevelForTest(1), WIDGET_STATE_CRITICAL);
}

namespace {

// Minimal battery-monitor + voltage-sensor fixture, deliberately
// self-contained rather than reusing battery_monitor.cpp's static helpers
// (file-local, not visible across translation units).
struct BatteryMonitorSensorGuard {
  BatteryMonitorData monitorBackup;
  TelemetrySensor sensorBackup;
  TelemetryItem itemBackup;

  BatteryMonitorSensorGuard() :
      monitorBackup(g_model.batteryMonitors[0]),
      sensorBackup(g_model.telemetrySensors[0]),
      itemBackup(telemetryItems[0])
  {
  }
  ~BatteryMonitorSensorGuard()
  {
    g_model.batteryMonitors[0] = monitorBackup;
    g_model.telemetrySensors[0] = sensorBackup;
    telemetryItems[0] = itemBackup;
  }
};

void setupThreeCellLipoMonitorOnSensor0(int32_t perCellCv)
{
  g_model.batteryMonitors[0].enabled = true;
  g_model.batteryMonitors[0].batteryType = BATTERY_TYPE_LIPO;
  g_model.batteryMonitors[0].cellCount = 3;
  g_model.batteryMonitors[0].selectedPackSlot = 0;
  g_model.batteryMonitors[0].sourceIndex = 1;  // sensor index 0 (1-based)

  g_model.telemetrySensors[0].init("VFAS", UNIT_VOLTS, 2);
  telemetryItems[0].value = perCellCv * 3;  // pack total, matching sensor 0's cell count
  telemetryItems[0].setFresh();
}

}  // namespace

// A telemetry voltage sensor gets Warning/Critical ONLY when it is the
// voltage source of an ENABLED battery monitor -- both the Value widget and
// GaugeWidget defer to this exact rule (see the doc comment at its
// definition in value.cpp), so a gauge and a value tile fed by the same
// sensor always agree.
TEST(TelemetryVoltageStateLevel, escalatesFromTheOwningBatteryMonitorCurve)
{
  BatteryMonitorSensorGuard guard;

  // Comfortably full LiPo cell (~4.0V/cell, ~74% remaining) -> Normal.
  setupThreeCellLipoMonitorOnSensor0(400);
  EXPECT_EQ(telemetryVoltageStateLevel(0), WIDGET_STATE_NORMAL);

  // Deeply discharged LiPo cell (~3.1V/cell, ~7% remaining) -> Critical
  // (<=20% remaining band, see flightBatteryRemainingWarningLevel).
  setupThreeCellLipoMonitorOnSensor0(310);
  EXPECT_EQ(telemetryVoltageStateLevel(0), WIDGET_STATE_CRITICAL);
}

TEST(TelemetryVoltageStateLevel, sensorWithNoOwningMonitorStaysDefault)
{
  BatteryMonitorSensorGuard guard;

  // A deeply-discharged reading, but no enabled battery monitor references
  // this sensor -- never guess a battery estimate.
  g_model.batteryMonitors[0].enabled = false;
  g_model.telemetrySensors[0].init("VFAS", UNIT_VOLTS, 2);
  telemetryItems[0].value = 330 * 3;
  telemetryItems[0].setFresh();

  EXPECT_EQ(telemetryVoltageStateLevel(0), WIDGET_STATE_NORMAL);
}

#endif  // COLORLCD
