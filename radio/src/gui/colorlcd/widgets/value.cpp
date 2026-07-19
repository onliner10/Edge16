/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "edgetx.h"
#include "telemetry/battery_monitor.h"
#include "widget.h"
#include "widget_palette.h"

#define ETX_STATE_TIMER_ELAPSED LV_STATE_USER_1
#define ETX_STATE_TELEM_STALE LV_STATE_USER_2
#define ETX_STATE_LARGE_FONT LV_STATE_USER_3

// A telemetry voltage sensor gets Warning/Critical ONLY when it is the
// voltage source of an enabled battery monitor, reusing that monitor's
// chemistry curve + warning bands (single source of truth). A sensor that no
// monitor references gets NO state (Default) — never guess a battery
// estimate. Stale data is rendered Muted by the caller, not here.
//
// External linkage (not static/anonymous-namespace): GaugeWidget (gauge.cpp)
// reuses this exact rule for its numeric value text so a gauge fed by the
// same battery-monitor-sourced sensor escalates identically to the Value
// widget, without duplicating the chemistry-curve lookup.
uint8_t telemetryVoltageStateLevel(int sensorIdx)
{
  for (int m = 0; m < MAX_BATTERY_MONITORS; m++) {
    auto& config = g_model.batteryMonitors[m];
    if (!config.enabled || config.sourceIndex - 1 != sensorIdx) continue;

    uint8_t cells = config.cellCount;
    BatteryType chem = (BatteryType)config.batteryType;
    if (config.selectedPackSlot > 0) {
      uint8_t slot = config.selectedPackSlot - 1;
      if (slot < MAX_BATTERY_PACKS && g_eeGeneral.batteryPacks[slot].active) {
        cells = g_eeGeneral.batteryPacks[slot].cellCount;
        chem = (BatteryType)g_eeGeneral.batteryPacks[slot].batteryType;
      }
    }
    if (cells == 0) return WIDGET_STATE_NORMAL;

    auto& sensor = g_model.telemetrySensors[sensorIdx];
    auto& item = telemetryItems[sensorIdx];
    if (!sensor.isAvailable() || !item.isAvailable() || item.isOld())
      return WIDGET_STATE_NORMAL;

    int32_t cv = convertTelemetryValue(item.value, sensor.unit, sensor.prec,
                                       UNIT_VOLTS, 2);
    int pct = flightBatteryVoltageRemainingPercent(cv / cells, chem);
    return flightBatteryRemainingWarningLevel(pct);
  }
  return WIDGET_STATE_NORMAL;
}

class ValueWidget : public NativeWidget
{
 public:
  ValueWidget(const WidgetFactory* factory, Window* parent, const rect_t& rect,
              WidgetLocation location) :
      NativeWidget(factory, parent, rect, location)
  {
  }

