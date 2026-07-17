/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 */

// Regression guard: a model saved by an earlier build (with a fully configured
// battery monitor and its own voltage/capacity sensor bindings) must load
// unchanged, and the new auto-bind convenience must never disturb that already
// established binding. This exercises the real model-data YAML node table, so
// it also proves the crafted pre-existing YAML used in the UI proof is valid.

#include "gtests.h"
#include "storage/yaml/yaml_parser.h"
#include "storage/yaml/yaml_tree_walker.h"
#include "storage/yaml/yaml_datastructs.h"
#include "telemetry/battery_monitor.h"

static void loadModelYamlStr(const char* str)
{
  YamlTreeWalker tree;
  tree.reset(get_modeldata_nodes(), (uint8_t*)&g_model);
  YamlParser yp;
  yp.init(YamlTreeWalker::get_parser_calls(), &tree);
  yp.parse(str, strlen(str));
}

// A model as an older firmware would have serialised it: two custom sensors
// (VFAS volts, Capa mAh) and a monitor already bound to both, LiPo 3S 2200.
static const char* kPreexistingModel =
    "header:\n"
    "   name: \"PREEXIST\"\n"
    "telemetrySensors:\n"
    "   0:\n"
    "      label: \"VFAS\"\n"
    "      type: TYPE_CUSTOM\n"
    "      unit: 1\n"
    "      prec: 2\n"
    "   1:\n"
    "      label: \"Capa\"\n"
    "      type: TYPE_CUSTOM\n"
    "      unit: 14\n"
    "      prec: 0\n"
    "batteryMonitors:\n"
    "   0:\n"
    "      enabled: 1\n"
    "      batteryType: TYPE_LIPO\n"
    "      cellCount: 3\n"
    "      capacity: 2200\n"
    "      sourceIndex: 1\n"
    "      currentIndex: 2\n"
    "      voltAlertEnabled: 1\n"
    "      capAlertEnabled: 1\n"
    "      compatiblePackMask: 1\n";

class BatteryPreexistingYamlTest : public EdgeTxTest {};

TEST_F(BatteryPreexistingYamlTest, LoadsConfiguredMonitorUnchanged)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(kPreexistingModel);

  const BatteryMonitorData& m = g_model.batteryMonitors[0];
  EXPECT_EQ((int)m.enabled, 1);
  EXPECT_EQ((int)m.batteryType, (int)BATTERY_TYPE_LIPO);
  EXPECT_EQ((int)m.cellCount, 3);
  EXPECT_EQ((int)m.capacity, 2200);
  EXPECT_EQ((int)m.sourceIndex, 1);   // bound to VFAS (slot 0)
  EXPECT_EQ((int)m.currentIndex, 2);  // bound to Capa (slot 1)
  EXPECT_EQ((int)m.compatiblePackMask, 1);

  EXPECT_STREQ(g_model.telemetrySensors[0].label, "VFAS");
  EXPECT_EQ((int)g_model.telemetrySensors[0].unit, UNIT_VOLTS);
  EXPECT_STREQ(g_model.telemetrySensors[1].label, "Capa");
  EXPECT_EQ((int)g_model.telemetrySensors[1].unit, UNIT_MAH);
}

TEST_F(BatteryPreexistingYamlTest, AutoBindLeavesExistingBindingUntouched)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(kPreexistingModel);

  // The monitor is enabled and each source already points at its own sensor.
  // Auto-bind must be a no-op: never overwrite what the loaded model chose.
  const int8_t voltBefore = g_model.batteryMonitors[0].sourceIndex;
  const int8_t capBefore = g_model.batteryMonitors[0].currentIndex;

  uint8_t bound = autoBindFlightBatterySensors(0);

  EXPECT_EQ(bound, 0);
  EXPECT_EQ(g_model.batteryMonitors[0].sourceIndex, voltBefore);
  EXPECT_EQ(g_model.batteryMonitors[0].currentIndex, capBefore);
}
