/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

#include "gtests.h"
#include "telemetry/battery_monitor.h"

// Auto-bind is a wiring convenience only: it fills an ENABLED monitor's UNSET
// voltage/capacity source when EXACTLY ONE matching-unit sensor exists. It must
// never overwrite an existing binding, never bind on zero or 2+ candidates, and
// never rebind once a binding is present (late discovery is inert).

class BatteryAutoBindTest : public EdgeTxTest
{
 protected:
  void SetUp() override
  {
    EdgeTxTest::SetUp();
    memclear(g_model.telemetrySensors, sizeof(g_model.telemetrySensors));
    g_model.batteryMonitors[0] = BatteryMonitorData();
    g_model.batteryMonitors[0].enabled = 1;
  }

  void addSensor(uint8_t index, const char* name, uint8_t unit)
  {
    g_model.telemetrySensors[index].init(name, unit, unit == UNIT_VOLTS ? 2 : 0);
  }
};

TEST_F(BatteryAutoBindTest, BindsSingleVoltageSensor)
{
  addSensor(0, "VFAS", UNIT_VOLTS);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x01, 0x01);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 1);
}

TEST_F(BatteryAutoBindTest, BindsSingleVoltageSensorAtNonZeroSlot)
{
  addSensor(3, "VBAT", UNIT_VOLTS);

  autoBindFlightBatterySensors(0);

  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 4);
}

TEST_F(BatteryAutoBindTest, TreatsCellsSensorAsVoltageCandidate)
{
  addSensor(1, "Cels", UNIT_CELLS);

  autoBindFlightBatterySensors(0);

  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 2);
}

TEST_F(BatteryAutoBindTest, DoesNotBindWhenTwoVoltageSensors)
{
  addSensor(0, "VFAS", UNIT_VOLTS);
  addSensor(1, "VBAT", UNIT_VOLTS);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x01, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 0);
}

TEST_F(BatteryAutoBindTest, DoesNotBindWhenNoVoltageSensor)
{
  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 0);
}

TEST_F(BatteryAutoBindTest, NeverOverwritesExistingVoltageBinding)
{
  g_model.batteryMonitors[0].sourceIndex = 5;  // user-chosen slot 5
  addSensor(0, "VFAS", UNIT_VOLTS);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x01, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 5);
}

TEST_F(BatteryAutoBindTest, LateSensorDiscoveryDoesNotRebind)
{
  // First: exactly one sensor -> auto-bind to slot 0.
  addSensor(0, "VFAS", UNIT_VOLTS);
  EXPECT_EQ(autoBindFlightBatterySensors(0) & 0x01, 0x01);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 1);

  // Later a second sensor is discovered; the existing binding is untouched.
  addSensor(1, "VBAT", UNIT_VOLTS);
  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x01, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 1);
}

TEST_F(BatteryAutoBindTest, DisabledMonitorIsNeverBound)
{
  g_model.batteryMonitors[0].enabled = 0;
  addSensor(0, "VFAS", UNIT_VOLTS);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 0);
}

TEST_F(BatteryAutoBindTest, BindsSingleCapacitySensor)
{
  addSensor(0, "Capa", UNIT_MAH);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x02, 0x02);
  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, 1);
}

TEST_F(BatteryAutoBindTest, DoesNotBindWhenTwoCapacitySensors)
{
  addSensor(0, "Cap1", UNIT_MAH);
  addSensor(1, "Cap2", UNIT_MAH);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound & 0x02, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, 0);
}

TEST_F(BatteryAutoBindTest, NeverOverwritesExistingCapacityBinding)
{
  g_model.batteryMonitors[0].currentIndex = 3;
  addSensor(0, "Capa", UNIT_MAH);

  autoBindFlightBatterySensors(0);

  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, 3);
}

TEST_F(BatteryAutoBindTest, BindsVoltageAndCapacityTogether)
{
  addSensor(0, "VFAS", UNIT_VOLTS);
  addSensor(1, "Capa", UNIT_MAH);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound, 0x03);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 1);
  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, 2);
}

TEST_F(BatteryAutoBindTest, CapacityBindsIndependentlyOfAmbiguousVoltage)
{
  // Two volts sensors (ambiguous, stays unset) but a single mAh sensor binds.
  addSensor(0, "VFAS", UNIT_VOLTS);
  addSensor(1, "VBAT", UNIT_VOLTS);
  addSensor(2, "Capa", UNIT_MAH);

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound, 0x02);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, 3);
}