  void createContent(lv_obj_t* parent) override
  {
    (void)parent;

    lv_style_init(&labelStyle);
    lv_style_set_width(&labelStyle, lv_pct(100));
    lv_style_set_height(&labelStyle, lv_pct(100));

    lv_style_init(&valueStyle);
    lv_style_set_width(&valueStyle, lv_pct(100));
    lv_style_set_height(&valueStyle, lv_pct(100));

    initRequiredLvObj(
        contentBox,
        [](lv_obj_t* parent) {
          return createFlexBox(parent, LV_FLEX_FLOW_COLUMN);
        },
        [](lv_obj_t*) {});

    initRequiredLvObj(
        labelShadow, [](lv_obj_t* parent) { return etx_label_create(parent); },
        [&](lv_obj_t* obj) {
          lv_obj_add_style(obj, &labelStyle, LV_PART_MAIN);
          lv_obj_set_style_text_color(obj, lv_color_black(), LV_PART_MAIN);
          lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
          lv_label_set_text(obj, "");
        });

    initRequiredLvObj(
        label,
        [&](lv_obj_t* parent) {
          contentBox.with([&](lv_obj_t* obj) { parent = obj; });
          return etx_label_create(parent);
        },
        [&](lv_obj_t* obj) {
          lv_obj_add_style(obj, &labelStyle, LV_PART_MAIN);
          etx_txt_color(obj, COLOR_THEME_WARNING_INDEX,
                        ETX_STATE_TIMER_ELAPSED);
          etx_txt_color(obj, COLOR_THEME_DISABLED_INDEX, ETX_STATE_TELEM_STALE);
          lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
          lv_label_set_text(obj, "");
        });

    initRequiredLvObj(
        valueShadow,
        [](lv_obj_t* parent) { return etx_label_create(parent, FONT_L_INDEX); },
        [&](lv_obj_t* obj) {
          lv_obj_add_style(obj, &valueStyle, LV_PART_MAIN);
          lv_obj_set_style_text_color(obj, lv_color_black(), LV_PART_MAIN);
          etx_font(obj, FONT_XL_INDEX, ETX_STATE_LARGE_FONT);
          lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
          lv_label_set_text(obj, "");
        });

    initRequiredLvObj(
        value,
        [&](lv_obj_t* parent) {
          contentBox.with([&](lv_obj_t* obj) { parent = obj; });
          return etx_label_create(parent, FONT_L_INDEX);
        },
        [&](lv_obj_t* obj) {
          lv_obj_add_style(obj, &valueStyle, LV_PART_MAIN);
          etx_txt_color(obj, COLOR_THEME_WARNING_INDEX,
                        ETX_STATE_TIMER_ELAPSED);
          etx_txt_color(obj, COLOR_THEME_DISABLED_INDEX, ETX_STATE_TELEM_STALE);
          etx_font(obj, FONT_XL_INDEX, ETX_STATE_LARGE_FONT);
          lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
          lv_label_set_text(obj, "");
        });
  }

  // State escalation from thresholds the pilot ALREADY configured elsewhere.
  // No widget-level colour/threshold options exist. Returns Default/Warning/
  // Critical (WidgetStateLevel). Stale telemetry is handled separately as Muted.
  uint8_t computeStateLevel(mixsrc_t field)
  {
    if (field >= MIXSRC_FIRST_TIMER && field <= MIXSRC_LAST_TIMER) {
      // Single source of truth: same rule the countdown audio uses.
      return timerWidgetStateLevel(field - MIXSRC_FIRST_TIMER);
    }

    if (field == MIXSRC_TX_VOLTAGE) {
      // Single source of truth: getTxBatteryAlarm() (edgetx.h) also drives
      // the audio alarm (main.cpp checkBatteryAlarms) and the Radio Battery
      // status widget, so all three can never disagree on the threshold.
      // TXBATT_ALARM_NONE/WARNING/CRITICAL (0/1/2) matches WidgetStateLevel.
      return getTxBatteryAlarm();
    }

    if (field >= MIXSRC_FIRST_TELEM)
      return telemetryVoltageStateLevel((field - MIXSRC_FIRST_TELEM) / 3);

    return WIDGET_STATE_NORMAL;
  }

  uint32_t contentRefreshKey() override
  {
    auto widgetData = getPersistentData();
    mixsrc_t field = widgetData->options[0].value.unsignedValue;

    WidgetRefreshKey key;
    key.add((int32_t)field).add((int32_t)getValue(field));
    key.add((int32_t)computeStateLevel(field));

    if (field >= MIXSRC_FIRST_TELEM) {
      TelemetryItem& telemetryItem =
          telemetryItems[(field - MIXSRC_FIRST_TELEM) / 3];
      key.add(telemetryItem.isAvailable()).add(telemetryItem.isOld());
    } else if (field >= MIXSRC_FIRST_TIMER && field <= MIXSRC_LAST_TIMER) {
      key.add((int32_t)getTimerStateValue(field - MIXSRC_FIRST_TIMER));
    }

    return key.value();
  }

