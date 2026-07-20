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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "battery_monitor_setup.h"

#include "battery_packs.h"
#include "button.h"
#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "sourcechoice.h"
#include "list_line_button.h"
#include "toggleswitch.h"
#include "telemetry/battery_monitor.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

static const char* capacityEstimateCurveLabels[] = {
  "Conservative", "Balanced", "Optimistic"
};

static bool isBatteryMonitorVoltageSensor(uint8_t index)
{
  if (index >= MAX_TELEMETRY_SENSORS) return false;
  const auto& sensor = g_model.telemetrySensors[index];
  return sensor.isAvailable() &&
         (sensor.unit == UNIT_VOLTS || sensor.unit == UNIT_CELLS);
}

static bool isBatteryMonitorCapacitySensor(uint8_t index)
{
  if (index >= MAX_TELEMETRY_SENSORS) return false;
  const auto& sensor = g_model.telemetrySensors[index];
  return sensor.isAvailable() && sensor.unit == UNIT_MAH;
}

static bool isBatteryMonitorSensorSource(int16_t value, uint8_t unit)
{
  if (value == MIXSRC_NONE) return true;
  if (value < MIXSRC_FIRST_TELEM) return false;

  const auto qr = div(value - MIXSRC_FIRST_TELEM, 3);
  if (qr.rem != 0 || qr.quot < 0 || qr.quot >= MAX_TELEMETRY_SENSORS)
    return false;

  return unit == UNIT_VOLTS ? isBatteryMonitorVoltageSensor(qr.quot)
                            : isBatteryMonitorCapacitySensor(qr.quot);
}

static void getFlightPackStatusString(FlightBatterySessionState state,
                                     uint8_t monitor, char* buf,
                                     size_t bufsize)
{
  BatteryMonitorData& config = g_model.batteryMonitors[monitor];

  if (!config.enabled) {
    strncpy(buf, "Monitor disabled", bufsize);
    return;
  }

  if (config.batteryType > BATTERY_TYPE_PB) {
    strncpy(buf, "Needs setup", bufsize);
    return;
  }

  switch (state) {
    case FlightBatterySessionState::Unknown:
      strncpy(buf, "Not detected", bufsize);
      break;
    case FlightBatterySessionState::WaitingForVoltage:
      strncpy(buf, "Waiting for voltage", bufsize);
      break;
    case FlightBatterySessionState::NoBatteryObserved:
      strncpy(buf, "No battery detected", bufsize);
      break;
    case FlightBatterySessionState::NeedsConfirmation:
      strncpy(buf, "Needs selection", bufsize);
      break;
    case FlightBatterySessionState::Confirmed: {
      uint8_t cellCount = config.cellCount;
      int16_t capacity = config.capacity;
      BatteryType chem = (BatteryType)config.batteryType;
      if (config.selectedPackSlot > 0) {
        uint8_t slot = config.selectedPackSlot - 1;
        if (slot < MAX_BATTERY_PACKS &&
            g_eeGeneral.batteryPacks[slot].active) {
          cellCount = g_eeGeneral.batteryPacks[slot].cellCount;
          capacity = g_eeGeneral.batteryPacks[slot].capacity;
          chem = (BatteryType)g_eeGeneral.batteryPacks[slot].batteryType;
        }
      }
      snprintf(buf, bufsize, "%s %dS %d confirmed",
               batteryTypeToString(chem), cellCount, capacity);
      break;
    }
    case FlightBatterySessionState::ConfirmedWaitingForVoltage:
      strncpy(buf, "Confirmed, voltage lost", bufsize);
      break;
    case FlightBatterySessionState::VoltageMismatch:
      strncpy(buf, "Voltage mismatch", bufsize);
      break;
    case FlightBatterySessionState::NeedsConfiguration:
      strncpy(buf, "Monitor needs setup", bufsize);
      break;
    default:
      strncpy(buf, "Unknown state", bufsize);
      break;
  }
}