  void refreshContent() override
  {
    bool changed = false;

    auto widgetData = getPersistentData();

    // get source from options[0]
    mixsrc_t field = widgetData->options[0].value.unsignedValue;

    // if the state level changed (threshold trip)
    uint8_t stateLevel = computeStateLevel(field);
    if (stateLevel != lastStateLevel) {
      lastStateLevel = stateLevel;
      changed = true;
    }

    // stale/lost telemetry -> rendered Muted (absence of data != emergency)
    bool telemStale = false;
    if (field >= MIXSRC_FIRST_TELEM) {
      TelemetryItem& telemetryItem =
          telemetryItems[(field - MIXSRC_FIRST_TELEM) / 3];
      telemStale = !telemetryItem.isAvailable() || telemetryItem.isOld();
    }

    // if value changed
    auto newValue = getValue(field);
    if (lastValue != newValue) {
      lastValue = newValue;
      changed = true;
    } else if (lastTelemState != telemStale) {
      lastTelemState = telemStale;
      changed = true;
    }

    if (changed) {
      if (usesCardChrome()) {
        // State-aware palette tokens: value text renders in Default and
        // escalates to Warning/Critical; the card border + tint (setCardStateCue)
        // is the redundant, non-colour structural cue. Stale telemetry renders
        // Muted with no state cue.
        lv_color_t valColor = telemStale ? paletteLvColor(PAL_MUTED)
                                         : paletteStateTextColor(stateLevel);
        value.with([&](lv_obj_t* obj) {
          lv_obj_set_style_text_color(obj, valColor, LV_PART_MAIN);
        });
        setCardStateCue(telemStale ? WIDGET_STATE_NORMAL : stateLevel);
      } else {
        // legacy top-bar path: keep the existing theme-state text styling
        label.with([](lv_obj_t* obj) {
          lv_obj_clear_state(obj,
                             static_cast<lv_state_t>(ETX_STATE_TIMER_ELAPSED | ETX_STATE_TELEM_STALE));
        });
        value.with([](lv_obj_t* obj) {
          lv_obj_clear_state(obj,
                             static_cast<lv_state_t>(ETX_STATE_TIMER_ELAPSED | ETX_STATE_TELEM_STALE));
        });

        if (field >= MIXSRC_FIRST_TIMER && field <= MIXSRC_LAST_TIMER) {
          if (getTimerStateValue(field - MIXSRC_FIRST_TIMER) < 0) {
            label.with([](lv_obj_t* obj) {
              lv_obj_add_state(obj, ETX_STATE_TIMER_ELAPSED);
            });
            value.with([](lv_obj_t* obj) {
              lv_obj_add_state(obj, ETX_STATE_TIMER_ELAPSED);
            });
          }
        } else if (telemStale) {
          label.with([](lv_obj_t* obj) {
            lv_obj_add_state(obj, ETX_STATE_TELEM_STALE);
          });
          value.with([](lv_obj_t* obj) {
            lv_obj_add_state(obj, ETX_STATE_TELEM_STALE);
          });
        }
      }

      std::string valueTxt;

      // Set value text
      if (field == MIXSRC_TX_VOLTAGE) {
        valueTxt =
            getSourceCustomValueString(field, getValue(field), valueFlags);
        valueTxt += STR_V;
      } else if (field == MIXSRC_TX_TIME) {
        int32_t tme = getValue(MIXSRC_TX_TIME);
        TimerOptions timerOptions;
        timerOptions.options = SHOW_TIME;
        valueTxt = getTimerString(tme, timerOptions);
      } else if (field >= MIXSRC_FIRST_TIMER && field <= MIXSRC_LAST_TIMER) {
        TimerOptions timerOptions;
        timerOptions.options = SHOW_TIMER;
        valueTxt = getTimerString(
            abs(getTimerStateValue(field - MIXSRC_FIRST_TIMER)), timerOptions);
      } else if (field >= MIXSRC_FIRST_TELEM) {
        std::string getSensorCustomValue(uint8_t sensor, int32_t value,
                                         LcdFlags flags);
        valueTxt = getSensorCustomValue((field - MIXSRC_FIRST_TELEM) / 3,
                                        getValue(field), valueFlags);
#if defined(LUA_INPUTS)
      } else if (field >= MIXSRC_FIRST_LUA && field <= MIXSRC_LAST_LUA) {
        valueTxt = getSourceCustomValueString(
            field, calcRESXto1000(getValue(field)), valueFlags | PREC1);
#endif
      } else {
        valueTxt =
            getSourceCustomValueString(field, getValue(field), valueFlags);
      }

      value.with(
          [&](lv_obj_t* obj) { lv_label_set_text(obj, valueTxt.c_str()); });
      valueShadow.with(
          [&](lv_obj_t* obj) { lv_label_set_text(obj, valueTxt.c_str()); });

      // Re-fit the card value font to the ACTUAL text so telemetry values
      // render in full at any precision (never ellipsised). layoutContent fits
      // the font before the value is known; different readings vary in width
      // (e.g. "12.60V" vs "9.60V"), so the font must be chosen per value.
      if (cardValueFit) {
        value.with([&](lv_obj_t* obj) {
          coord_t vy = 0, vfh = 0;
          FontIndex f =
              fitCardStackValue(valueTxt.c_str(), cardRectSaved, vy, vfh);
          etx_font(obj, f);
          setObjRect(obj, 0, vy, cardRectSaved.w, vfh);
        });
      }
    }
  }

  static const WidgetOption options[];

 protected:
  int32_t lastValue = -10000;
  bool lastTelemState = false;
  uint8_t lastStateLevel = 255;
  // Card stack geometry captured at layout time so refreshContent can re-fit
  // the value font to the actual value text (see refreshContent).
  bool cardValueFit = false;
  rect_t cardRectSaved = {0, 0, 0, 0};
  lv_style_t labelStyle;
  lv_style_t valueStyle;
  RequiredLvObj label;
  RequiredLvObj labelShadow;
  RequiredLvObj value;
  RequiredLvObj valueShadow;
  RequiredLvObj contentBox;
  LcdFlags valueFlags = 0;

  static LAYOUT_VAL_SCALED(VAL_Y1, 14) static LAYOUT_VAL_SCALED(VAL_Y2, 18) static LAYOUT_VAL_SCALED( // ds-allow: value widget — DPI-scaled label/value Y offsets and compact Y for the canvas value layout; not a DS row/form.
      COMPACT_VAL_Y,
      14) static LAYOUT_VAL_SCALED(H_CHK, 50) static LAYOUT_VAL_SCALED(W_CHK, // ds-allow: value widget — DPI-scaled short-card height/width thresholds for choosing the inline vs stacked value layout; canvas geometry, not a DS row/form.
                                                                       120)