class CompatiblePackLine : public ListLineButton
{
 public:
  CompatiblePackLine(Window* parent, uint8_t monitor, uint8_t slot)
      : ListLineButton(parent, slot, LineDependencies::LiveValues), monitor(monitor), slot(slot)
  {
    setHeight(PACK_LINE_H);
    padAll(PAD_ZERO); // ds-allow: battery-monitor pack row (ListLineButton) zeroes padding to draw pack spec text at absolute offsets; not a plain DS list row
    setWidth(LV_PCT(100));  // flex to the ds::List content width (as SensorButton)

    setPressHandler([this]() -> uint8_t {
      togglePack();
      return 0;
    });
    check(isActive());
  }

  bool isActive() const override
  {
    const uint16_t mask = g_model.batteryMonitors[monitor].compatiblePackMask;
    return (mask & specMask()) != 0;
  }

  void onLineLoaded() override { onRefresh(); }

  void describeLine(LineView& view) const override
  {
    view.text(PAD_MEDIUM, PACK_TEXT_Y, // ds-allow: battery-monitor pack row draws spec text at absolute x/y offsets in describeLine; not a plain DS list row
              ListLineButton::GRP_W - PAD_MEDIUM * 2, // ds-allow: battery-monitor pack row text width computed from group width at absolute offsets; not a plain DS list row
              EdgeTxStyles::STD_FONT_HEIGHT, lineText);
  }

  void onRefresh() override
  {
    const BatteryPackData& pack = g_eeGeneral.batteryPacks[slot];
    snprintf(lineText, sizeof(lineText), "%s %dS %dmAh",
             batteryTypeToString((BatteryType)pack.batteryType),
             pack.cellCount, pack.capacity);
    updateAutomationText();
    check(isActive());
    withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
  }

 protected:
  uint8_t monitor;
  uint8_t slot;
  char lineText[64] = {};

  static constexpr coord_t PACK_LINE_H = EdgeTxStyles::UI_ELEMENT_HEIGHT +
                                         PAD_TINY * 2; // ds-allow: battery-monitor pack row height constant for absolute-offset text layout; not a DS row
  static constexpr coord_t PACK_TEXT_Y =
      (PACK_LINE_H - EdgeTxStyles::STD_FONT_HEIGHT) / 2;

  uint16_t specMask() const
  {
    uint16_t mask = 0;
    const auto& ref = g_eeGeneral.batteryPacks[slot];
    for (uint8_t i = 0; i < MAX_BATTERY_PACKS; i++) {
      const auto& p = g_eeGeneral.batteryPacks[i];
      if (!p.active) continue;
      if (batterySpecEquals(p, ref))
        mask |= (uint16_t(1u) << i);
    }
    return mask;
  }

  void togglePack()
  {
    uint16_t mask = g_model.batteryMonitors[monitor].compatiblePackMask;
    uint16_t match = specMask();
    if ((mask & match) == match)
      mask &= ~match;
    else
      mask |= match;
    g_model.batteryMonitors[monitor].compatiblePackMask = mask;
    invalidateFlightBatteryMonitor(monitor);
    SET_DIRTY();
    check(isActive());
    updateAutomationText();
  }

  void updateAutomationText()
  {
#if defined(SIMU)
    char text[96];
    snprintf(text, sizeof(text), "%s | selected=%u", lineText,
             unsigned(isActive()));
    setAutomationText(text);
#endif
  }
};

BatteryMonitorPage::BatteryMonitorPage(uint8_t monitor, Route route)
    : SubPage(ICON_MODEL_TELEMETRY, route, STR_MAIN_MENU_MODEL_SETTINGS,
              (std::string("Battery ") + std::to_string(monitor + 1)).c_str()),
      monitor(monitor)
{
  body->setFlexLayout();
  build();
}

void BatteryMonitorPage::rebuild()
{
  body->clear();
  y = 0;
  build();
}