      void layoutContent(const rect_t& content) override
  {
    auto widgetData = getPersistentData();

    // Only the stacked card path re-fits the value font (set below).
    cardValueFit = false;

    // get source from options[0]
    mixsrc_t field = widgetData->options[0].value.unsignedValue;

    // No colour option (index 1 is a retired placeholder). On cards: Muted
    // labels + Default value token (state colours applied in refreshContent).
    // Top bar (dark surface): legacy PRIMARY2 ink.
    bool nativeCard = usesCardChrome();
    label.with([&](lv_obj_t* obj) {
      if (nativeCard)
        lv_obj_set_style_text_color(obj, mutedTextColor(), LV_PART_MAIN);
      else
        etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX);
    });
    value.with([&](lv_obj_t* obj) {
      if (nativeCard)
        lv_obj_set_style_text_color(obj, paletteLvColor(PAL_DEFAULT),
                                    LV_PART_MAIN);
      else
        etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX);
    });

    // get label alignment from options[3]
    LcdFlags lblAlign = widgetData->options[3].value.unsignedValue;

    // get value alignment from options[4]
    LcdFlags valAlign = widgetData->options[4].value.unsignedValue;

    bool compact = isCompactTopBarWidget();
    bool shortCard = !compact && content.h < H_CHK;

    if (usesCardChrome()) {
      labelShadow.with([&](lv_obj_t* obj) { setObjVisible(obj, false); });
      valueShadow.with([&](lv_obj_t* obj) { setObjVisible(obj, false); });
      char* labelTxt = getSourceString(field);
      label.with([&](lv_obj_t* obj) {
        lv_label_set_text(obj, labelTxt);
        lv_obj_clear_state(obj, ETX_STATE_LARGE_FONT);
      });
      value.with([&](lv_obj_t* obj) {
        lv_obj_clear_state(obj, ETX_STATE_LARGE_FONT);
      });

      coord_t minLabelH = getFontHeight(LcdFlags(cardTitleFont(content)) << 8u);
      bool showLabel = content.w >= 36 && content.h >= minLabelH * 2;
      label.with([&](lv_obj_t* obj) { setObjVisible(obj, showLabel); });
      contentBox.with([&](lv_obj_t* obj) {
        layoutFlexBox(
            obj, content, showLabel ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW,
            cardGap(content), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
      });
      if (showLabel) {
        // Stacked title + value: refreshContent re-fits the value font to the
        // actual reading so it never ellipsises.
        cardValueFit = true;
        cardRectSaved = content;
        contentBox.with([&](lv_obj_t* row) {
          label.with([&](lv_obj_t* title) {
            value.with([&](lv_obj_t* val) {
              layoutCardStack(row, title, val, content);
              // layoutCardStack resets the value colour; re-apply the Default
              // token (state colours are applied afterwards in refreshContent).
              lv_obj_set_style_text_color(val, paletteLvColor(PAL_DEFAULT),
                                          LV_PART_MAIN);
            });
          });
        });
      } else
        value.with([&](lv_obj_t* obj) {
          etx_font(obj, cardStackValueFont(content));
          lv_obj_set_style_text_color(obj, paletteLvColor(PAL_DEFAULT),
                                      LV_PART_MAIN);
          lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
          setFlexChild(obj, content.w, content.h, 1);
        });

      lastValue = -10000;
      invalidateNativeRefresh();
      return;
    }

    lv_coord_t labelX = content.x;
    lv_coord_t labelY = content.y;
    lv_coord_t valueX = content.x;
    lv_coord_t valueY = content.y;
    lv_coord_t labelW = content.w;
    lv_coord_t valueW = content.w;
    lv_coord_t labelH = getFontHeight(LcdFlags(FONT_XXS_INDEX) << 8u);
    lv_coord_t valueH = getFontHeight(LcdFlags(FONT_L_INDEX) << 8u);

    label.with([](lv_obj_t* obj) { etx_font(obj, FONT_STD_INDEX); });
    labelShadow.with([](lv_obj_t* obj) { etx_font(obj, FONT_STD_INDEX); });
    value.with([](lv_obj_t* obj) {
      etx_font(obj, FONT_L_INDEX);
      lv_obj_clear_state(obj, ETX_STATE_LARGE_FONT);
    });
    valueShadow.with([](lv_obj_t* obj) {
      etx_font(obj, FONT_L_INDEX);
      lv_obj_clear_state(obj, ETX_STATE_LARGE_FONT);
    });

    // Get positions, alignment and value font size.
    if (compact) {
      lblAlign = ALIGN_LEFT;
      valAlign = ALIGN_LEFT;
      labelX = content.x;
      labelY = content.y;
      valueX = content.x;
      valueY = content.y + COMPACT_VAL_Y;
      label.with([](lv_obj_t* obj) { etx_font(obj, FONT_XXS_INDEX); });
      labelShadow.with([](lv_obj_t* obj) { etx_font(obj, FONT_XXS_INDEX); });
      value.with([](lv_obj_t* obj) { etx_font(obj, FONT_BOLD_INDEX); });
      valueShadow.with([](lv_obj_t* obj) { etx_font(obj, FONT_BOLD_INDEX); });
      labelH = getFontHeight(LcdFlags(FONT_XXS_INDEX) << 8u);
      valueH = getFontHeight(LcdFlags(FONT_BOLD_INDEX) << 8u);
    } else if (shortCard) {
      bool showLabel = content.w >= W_CHK;
      char* labelTxt = getSourceString(field);
      label.with([&](lv_obj_t* obj) { lv_label_set_text(obj, labelTxt); });
      lblAlign = ALIGN_LEFT;
      valAlign = showLabel ? ALIGN_RIGHT : ALIGN_CENTER;
      label.with([&](lv_obj_t* obj) { setObjVisible(obj, showLabel); });
      if (showLabel) {
        label.with([&](lv_obj_t* obj) {
          layoutInlineMetric(obj, nullptr, content, content.w / 3,
                             FONT_STD_INDEX);
        });
        value.with([&](lv_obj_t* obj) {
          layoutInlineMetric(nullptr, obj, content, content.w / 3,
                             FONT_STD_INDEX);
        });
        lastValue = -10000;
        invalidateNativeRefresh();
        return;
      }
      value.with([&](lv_obj_t* obj) {
        layoutText(obj, content, FONT_STD_INDEX, primaryTextColor(),
                   LV_TEXT_ALIGN_CENTER);
      });
      lastValue = -10000;
      invalidateNativeRefresh();
      return;
    } else {
      labelX = content.x;
      labelY = content.y;
      valueX = content.x;
      valueY = content.y + labelH + PAD_SMALL; // ds-allow: value widget — value Y placed below the label with a small gap; pixel geometry inside the resizable zone, not a DS row/form.
      valueH = content.h > labelH + PAD_SMALL ? content.h - labelH - PAD_SMALL // ds-allow: value widget — value height from zone height minus label and a small gap; pixel geometry, not a DS row/form.
                                              : content.h;
      FontIndex valueFont = content.h >= 70 ? FONT_L_INDEX : FONT_BOLD_INDEX;
      value.with([&](lv_obj_t* obj) { etx_font(obj, valueFont); });
      valueShadow.with([&](lv_obj_t* obj) { etx_font(obj, valueFont); });
      valueH = getFontHeight(LcdFlags(valueFont) << 8u);
      valueY = content.y + labelH + PAD_SMALL + // ds-allow: value widget — value Y recomputed to vertically center within the remaining zone height (label + small gap); pixel geometry, not a DS row/form.
               ((content.h - labelH - PAD_SMALL - valueH) > 0 // ds-allow: value widget — remaining-height check for vertical centering within the zone; pixel geometry, not a DS row/form.
                    ? (content.h - labelH - PAD_SMALL - valueH) / 2 // ds-allow: value widget — half the remaining zone height used to center the value; pixel geometry, not a DS row/form.
                    : 0);
      if (!nativeCard && field >= MIXSRC_FIRST_TELEM) {
        int8_t sensor = 1 + (field - MIXSRC_FIRST_TELEM) / 3;
        if (!isGPSSensor(sensor) && !isSensorUnit(sensor, UNIT_DATETIME) &&
            !isSensorUnit(sensor, UNIT_TEXT)) {
          // Set font to XL
          value.with([](lv_obj_t* obj) {
            lv_obj_add_state(obj, ETX_STATE_LARGE_FONT);
          });
          valueShadow.with([](lv_obj_t* obj) {
            lv_obj_add_state(obj, ETX_STATE_LARGE_FONT);
          });
        }
      }
#if defined(INTERNAL_GPS)
      else if (field == MIXSRC_TX_GPS) {
      }
#endif
      else if (!nativeCard) {
        // Set font to XL
        value.with(
            [](lv_obj_t* obj) { lv_obj_add_state(obj, ETX_STATE_LARGE_FONT); });
        valueShadow.with(
            [](lv_obj_t* obj) { lv_obj_add_state(obj, ETX_STATE_LARGE_FONT); });
      }
    }

    // Set text alignment
    lv_style_set_text_align(&labelStyle,
                            (lblAlign == ALIGN_RIGHT)    ? LV_TEXT_ALIGN_RIGHT
                            : (lblAlign == ALIGN_CENTER) ? LV_TEXT_ALIGN_CENTER
                                                         : LV_TEXT_ALIGN_LEFT);
    lv_style_set_text_align(&valueStyle,
                            (valAlign == ALIGN_RIGHT)    ? LV_TEXT_ALIGN_RIGHT
                            : (valAlign == ALIGN_CENTER) ? LV_TEXT_ALIGN_CENTER
                                                         : LV_TEXT_ALIGN_LEFT);

    // Set label text
    char* labelTxt = getSourceString(field);
    label.with([&](lv_obj_t* obj) { lv_label_set_text(obj, labelTxt); });
    labelShadow.with([&](lv_obj_t* obj) { lv_label_set_text(obj, labelTxt); });

    // Set label and value positions.
    labelShadow.with([&](lv_obj_t* obj) {
      lv_obj_set_pos(obj, labelX + 1, labelY + 1); // ds-allow: value widget — drop-shadow label nudged 1px behind the label; canvas widget positioned by pixel in the resizable zone, not a DS row/form.
      lv_obj_set_size(obj, labelW, labelH);
    });
    label.with([&](lv_obj_t* obj) {
      lv_obj_set_pos(obj, labelX, labelY); // ds-allow: value widget — label placed at absolute offset within the resizable zone; canvas widget, not a DS row/form.
      lv_obj_set_size(obj, labelW, labelH);
      if (!shortCard) setObjVisible(obj, true);
    });
    valueShadow.with([&](lv_obj_t* obj) {
      lv_obj_set_pos(obj, valueX + 1, valueY + 1); // ds-allow: value widget — drop-shadow value nudged 1px behind the value; canvas widget positioned by pixel in the resizable zone, not a DS row/form.
      lv_obj_set_size(obj, valueW, valueH);
    });
    value.with([&](lv_obj_t* obj) {
      lv_obj_set_pos(obj, valueX, valueY); // ds-allow: value widget — value placed at absolute offset within the resizable zone; canvas widget, not a DS row/form.
      lv_obj_set_size(obj, valueW, valueH);
    });

    // Show / hide shadow
    labelShadow.with([&](lv_obj_t* obj) {
      if (!usesCardChrome() && widgetData->options[2].value.boolValue)
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    });
    valueShadow.with([&](lv_obj_t* obj) {
      if (!usesCardChrome() && widgetData->options[2].value.boolValue)
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    });

    lastValue = -10000;
    invalidateNativeRefresh();
  }
};

// No Color option: value colours are state-driven theme palette tokens. The
// retired Color slot is kept as a hidden Deprecated placeholder so that
// positionally-stored model YAML keeps shadow/align at their original indices;
// the legacy colour value is ignored and dropped on next save.
const WidgetOption ValueWidget::options[] = {
    {STR_SOURCE, WidgetOption::Source, MIXSRC_FIRST_STICK},
    {"", WidgetOption::Deprecated, 0},
    {STR_SHADOW, WidgetOption::Bool, false},
    {STR_ALIGN_LABEL, WidgetOption::Align, ALIGN_LEFT},
    {STR_ALIGN_VALUE, WidgetOption::Align, ALIGN_LEFT},
    {nullptr, WidgetOption::Bool}};

BaseWidgetFactory<ValueWidget> ValueWidget("Value", ValueWidget::options,
                                           STR_WIDGET_VALUE);