// Reuses the exact Radio Settings > Battery Library pack editor. Seeds a fresh
// pack slot with sane defaults (matching the library's "+" action), then on
// return auto-selects it as a compatible pack for this monitor.
void BatteryMonitorPage::createBattery()
{
  uint8_t slot = MAX_BATTERY_PACKS;
  for (uint8_t i = 0; i < MAX_BATTERY_PACKS; i++) {
    if (!g_eeGeneral.batteryPacks[i].active) {
      slot = i;
      break;
    }
  }
  if (slot >= MAX_BATTERY_PACKS) return;  // library full

  BatteryPackData* pack = &g_eeGeneral.batteryPacks[slot];
  pack->active = 1;
  pack->batteryType = BATTERY_TYPE_LIPO;
  pack->cellCount = 3;
  pack->capacity = 2200;
  storageDirty(EE_GENERAL);

  auto editWindow = new BatteryPackEditWindow(
      slot, route().appended(RP_BATTERY_PACK_EDIT, static_cast<int16_t>(slot)));
  editWindow->setCloseHandler([this, slot]() {
    // The editor's Delete button clears active; only auto-select a pack the
    // user actually kept.
    if (g_eeGeneral.batteryPacks[slot].active) {
      g_model.batteryMonitors[monitor].compatiblePackMask |=
          uint16_t(1u << slot);
      invalidateFlightBatteryMonitor(monitor);
      storageDirty(EE_MODEL);
    }
    rebuild();
  });
}

void BatteryMonitorPage::build()
{
  BatteryMonitorData* config = &g_model.batteryMonitors[monitor];
  if (!flightBatteryCapacityEstimateCurveIsValid(config->capacityEstimateCurve)) {
    config->capacityEstimateCurve = FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE;
    SET_DIRTY();
  }

  // Wiring convenience: an already-enabled monitor with an unset source and a
  // single matching sensor shows the binding as soon as the page opens.
  if (config->enabled) autoBindFlightBatterySensors(monitor);

  // DESIGN SYSTEM (see DESIGN_SYSTEM.md): the page is composed exactly like the
  // Timer form. A ds::List owns the page margins and inter-row gaps; the
  // label/control settings lines are ds::FormRows (label 40% / control 60%,
  // 40 px min, vertically centered) grouped in a borderless ds::Card. Section
  // dividers are ds::SectionHeader. The compatible-pack list (live
  // ListLineButton rows) and the create action live directly in the list, as
  // they are not label/control form lines.
  auto* list = new ds::List(body);
  auto* form = new ds::Card(list);

  new ds::FormRow(form, "Enabled", [=](Window* slot) {
    new ToggleSwitch(slot, rect_t{}, GET_DEFAULT(config->enabled),
                      [=](int32_t newValue) {
                        config->enabled = newValue;
                        // Enabling wires up the source live; the SourceChoice
                        // below re-reads config on its next refresh.
                        if (newValue) autoBindFlightBatterySensors(monitor);
                        invalidateFlightBatteryMonitor(monitor);
                        SET_DIRTY();
                      });
  });

  new ds::FormRow(form, "Voltage Alert", [=](Window* slot) {
    new ToggleSwitch(slot, rect_t{},
                     GET_DEFAULT(config->voltAlertEnabled),
                      [=](int32_t newValue) {
                        config->voltAlertEnabled = newValue;
                        invalidateFlightBatteryMonitor(monitor);
                        SET_DIRTY();
                      });
  });

  new ds::FormRow(form, "Capacity Alert", [=](Window* slot) {
    new ToggleSwitch(slot, rect_t{},
                     GET_DEFAULT(config->capAlertEnabled),
                      [=](int32_t newValue) {
                        config->capAlertEnabled = newValue;
                        invalidateFlightBatteryMonitor(monitor);
                        SET_DIRTY();
                      });
  });

  new ds::FormRow(form, "Capacity Estimate", [=](Window* slot) {
    new Choice(slot, rect_t{}, capacityEstimateCurveLabels,
               FLIGHT_BATTERY_CAPACITY_CURVE_CONSERVATIVE,
               FLIGHT_BATTERY_CAPACITY_CURVE_OPTIMISTIC,
               GET_DEFAULT(flightBatteryCapacityEstimateCurveFromConfig(
                   config->capacityEstimateCurve)),
               [=](int32_t newValue) {
                 config->capacityEstimateCurve = newValue;
                 invalidateFlightBatteryMonitor(monitor);
                 SET_DIRTY();
               });
  });

  new ds::FormRow(form, "Flight Pack Status", [=](Window* slot) {
    new DynamicText(slot, rect_t{}, [=]() {
      char buf[64];
      auto state = flightBatterySessionState(monitor);
      getFlightPackStatusString(state, monitor, buf, sizeof(buf));
      return std::string(buf);
    });
  });

  bool hasActivePacks = false;
  for (uint8_t i = 0; i < MAX_BATTERY_PACKS; i++) {
    if (g_eeGeneral.batteryPacks[i].active) {
      hasActivePacks = true;
      break;
    }
  }

  bool libraryFull = true;
  for (uint8_t i = 0; i < MAX_BATTERY_PACKS; i++) {
    if (!g_eeGeneral.batteryPacks[i].active) {
      libraryFull = false;
      break;
    }
  }

  if (hasActivePacks) {
    new ds::SectionHeader(list, "Compatible Batteries");

    bool seen[MAX_BATTERY_PACKS] = {};
    for (uint8_t i = 0; i < MAX_BATTERY_PACKS; i++) {
      if (!g_eeGeneral.batteryPacks[i].active) continue;
      if (seen[i]) continue;

      for (uint8_t j = i + 1; j < MAX_BATTERY_PACKS; j++) {
        if (g_eeGeneral.batteryPacks[j].active &&
            batterySpecEquals(g_eeGeneral.batteryPacks[i],
                              g_eeGeneral.batteryPacks[j]))
          seen[j] = true;
      }

      uint8_t slotIdx = i;
      new CompatiblePackLine(list, monitor, slotIdx);
    }
  } else {
    // No packs exist yet: instead of a dead-end message, offer inline creation
    // that reuses the radio-level pack library editor.
    new ds::SectionHeader(list, "No batteries configured yet");
  }

  if (!libraryFull) {
    new ds::DSButton(list, "Create battery", ds::ButtonRole::Secondary,
                     [this]() -> uint8_t {
                       createBattery();
                       return 0;
                     });
  }

  new ds::SectionHeader(list, "Advanced Telemetry");
  auto* advForm = new ds::Card(list);

  new ds::FormRow(advForm, STR_VOLTAGE, [=](Window* slot) {
    bool hasVoltageSensor = false;
    for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
      if (isBatteryMonitorVoltageSensor(i)) {
        hasVoltageSensor = true;
        break;
      }
    }
    if (!hasVoltageSensor) {
      new StaticText(slot, rect_t{},
                      "No voltage sensor - add one in Telemetry",
                      COLOR_THEME_WARNING_INDEX);
    } else {
      auto sc = new SourceChoice(
          slot, rect_t{}, MIXSRC_NONE, MIXSRC_LAST_TELEM,
          [=]() -> int {
            if (config->sourceIndex <= 0) return MIXSRC_NONE;
            return MIXSRC_FIRST_TELEM + 3 * (config->sourceIndex - 1);
          },
          [=](int newValue) {
            config->sourceIndex =
                newValue == MIXSRC_NONE
                    ? 0
                    : (newValue - MIXSRC_FIRST_TELEM) / 3 + 1;
            invalidateFlightBatteryMonitor(monitor);
            SET_DIRTY();
          });
      sc->setAvailableHandler([=](int16_t value) {
        return isBatteryMonitorSensorSource(value, UNIT_VOLTS);
      });
    }
  });

  new ds::FormRow(advForm, STR_CURRENTSENSOR, [=](Window* slot) {
    bool hasCurrentSensor = false;
    for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
      if (isBatteryMonitorCapacitySensor(i)) {
        hasCurrentSensor = true;
        break;
      }
    }
    if (!hasCurrentSensor) {
      new StaticText(slot, rect_t{},
                      "No capacity sensor - add one in Telemetry",
                     COLOR_THEME_WARNING_INDEX);
    } else {
      auto sc = new SourceChoice(
          slot, rect_t{}, MIXSRC_NONE, MIXSRC_LAST_TELEM,
          [=]() -> int {
            if (config->currentIndex <= 0) return MIXSRC_NONE;
            return MIXSRC_FIRST_TELEM + 3 * (config->currentIndex - 1);
          },
          [=](int newValue) {
            config->currentIndex =
                newValue == MIXSRC_NONE
                    ? 0
                    : (newValue - MIXSRC_FIRST_TELEM) / 3 + 1;
            invalidateFlightBatteryMonitor(monitor);
            SET_DIRTY();
          });
      sc->setAvailableHandler([=](int16_t value) {
        return isBatteryMonitorSensorSource(value, UNIT_MAH);
      });
    }
  });
}
