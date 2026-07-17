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

#include "special_functions.h"

#include "filechoice.h"
#include "getset_helpers.h"
#include "hal/adc_driver.h"
#include "menu.h"
#include "page.h"
#include "route.h"
#include "sourcechoice.h"
#include "switchchoice.h"
#include "timeedit.h"
#include "toggleswitch.h"
#include "ui_events.h"
#include "view_main.h"

#define SET_DIRTY() setDirty()

static void publishSpecialFunctionsChanged()
{
  UiEventHub::publish(UiTopic::SpecialFunctionsChanged);
}

// A freshly zeroed CustomFunctionData defaults 'func' to raw value 0, which
// is FUNC_OVERRIDE_CHANNEL (channel 1, value 0) -- an armed, *active*
// channel override, not an inert placeholder. That's harmless today only
// because a brand new slot also defaults 'active' to 0. Special/Global
// Functions are now created enabled by default (so a fully-configured new
// function actually fires instead of silently doing nothing), which means a
// slot must be genuinely inert before any function is chosen, not just
// "disabled". FUNC_PLAY_TRACK with an empty track name is a verified no-op
// (see playCustomFunctionFile(): it does nothing when play.name[0] == '\0'),
// is available for both model (Special) and radio-wide (Global) functions,
// and needs no enum reordering that would break stored-model migration. Call
// this on any newly created (never-before-configured) slot before opening
// its editor.
static void presetNewFunctionData(CustomFunctionData* cfn)
{
  cfn->func = FUNC_PLAY_TRACK;
  cfn->active = 1;
}

// CFN_RESET() unconditionally clears 'active' whenever a function's type
// changes (its accumulated state may not mean the same thing under a new
// function, so re-arming automatically would be unsafe for an
// already-configured function). But applied verbatim to a brand new slot,
// it silently undoes newSF()'s "created enabled" preset the instant the
// pilot actually picks a function -- e.g. switch + Play Sound, exit, would
// still land disabled. Call this right after CFN_RESET(); it re-affirms
// 'active' only for a slot that was completely unconfigured
// (CustomFunctionData::isEmpty(), i.e. no switch yet) when its editor was
// opened, so first-time setup in one sitting ends up enabled while
// repurposing an existing configured function still requires an explicit
// re-enable.
static void applyFunctionChangeActiveState(CustomFunctionData* cfn,
                                           bool wasEmptyOnOpen)
{
  if (wasEmptyOnOpen) cfn->active = 1;
}

//-----------------------------------------------------------------------------

static const char* _failsafe_module[] = {
    "Int.",
    "Ext.",
};

FunctionLineButton::FunctionLineButton(Window* parent, const rect_t& rect,
                                       CustomFunctionData* cfn,
                                       uint8_t index, const char* prefix) :
    ListLineButton(parent, index, LineDependencies::LiveValues), cfn(cfn), prefix(prefix)
{
  setHeight(FunctionsPage::SF_BUTTON_H);
  padAll(PAD_ZERO);
}

void FunctionLineButton::onLineLoaded()
{
  if (!withLive([&](LiveWindow& live) {
        sfEnable = new CheckButton(
            this, {EN_X, EN_Y, EN_SZ, EN_SZ},
            [=]() -> uint8_t { return functionEnabled(); },
            [=](uint8_t enabled) { setFunctionEnabled(enabled); });
        if (!sfEnable || !sfEnable->isAvailable()) return false;
        sfEnable->setWindowFlag(NO_FOCUS);
        onRefresh();
        lv_obj_invalidate(live.lvobj());
        return true;
      }))
    return;

}

void FunctionLineButton::describeLine(LineView& view) const
{
  view.text(NM_X, NM_Y, NM_W, EdgeTxStyles::STD_FONT_HEIGHT, sfNameText);
  view.text(SW_X, SW_Y, SW_W, EdgeTxStyles::STD_FONT_HEIGHT, sfSwitchText);
  view.text(FN_X, FN_Y, FN_W, EdgeTxStyles::STD_FONT_HEIGHT, sfFuncText);
  view.text(RP_X, RP_Y, RP_W, EdgeTxStyles::STD_FONT_HEIGHT, sfRepeatText, 0,
            LV_TEXT_ALIGN_RIGHT);
}

void FunctionLineButton::updateAutomationText()
{
#if defined(SIMU)
  char text[224];
  snprintf(text, sizeof(text), "%s | switch=%s | function=%s | repeat=%s | enabled=%u",
           sfNameText, sfSwitchText, sfFuncText, sfRepeatText,
           unsigned(functionEnabled()));
  setAutomationText(text);
#endif
}

bool FunctionLineButton::functionEnabled() const { return CFN_ACTIVE(cfn); }

void FunctionLineButton::setFunctionEnabled(bool enabled)
{
  if (functionEnabled() == enabled) {
    if (sfEnable) sfEnable->update();
    return;
  }

  CFN_ACTIVE(cfn) = enabled ? 1 : 0;
  setDirty();
  if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT || CFN_FUNC(cfn) == FUNC_RGB_LED)
    LUA_LOAD_MODEL_SCRIPTS();
  requestLineUpdate();
}

void FunctionLineButton::onRefresh()
{
  uint8_t func = CFN_FUNC(cfn);

  char s[64];

  snprintf(sfNameText, sizeof(sfNameText), "%s%d", prefix, index + 1);
  sfSwitchText[0] = '\0';
  strAppend(sfSwitchText, getSwitchPositionName(CFN_SWITCH(cfn)),
            sizeof(sfSwitchText) - 1);

  strcpy(s, funcGetLabel(func));
  strcat(s, " - ");

  switch (func) {
    case FUNC_OVERRIDE_CHANNEL:
      sprintf(s + strlen(s), "%s = %s",
              getSourceString(MIXSRC_FIRST_CH + CFN_CH_INDEX(cfn)),
              formatNumberAsString(CFN_PARAM(cfn)).c_str());
      break;

    case FUNC_TRAINER: {
      int16_t value = CFN_CH_INDEX(cfn);
      if (value == 0)
        strcat(s, STR_STICKS);
      else if (value == MAX_STICKS + 1)
        strcat(s, STR_CHANS);
      else
        strcat(s, getMainControlLabel(value - 1));
      break;
    }

    case FUNC_RESET:
      if (CFN_PARAM(cfn) < FUNC_RESET_PARAM_FIRST_TELEM) {
        strcat(s, STR_VFSWRESET[CFN_PARAM(cfn)]);
      } else {
        TelemetrySensor* sensor =
            &g_model.telemetrySensors[CFN_PARAM(cfn) -
                                      FUNC_RESET_PARAM_FIRST_TELEM];
        strAppend(s + strlen(s), sensor->label, TELEM_LABEL_LEN);
      }
      break;

    case FUNC_VOLUME:
    case FUNC_BACKLIGHT:
    case FUNC_PLAY_VALUE:
      strcat(s, getSourceString(CFN_PARAM(cfn)));
      break;

    case FUNC_PLAY_SOUND:
      strcat(s, STR_FUNCSOUNDS[CFN_PARAM(cfn)]);
      break;

    case FUNC_PLAY_TRACK:
    case FUNC_BACKGND_MUSIC:
    case FUNC_PLAY_SCRIPT:
    case FUNC_RGB_LED:
      if (ZEXIST(cfn->play.name)) {
        strAppend(s + strlen(s), cfn->play.name, LEN_FUNCTION_NAME);
      } else {
        strcat(s, STR_VCSWFUNC[0]);
      }
      break;

    case FUNC_SET_TIMER:
      if (isTimerSourceAvailable(CFN_TIMER_INDEX(cfn))) {
        sprintf(s + strlen(s), "%s%d = %s", STR_SRC_TIMER,
                CFN_TIMER_INDEX(cfn) + 1,
                getTimerString(CFN_PARAM(cfn), {.options = SHOW_TIME}));
      } else {
        sprintf(s + strlen(s), "---");
      }
      break;

    case FUNC_SET_FAILSAFE:
      strcat(s, _failsafe_module[CFN_PARAM(cfn)]);
      break;

    case FUNC_HAPTIC:
    case FUNC_SET_SCREEN:
      strcat(s, formatNumberAsString(CFN_PARAM(cfn)).c_str());
      break;

#if defined(FUNCTION_SWITCHES)
    case FUNC_PUSH_CUST_SWITCH:
      strAppend(s + strlen(s),
                switchGetDefaultName(
                    switchGetSwitchFromCustomIdx(CFN_CS_INDEX(cfn))));
      break;
#endif

    case FUNC_LOGS:
      strcat(
          s,
          formatNumberAsString(CFN_PARAM(cfn), PREC1, 0, nullptr, "s").c_str());
      break;

    case FUNC_ADJUST_GVAR:
      strcat(s, getSourceString(CFN_GVAR_INDEX(cfn) + MIXSRC_FIRST_GVAR));
      switch (CFN_GVAR_MODE(cfn)) {
        case FUNC_ADJUST_GVAR_CONSTANT:
          sprintf(s + strlen(s), " = %s",
                  formatNumberAsString(CFN_PARAM(cfn)).c_str());
          break;
        case FUNC_ADJUST_GVAR_SOURCE:
        case FUNC_ADJUST_GVAR_SOURCERAW:
          sprintf(s + strlen(s), " = %s", getSourceString(CFN_PARAM(cfn)));
          break;
        case FUNC_ADJUST_GVAR_GVAR:
          sprintf(s + strlen(s), " = %s",
                  getSourceString(CFN_PARAM(cfn) + MIXSRC_FIRST_GVAR));
          break;
        case FUNC_ADJUST_GVAR_INCDEC: {
          int16_t value = CFN_PARAM(cfn);
          sprintf(s + strlen(s), " %s= %d", (value >= 0) ? "+" : "-",
                  abs(value));
          break;
        }
      }
      break;

    default:
      strcpy(s, funcGetLabel(func));
      break;
  }

  sfFuncText[0] = '\0';
  strAppend(sfFuncText, s, sizeof(sfFuncText) - 1);

  s[0] = 0;

  if (sfEnable) sfEnable->update();

  if (HAS_REPEAT_PARAM(func)) {
    if (func == FUNC_PLAY_SCRIPT || func == FUNC_RGB_LED) {
      sprintf(s, "(%s)", (CFN_PLAY_REPEAT(cfn) == 0) ? "On" : "1x");
    } else {
      sprintf(
          s, "(%s)",
          (CFN_PLAY_REPEAT(cfn) == 0) ? "1x"
          : (CFN_PLAY_REPEAT(cfn) == CFN_PLAY_REPEAT_NOSTART)
              ? "!1x"
              : formatNumberAsString(CFN_PLAY_REPEAT(cfn) * CFN_PLAY_REPEAT_MUL,
                                     0, 0, nullptr, "s")
                    .c_str());
    }
  }

  sfRepeatText[0] = '\0';
  strAppend(sfRepeatText, s, sizeof(sfRepeatText) - 1);
  updateAutomationText();
  withLive([&](LiveWindow& live) { lv_obj_invalidate(live.lvobj()); });
}

//-----------------------------------------------------------------------------

static const lv_coord_t col_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(3),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

#define ETX_STATE_SF_ACTIVE LV_STATE_USER_1

#define SD_LOGS_PERIOD_MIN 1       // 0.1s  fastest period
#define SD_LOGS_PERIOD_MAX 255     // 25.5s slowest period
#define SD_LOGS_PERIOD_DEFAULT 10  // 1s    default period for newly created SF

#define PUSH_CS_DURATION_MIN 0  // 0     no duration : as long as switch is true
#define PUSH_CS_DURATION_MAX 255  // 25.5s longest duration

FunctionEditPage::FunctionEditPage(uint8_t index, EdgeTxIcon icon,
                                   Route route, const char* title, const char* prefix) :
    Page(icon, route), index(index)
{
  buildHeader(header, title, prefix);

  delayLoad();
}

void FunctionEditPage::delayedInit() { buildBody(body); }

void FunctionEditPage::onLiveCheckEvents(Window::LiveWindow& live)
{
  Page::onLiveCheckEvents(live);
  if (active != isActive()) {
    if (isActive()) {
      headerSF->addState(ETX_STATE_SF_ACTIVE);
    } else {
      headerSF->clearState(ETX_STATE_SF_ACTIVE);
    }
    active = isActive();
  }
}

void FunctionEditPage::buildHeader(Window* window, const char* title,
                                   const char* prefix)
{
  header->setTitle(title);
  headerSF = header->setTitle2(prefix + std::to_string(index + 1));

  headerSF->textColor(COLOR_THEME_ACTIVE_INDEX, ETX_STATE_SF_ACTIVE);
  headerSF->font(FONT_BOLD_INDEX, ETX_STATE_SF_ACTIVE);
}

void FunctionEditPage::addSourceChoice(FormLine* line, const char* title,
                                       CustomFunctionData* cfn, int16_t vmax)
{
  new StaticText(line, rect_t{}, title);
  new SourceChoice(line, rect_t{}, 0, vmax, GET_SET_DEFAULT(CFN_PARAM(cfn)),
                   true);
}

NumberEdit* FunctionEditPage::addNumberEdit(FormLine* line, const char* title,
                                            CustomFunctionData* cfn,
                                            int16_t vmin, int16_t vmax)
{
  new StaticText(line, rect_t{}, title);
  return new NumberEdit(line, rect_t{}, vmin, vmax,
                        GET_SET_DEFAULT(CFN_PARAM(cfn)));
}

void FunctionEditPage::updateSpecialFunctionOneWindow()
{
  specialFunctionOneWindow->clear();
  specialFunctionOneWindow->setFlexLayout();
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  auto line = specialFunctionOneWindow->newLine(grid);

  CustomFunctionData* cfn = customFunctionData();
  uint8_t func = CFN_FUNC(cfn);

  // Func param
  switch (func) {
    case FUNC_OVERRIDE_CHANNEL: {
      new StaticText(line, rect_t{}, STR_CH);
      auto* chNum = new NumberEdit(line, rect_t{}, 1, MAX_OUTPUT_CHANNELS,
                     GET_SET_VALUE_WITH_OFFSET(CFN_CH_INDEX(cfn), 1));
      chNum->setDirectKeyboard(false);
      chNum->setEditTitle(STR_CHANNELRANGE);
      line = specialFunctionOneWindow->newLine(grid);

      int limit =
          (g_model.extendedLimits ? LIMIT_EXT_PERCENT : LIMIT_STD_PERCENT);
      addNumberEdit(line, STR_VALUE, cfn, -limit, limit);
      break;
    }

    case FUNC_TRAINER: {
      new StaticText(line, rect_t{}, STR_VALUE);
      auto max_sticks = adcGetMaxInputs(ADC_INPUT_MAIN);
      auto choice = new Choice(line, rect_t{}, 0, max_sticks + 1,
                               GET_SET_DEFAULT(CFN_CH_INDEX(cfn)));
      choice->setTextHandler([=](int32_t value) {
        if (value == 0)
          return std::string(STR_STICKS);
        else if (value == MAX_STICKS + 1)
          return std::string(STR_CHANS);

        return std::string(getMainControlLabel(value - 1));
      });
      break;
    }

    case FUNC_RESET:
      if (CFN_PARAM(cfn) <= FUNC_RESET_PARAM_LAST) {
        new StaticText(line, rect_t{}, STR_RESET);
        auto choice =
            new Choice(line, rect_t{}, 0,
                       FUNC_RESET_PARAM_FIRST_TELEM + lastUsedTelemetryIndex(),
                       GET_SET_DEFAULT(CFN_PARAM(cfn)));
        choice->setAvailableHandler(isSourceAvailableInResetSpecialFunction);
        choice->setTextHandler([=](int32_t value) {
          if (value < FUNC_RESET_PARAM_FIRST_TELEM)
            return std::string(STR_VFSWRESET[value]);
          else
            return std::string(
                g_model.telemetrySensors[value - FUNC_RESET_PARAM_FIRST_TELEM]
                    .label,
                TELEM_LABEL_LEN);
        });
      }
      break;

    case FUNC_VOLUME:
      addSourceChoice(line, STR_VOLUME, cfn, MIXSRC_LAST_CH);
      break;

    case FUNC_BACKLIGHT:
      addSourceChoice(line, STR_VALUE, cfn, MIXSRC_LAST_CH);
      break;

    case FUNC_PLAY_SOUND:
      new StaticText(line, rect_t{}, STR_VALUE);
      new Choice(line, rect_t{}, STR_FUNCSOUNDS, 0,
                 AU_SPECIAL_SOUND_LAST - AU_SPECIAL_SOUND_FIRST - 1,
                 GET_SET_DEFAULT(CFN_PARAM(cfn)));
      break;

    case FUNC_PLAY_TRACK:
    case FUNC_BACKGND_MUSIC:
    case FUNC_PLAY_SCRIPT:
    case FUNC_RGB_LED:
      new StaticText(line, rect_t{}, STR_VALUE);
      new FileChoice(
          line, rect_t{},
          func == FUNC_PLAY_SCRIPT || func == FUNC_RGB_LED
              ? (func == FUNC_PLAY_SCRIPT ? SCRIPTS_FUNCS_PATH
                                          : SCRIPTS_RGB_PATH)
              : std::string(SOUNDS_PATH, SOUNDS_PATH_LNG_OFS) +
                    std::string(currentLanguagePack->id, 2),
          (func == FUNC_PLAY_SCRIPT || func == FUNC_RGB_LED) ? SCRIPTS_EXT
                                                             : SOUNDS_EXT,
          sizeof(cfn->play.name),
          [=]() { return std::string(cfn->play.name, ZLEN(cfn->play.name)); },
          [=](std::string newValue) {
            strncpy(cfn->play.name, newValue.c_str(), sizeof(cfn->play.name));
            SET_DIRTY();
            if (func == FUNC_PLAY_SCRIPT || func == FUNC_RGB_LED)
              LUA_LOAD_MODEL_SCRIPTS();
          },
          true, funcGetLabel(func));
      break;

    case FUNC_SET_TIMER: {
      new StaticText(line, rect_t{}, STR_TIMER);
      if (timersSetupCount() > 0) {
        auto timerchoice = new Choice(line, rect_t{}, 0, TIMERS - 1,
                                      GET_SET_DEFAULT(CFN_TIMER_INDEX(cfn)));
        timerchoice->setTextHandler([](int32_t value) {
          return std::string(STR_TIMER) + std::to_string(value + 1);
        });
        timerchoice->setAvailableHandler(
            [=](int value) { return isTimerSourceAvailable(value); });

        line = specialFunctionOneWindow->newLine(grid);
        new StaticText(line, rect_t{}, STR_VALUE);
        new TimeEdit(line, rect_t{}, 0, 9 * 60 * 60 - 1,
                     GET_SET_DEFAULT(CFN_PARAM(cfn)));
      } else {
        new StaticText(line, rect_t{}, STR_NO_TIMERS);
      }
      break;
    }

      static const char* const strModules[] = {"Int.", "Ext."};
    case FUNC_SET_FAILSAFE:
      new StaticText(line, rect_t{}, STR_MODULE);
      new Choice(line, rect_t{}, strModules, 0, NUM_MODULES - 1,
                 GET_SET_DEFAULT(CFN_PARAM(cfn)));
      break;

    case FUNC_PLAY_VALUE:
      addSourceChoice(line, STR_VALUE, cfn, MIXSRC_LAST_TELEM);
      break;

    case FUNC_HAPTIC:
      addNumberEdit(line, STR_VALUE, cfn, 0, 3);
      break;

#if defined(FUNCTION_SWITCHES)
    case FUNC_PUSH_CUST_SWITCH: {
      new StaticText(line, rect_t{}, STR_SWITCH);
      auto choice = new Choice(
          line, rect_t{}, 0, switchGetMaxSwitches() - 1,
          [=]() { return switchGetSwitchFromCustomIdx(CFN_CS_INDEX(cfn)); },
          [=](int n) { CFN_CS_INDEX(cfn) = switchGetCustomSwitchIdx(n); },
          STR_SWITCH);
      choice->setTextHandler(
          [=](int n) { return std::string(switchGetDefaultName(n)); });
      choice->setAvailableHandler(switchIsCustomSwitch);

      line = specialFunctionOneWindow->newLine(grid);

      auto edit = addNumberEdit(line, STR_INTERVAL, cfn, PUSH_CS_DURATION_MIN,
                                PUSH_CS_DURATION_MAX);

      edit->setDisplayHandler([](int32_t value) {
        return formatNumberAsString(value, PREC1, 0, nullptr, "s");
      });
    } break;
#endif

    case FUNC_LOGS: {
      if (CFN_PARAM(cfn) == 0)  // use stored value if SF exists
        CFN_PARAM(cfn) = SD_LOGS_PERIOD_DEFAULT;  // otherwise initialize with
                                                  // default value

      auto edit = addNumberEdit(line, STR_INTERVAL, cfn, SD_LOGS_PERIOD_MIN,
                                SD_LOGS_PERIOD_MAX);
      edit->setDefault(
          SD_LOGS_PERIOD_DEFAULT);  // set default period for DEF button
      edit->setDisplayHandler([](int32_t value) {
        return formatNumberAsString(value, PREC1, 0, nullptr, "s");
      });
      break;
    }

    case FUNC_SET_SCREEN:
      CFN_PARAM(cfn) = (int16_t)max(CFN_PARAM(cfn), (int16_t)1);
      CFN_PARAM(cfn) = (int16_t)min(
          CFN_PARAM(cfn), (int16_t)ViewMain::instance()->getMainViewsCount());
      addNumberEdit(line, STR_VALUE, cfn, 1,
                    ViewMain::instance()->getMainViewsCount());
      break;

    case FUNC_ADJUST_GVAR: {
      if (validateSFGV(cfn)) SET_DIRTY();
      new StaticText(line, rect_t{}, STR_GLOBALVAR);
      auto gvarchoice =
          new Choice(line, rect_t{}, 0, MAX_GVARS - 1,
                     GET_DEFAULT(CFN_GVAR_INDEX(cfn)), [=](int32_t newValue) {
                       CFN_GVAR_INDEX(cfn) = newValue;
                       SET_DIRTY();
                       updateSpecialFunctionOneWindow();
                     });
      gvarchoice->setTextHandler([](int32_t value) {
        return std::string(getSourceString(value + MIXSRC_FIRST_GVAR));
      });
      line = specialFunctionOneWindow->newLine(grid);

      new StaticText(line, rect_t{}, STR_MODE);
      auto modechoice = new Choice(
          line, rect_t{}, FUNC_ADJUST_GVAR_CONSTANT, FUNC_ADJUST_GVAR_INCDEC,
          GET_DEFAULT(CFN_GVAR_MODE(cfn)), [=](int32_t newValue) {
            CFN_GVAR_MODE(cfn) = newValue;
            CFN_PARAM(cfn) = 0;
            SET_DIRTY();
            updateSpecialFunctionOneWindow();
          });
      line = specialFunctionOneWindow->newLine(grid);

      modechoice->setTextHandler([](int32_t value) {
        switch (value) {
          case FUNC_ADJUST_GVAR_CONSTANT:
            return std::string(STR_CONSTANT);
          case FUNC_ADJUST_GVAR_SOURCE:
            return std::string(STR_MIXSOURCE);
          case FUNC_ADJUST_GVAR_SOURCERAW:
            return std::string(STR_MIXSOURCERAW);
          case FUNC_ADJUST_GVAR_GVAR:
            return std::string(STR_GLOBALVAR);
          case FUNC_ADJUST_GVAR_INCDEC:
            return std::string(STR_INCDEC);
        }
        return std::string("---");
      });

      switch (CFN_GVAR_MODE(cfn)) {
        case FUNC_ADJUST_GVAR_CONSTANT: {
          int16_t val_min, val_max;
          getMixSrcRange(CFN_GVAR_INDEX(cfn) + MIXSRC_FIRST_GVAR, val_min,
                         val_max);
          addNumberEdit(line, STR_CONSTANT, cfn, val_min, val_max);
          break;
        }
        case FUNC_ADJUST_GVAR_SOURCE:
        case FUNC_ADJUST_GVAR_SOURCERAW:
          addSourceChoice(line, STR_MIXSOURCE, cfn, MIXSRC_LAST_CH);
          break;
        case FUNC_ADJUST_GVAR_GVAR: {
          new StaticText(line, rect_t{}, STR_GLOBALVAR);
          auto gvarchoice = new Choice(line, rect_t{}, 0, MAX_GVARS - 1,
                                       GET_SET_DEFAULT(CFN_PARAM(cfn)));
          gvarchoice->setTextHandler([](int32_t value) {
            return std::string(getSourceString(value + MIXSRC_FIRST_GVAR));
          });
          gvarchoice->setAvailableHandler(
              [=](int value) { return CFN_GVAR_INDEX(cfn) != value; });
          break;
        }
        case FUNC_ADJUST_GVAR_INCDEC: {
          int16_t val_min, val_max;
          getMixSrcRange(CFN_GVAR_INDEX(cfn) + MIXSRC_FIRST_GVAR, val_min,
                         val_max);
          getGVarIncDecRange(val_min, val_max);
          auto numedit = addNumberEdit(line, STR_INCDEC, cfn, val_min, val_max);
          numedit->setDisplayHandler([](int value) {
            return formatNumberAsString(abs(value), 0, 0,
                                        value >= 0 ? "+=" : "-=", nullptr);
          });
          break;
        }
      }
    }
  }

  if (HAS_REPEAT_PARAM(func)) {  // !1x 1x 1s 2s 3s ...
    line = specialFunctionOneWindow->newLine(grid);
    new StaticText(line, rect_t{}, STR_REPEAT);
    if (func == FUNC_PLAY_SCRIPT || func == FUNC_RGB_LED) {
      auto repeat = new Choice(line, rect_t{}, 0, 1,
                               GET_DEFAULT((int8_t)CFN_PLAY_REPEAT(cfn)),
                               SET_DEFAULT(CFN_PLAY_REPEAT(cfn)));
      repeat->setTextHandler([](int32_t value) {
        // 0 == repeat at 50ms interval for backward compatibility
        return (value == 0) ? std::string("On") : std::string("1x");
      });
    } else {
      auto repeat = new NumberEdit(line, rect_t{}, -1, 60 / CFN_PLAY_REPEAT_MUL,
                                   GET_DEFAULT((int8_t)CFN_PLAY_REPEAT(cfn)),
                                   SET_DEFAULT(CFN_PLAY_REPEAT(cfn)));
      repeat->setDisplayHandler([](int32_t value) {
        if (value == 0)
          return std::string("1x");
        else if (value == (int8_t)CFN_PLAY_REPEAT_NOSTART)
          return std::string("!1x");
        else {
          return formatNumberAsString(value * CFN_PLAY_REPEAT_MUL, 0, 0,
                                      nullptr, "s");
        }
      });
    }
  }

  line = specialFunctionOneWindow->newLine(grid);
  new StaticText(line, rect_t{}, STR_ENABLE);
  new ToggleSwitch(
      line, rect_t{}, GET_DEFAULT(CFN_ACTIVE(cfn)), [=](int newValue) {
        CFN_ACTIVE(cfn) = newValue;
        SET_DIRTY();
        if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT || CFN_FUNC(cfn) == FUNC_RGB_LED)
          LUA_LOAD_MODEL_SCRIPTS();
      });
}

void FunctionEditPage::buildBody(Window* form)
{
  form->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_LARGE);

  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);

  CustomFunctionData* cfn = customFunctionData();

  // Was this slot completely unconfigured when this editor was opened? If
  // so, the pilot is setting it up for the first time in one sitting
  // (assign a switch, then pick a function) and it should come out enabled
  // per newSF()'s preset -- CFN_RESET() below unconditionally clears
  // 'active' on every function-type change, which otherwise re-disables a
  // brand new function the instant its actual function is chosen. That
  // reset is still wanted when *editing an already-configured* function
  // (repurposing it should force the pilot to re-confirm enable), so it
  // only gets overridden here for genuinely new slots.
  const bool wasEmptyOnOpen = cfn->isEmpty();

  // Switch
  auto line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_SF_SWITCH);
  auto switchChoice = new SwitchChoice(line, rect_t{}, SWSRC_FIRST, SWSRC_LAST,
                                       GET_SET_DEFAULT(CFN_SWITCH(cfn)));
  switchChoice->setAvailableHandler(
      [=](int value) { return isSwitchAvailable(value); });

  // Patch function in case not available
  if (!isAssignableFunctionAvailable(CFN_FUNC(cfn))) {
    auto func = 0;
    while (!isAssignableFunctionAvailable(func) && (func < FUNC_MAX - 1)) {
      func++;
    }
    if (func < FUNC_MAX - 1) {
      CFN_FUNC(cfn) = func;
    }
  }

  // Function
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_FUNC);
  auto functionChoice = new Choice(
      line, rect_t{}, 0, FUNC_MAX - 1,
      GET_DEFAULT(getFuncSortIdx(CFN_FUNC(cfn))), [=](int32_t newValue) {
        Functions newFunc = cfn_sorted[newValue];
        // If changing from Lua script then reload to remove old reference
        if ((CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT ||
             CFN_FUNC(cfn) == FUNC_RGB_LED) &&
            newFunc != FUNC_PLAY_SCRIPT && newFunc != FUNC_RGB_LED)
          LUA_LOAD_MODEL_SCRIPTS();
        CFN_FUNC(cfn) = newFunc;
        CFN_RESET(cfn);
        applyFunctionChangeActiveState(cfn, wasEmptyOnOpen);
        SET_DIRTY();
        updateSpecialFunctionOneWindow();
      });
  functionChoice->setTextHandler(
      [=](int val) { return funcGetLabel(cfn_sorted[val]); });
  functionChoice->setAvailableHandler([=](int value) {
    return isAssignableFunctionAvailable(cfn_sorted[value]);
  });

  specialFunctionOneWindow = new Window(form, rect_t{});
  updateSpecialFunctionOneWindow();
}

//-----------------------------------------------------------------------------

FunctionsPage::FunctionsPage(CustomFunctionData* functions,
                             const PageDef& pageDef, const char* prefix) :
    PageGroupItem(pageDef), functions(functions), prefix(prefix)
{
}

void FunctionsPage::rebuild(Window* window)
{
  // When window.clear() is called the last button on screen is given focus
  // (???) This causes the page to jump to the end when rebuilt. Set flag to
  // bypass the button focus handler and reset focusIndex when rebuilding
  isRebuilding = true;
  window->clear();
  build(window);
  isRebuilding = false;
}

void FunctionsPage::newSF(Window* window, bool pasteSF)
{
  Menu* menu = new Menu();
  menu->setTitle(title);

  // search for unused switches
  for (uint8_t i = 0; i < MAX_SPECIAL_FUNCTIONS; i++) {
    CustomFunctionData* cfn = customFunctionData(i);
    if (cfn->swtch == 0) {
      menu->addLineBuffered(prefix + std::to_string(i + 1), [=]() {
        if (pasteSF) {
          pasteSpecialFunction(window, i);
        } else {
          presetNewFunctionData(cfn);
          editSpecialFunction(window, i);
        }
      });
    }
  }
  menu->updateLines();
}

void FunctionsPage::pasteSpecialFunctionData(uint8_t index)
{
  CustomFunctionData* cfn = customFunctionData(index);
  if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT) LUA_LOAD_MODEL_SCRIPTS();
  *cfn = clipboard.data.cfn;
  if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT) LUA_LOAD_MODEL_SCRIPTS();
  storageDirty(EE_MODEL);
  if (functions == g_model.customFn) publishSpecialFunctionsChanged();
}

void FunctionsPage::pasteSpecialFunction(Window* window, uint8_t index)
{
  pasteSpecialFunctionData(index);
  focusIndex = index;
  rebuild(window);
}

void FunctionsPage::pasteSpecialFunction(Window* window, uint8_t index,
                                         FunctionLineButton&)
{
  pasteSpecialFunctionData(index);
  focusIndex = index;
}

void FunctionsPage::editSpecialFunction(Window* window, uint8_t index)
{
  auto edit = editPage(index);
  edit->setCloseHandler([=]() {
    CustomFunctionData* cfn = customFunctionData(index);
    if (cfn->swtch != 0) focusIndex = index;
    rebuild(window);
    if (functions == g_model.customFn) publishSpecialFunctionsChanged();
  });
}

bool FunctionsPage::openRoute(const Route& r, uint8_t depth)
{
  if (depth >= r.depth) return true;
  if (r.pages[depth] != editorPageId()) return false;

  uint8_t index = static_cast<uint8_t>(r.params[depth]);
  if (index >= MAX_SPECIAL_FUNCTIONS) return false;
  if (customFunctionData(index)->swtch == 0) return false;

  editSpecialFunction(pageWindow, index);
  return true;
}

void FunctionsPage::editSpecialFunction(Window* window, uint8_t index,
                                        FunctionLineButton& button)
{
  auto edit = editPage(index);
  auto buttonPtr = &button;
  edit->setCloseHandler([=]() {
    CustomFunctionData* cfn = customFunctionData(index);
    if (cfn->swtch != 0) {
      focusIndex = index;
      buttonPtr->requestLineUpdate();
      if (functions == g_model.customFn) publishSpecialFunctionsChanged();
      return;  // Skip full rebuild
    }
    rebuild(window);
    if (functions == g_model.customFn) publishSpecialFunctionsChanged();
  });
}

void FunctionsPage::plusPopup(Window* window)
{
  if (clipboard.type == CLIPBOARD_TYPE_CUSTOM_FUNCTION) {
    Menu* menu = new Menu();
    menu->addLine(STR_NEW, [=]() { newSF(window, false); });
    menu->addLine(STR_PASTE, [=]() { newSF(window, true); });
  } else {
    newSF(window, false);
  }
}

void FunctionsPage::build(Window* window)
{
  pageWindow = window;
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_TINY);

  bool hasEmptyFunction = false;

  // Reset focusIndex after switching tabs
  if (!isRebuilding) focusIndex = prevFocusIndex;

  for (uint8_t i = 0; i < MAX_SPECIAL_FUNCTIONS; i++) {
    CustomFunctionData* cfn = customFunctionData(i);

    bool isActive = (cfn->swtch != 0);

    if (isActive) {
      auto button = functionButton(
          window,
          rect_t{0, 0, window->width() - PAD_LARGE - PAD_SMALL, SF_BUTTON_H},
          i);

      button->setGridCell(LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0,
                          1);

      if (focusIndex == i) {
        button->focus();
      }

      button->setFocusHandler([=](bool hasFocus) {
        if (hasFocus && !isRebuilding) {
          prevFocusIndex = focusIndex;
          focusIndex = i;
        }
      });

      button->setPressHandler([=]() {
        editSpecialFunction(window, i, *button);
        return 0;
      });

      button->setLongPressHandler([=]() -> uint8_t {
        Menu* menu = new Menu();
        menu->addLine(STR_EDIT,
                      [=]() { editSpecialFunction(window, i, *button); });
        if (isActive) {
          menu->addLine(STR_COPY, [=]() {
            clipboard.type = CLIPBOARD_TYPE_CUSTOM_FUNCTION;
            clipboard.data.cfn = *cfn;
          });
        }
        if (clipboard.type == CLIPBOARD_TYPE_CUSTOM_FUNCTION) {
          menu->addLine(STR_PASTE,
                        [=]() { pasteSpecialFunction(window, i, *button); });
        }
        CustomFunctionData* cfn = customFunctionData(i);
        if (CFN_ACTIVE(cfn)) {
          menu->addLine(STR_DISABLE, [=]() {
            CFN_ACTIVE(cfn) = 0;
            SET_DIRTY();
            rebuild(window);
          });
        } else {
          menu->addLine(STR_ENABLE, [=]() {
            CFN_ACTIVE(cfn) = 1;
            SET_DIRTY();
            if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT ||
                CFN_FUNC(cfn) == FUNC_RGB_LED)
              LUA_LOAD_MODEL_SCRIPTS();
            rebuild(window);
          });
        }
        if (customFunctionData(MAX_SPECIAL_FUNCTIONS - 1)->isEmpty()) {
          for (int j = i; j < MAX_SPECIAL_FUNCTIONS; j++) {
            if (!customFunctionData(j)->isEmpty()) {
              menu->addLine(STR_INSERT, [=]() {
                memmove(cfn + 1, cfn,
                        (MAX_SPECIAL_FUNCTIONS - i - 1) *
                            sizeof(CustomFunctionData));
                memset(cfn, 0, sizeof(CustomFunctionData));
                presetNewFunctionData(cfn);
                editSpecialFunction(window, i);
              });
              break;
            }
          }
        }
        if (isActive) {
          menu->addLine(STR_CLEAR, [=]() {
            if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT) LUA_LOAD_MODEL_SCRIPTS();
            memset(cfn, 0, sizeof(CustomFunctionData));
            SET_DIRTY();
            rebuild(window);
          });
        }
        for (int j = i; j < MAX_SPECIAL_FUNCTIONS; j++) {
          if (!customFunctionData(j)->isEmpty()) {
            menu->addLine(STR_DELETE, [=]() {
              if (CFN_FUNC(cfn) == FUNC_PLAY_SCRIPT) LUA_LOAD_MODEL_SCRIPTS();
              memmove(
                  cfn, cfn + 1,
                  (MAX_SPECIAL_FUNCTIONS - i - 1) * sizeof(CustomFunctionData));
              memset(customFunctionData(MAX_SPECIAL_FUNCTIONS - 1), 0,
                     sizeof(CustomFunctionData));
              SET_DIRTY();
              rebuild(window);
            });
            break;
          }
        }
        return 0;
      });
    } else {
      hasEmptyFunction = true;
    }
  }

  if (hasEmptyFunction) {
    addButton = new TextButton(
        window, rect_t{0, 0, window->width() - PAD_SMALL * 2, SF_BUTTON_H},
        LV_SYMBOL_PLUS, [=]() {
          plusPopup(window);
          return 0;
        });

    addButton->setLongPressHandler([=]() -> uint8_t {
      plusPopup(window);
      return 0;
    });

    addButton->setFocusHandler([=](bool hasFocus) {
      if (hasFocus && !isRebuilding) {
        prevFocusIndex = focusIndex;
      }
    });
  } else {
    addButton = nullptr;
  }
}

//-----------------------------------------------------------------------------

class SpecialFunctionLineButton : public FunctionLineButton
{
 public:
  SpecialFunctionLineButton(Window* parent, const rect_t& rect, uint8_t index) :
      FunctionLineButton(parent, rect, &g_model.customFn[index], index, "SF")
  {
  }

 protected:
  bool isActive() const override
  {
    return modelFunctionsContext.activeSwitches.load(
               std::memory_order_relaxed) &
           ((MASK_CFN_TYPE)1 << index);
  }

  void setDirty() const override
  {
    storageDirty(EE_MODEL);
    publishSpecialFunctionsChanged();
  }
};

//-----------------------------------------------------------------------------

class SpecialFunctionEditPage : public FunctionEditPage
{
 public:
  SpecialFunctionEditPage(uint8_t index, Route route) :
      FunctionEditPage(index, ICON_MODEL_SPECIAL_FUNCTIONS, route, STR_MENUCUSTOMFUNC,
                       "SF")
  {
  }

 protected:
  bool isActive() const override
  {
    return modelFunctionsContext.activeSwitches.load(
               std::memory_order_relaxed) &
           ((MASK_CFN_TYPE)1 << index);
  }

  bool isSwitchAvailable(int value) const override
  {
    return ::isSwitchAvailable(value, ModelCustomFunctionsContext);
  }

  CustomFunctionData* customFunctionData() const override
  {
    return &g_model.customFn[index];
  }

  bool isAssignableFunctionAvailable(int function) const override
  {
    return ::isAssignableFunctionAvailable(function, true);
  }

  void setDirty() const override
  {
    storageDirty(EE_MODEL);
    publishSpecialFunctionsChanged();
  }
};

//-----------------------------------------------------------------------------

SpecialFunctionsPage::SpecialFunctionsPage(const PageDef& pageDef) :
    FunctionsPage(g_model.customFn, pageDef, "SF")
{
}

CustomFunctionData* SpecialFunctionsPage::customFunctionData(
    uint8_t index) const
{
  return &g_model.customFn[index];
}

FunctionEditPage* SpecialFunctionsPage::editPage(uint8_t index) const
{
  return new SpecialFunctionEditPage(index,
      route().appended(RP_SPECIAL_FUNCTION_EDIT, static_cast<int16_t>(index)));
}

FunctionLineButton* SpecialFunctionsPage::functionButton(Window* parent,
                                                         const rect_t& rect,
                                                         uint8_t index) const
{
  return new SpecialFunctionLineButton(parent, rect, index);
}

void SpecialFunctionsPage::setDirty() const
{
  storageDirty(EE_MODEL);
  publishSpecialFunctionsChanged();
}

//-----------------------------------------------------------------------------

class GlobalFunctionLineButton : public FunctionLineButton
{
 public:
  GlobalFunctionLineButton(Window* parent, const rect_t& rect, uint8_t index) :
      FunctionLineButton(parent, rect, &g_eeGeneral.customFn[index], index,
                         "GF")
  {
  }

 protected:
  bool isActive() const override
  {
    return globalFunctionsContext.activeSwitches.load(
               std::memory_order_relaxed) &
           ((MASK_CFN_TYPE)1 << index);
  }

  void setDirty() const override { storageDirty(EE_GENERAL); }
};

//-----------------------------------------------------------------------------

class GlobalFunctionEditPage : public FunctionEditPage
{
 public:
  GlobalFunctionEditPage(uint8_t index, Route route) :
      FunctionEditPage(index, ICON_RADIO_GLOBAL_FUNCTIONS, route, STR_MENUSPECIALFUNCS,
                       "GF")
  {
  }

 protected:
  bool isActive() const override
  {
    return globalFunctionsContext.activeSwitches.load(
               std::memory_order_relaxed) &
           ((MASK_CFN_TYPE)1 << index);
  }

  bool isSwitchAvailable(int value) const override
  {
    return ::isSwitchAvailable(value, GeneralCustomFunctionsContext);
  }

  CustomFunctionData* customFunctionData() const override
  {
    return &g_eeGeneral.customFn[index];
  }

  bool isAssignableFunctionAvailable(int function) const override
  {
    return ::isAssignableFunctionAvailable(function, false);
  }

  void setDirty() const override { storageDirty(EE_GENERAL); }
};

//-----------------------------------------------------------------------------

GlobalFunctionsPage::GlobalFunctionsPage(const PageDef& pageDef) :
    FunctionsPage(g_eeGeneral.customFn, pageDef, "GF")
{
}

CustomFunctionData* GlobalFunctionsPage::customFunctionData(uint8_t index) const
{
  return &g_eeGeneral.customFn[index];
}

FunctionEditPage* GlobalFunctionsPage::editPage(uint8_t index) const
{
  return new GlobalFunctionEditPage(index,
      route().appended(RP_GLOBAL_FUNCTION_EDIT, static_cast<int16_t>(index)));
}

FunctionLineButton* GlobalFunctionsPage::functionButton(Window* parent,
                                                        const rect_t& rect,
                                                        uint8_t index) const
{
  return new GlobalFunctionLineButton(parent, rect, index);
}

void GlobalFunctionsPage::setDirty() const { storageDirty(EE_GENERAL); }

//-----------------------------------------------------------------------------

#if defined(SIMU)
void playCustomFunctionFile(const CustomFunctionData* sd, uint8_t id);

// A brand new Special/Global Function slot must come out both enabled (so a
// fully-configured function actually fires, per the new-creation UX) and
// genuinely inert until the pilot picks a real function (so an untouched
// slot -- e.g. only a trigger switch was assigned -- can never silently
// start doing something, such as the raw zero-default FUNC_OVERRIDE_CHANNEL
// would).
bool presetNewFunctionDataIsEnabledAndInertForTest()
{
  CustomFunctionData cfn;
  memset(&cfn, 0, sizeof(cfn));

  presetNewFunctionData(&cfn);

  bool enabled = CFN_ACTIVE(&cfn) == 1;
  bool usesPlayTrack = CFN_FUNC(&cfn) == FUNC_PLAY_TRACK;
  bool trackNameEmpty = cfn.play.name[0] == '\0';
  bool assignableForSF = isAssignableFunctionAvailable(FUNC_PLAY_TRACK, true);
  bool assignableForGF = isAssignableFunctionAvailable(FUNC_PLAY_TRACK, false);

  // Still an "empty" (invisible/inert) slot until a trigger switch is
  // assigned -- presetting func/active must not by itself make the function
  // fire or show up in the list.
  bool stillEmptyUntilSwitchChosen = cfn.isEmpty();

  return enabled && usesPlayTrack && trackNameEmpty && assignableForSF &&
        assignableForGF && stillEmptyUntilSwitchChosen;
}

bool presetFunctionDataDoesNotPlayWithEmptyTrackNameForTest()
{
  CustomFunctionData cfn;
  memset(&cfn, 0, sizeof(cfn));
  presetNewFunctionData(&cfn);

  // playCustomFunctionFile() early-returns when play.name[0] == '\0', so
  // this must not crash and must be a pure no-op (no file lookup, no audio
  // queued). There is no lightweight way to assert "nothing was queued" from
  // a unit test without a full audio harness, so this proves the no-crash /
  // early-return contract that the runtime relies on for inertness.
  playCustomFunctionFile(&cfn, 0);
  return true;
}

// Reproduces the exact "create a new Special Function (trigger + Play
// Sound), exit" scenario at the data level: newSF() presets a fresh slot
// enabled, the pilot assigns a switch (still empty until then), then picks
// a real function from the Choice widget -- which calls CFN_RESET()
// (clearing 'active') followed by applyFunctionChangeActiveState(). The
// slot must still be enabled afterwards, or a fully-configured new SF would
// once again silently land disabled.
bool firstTimeFunctionPickStaysEnabledForTest()
{
  CustomFunctionData cfn;
  memset(&cfn, 0, sizeof(cfn));
  presetNewFunctionData(&cfn);

  // FunctionEditPage::buildBody() captures this before the pilot touches
  // anything.
  const bool wasEmptyOnOpen = cfn.isEmpty();
  if (!wasEmptyOnOpen) return false;  // sanity: preset must not set swtch

  // Pilot assigns a trigger switch (still doesn't touch 'active' or 'func').
  cfn.swtch = 1;

  // Pilot picks "Play Sound" from the Function choice -- mirrors
  // FunctionEditPage::buildBody()'s onChange handler exactly.
  cfn.func = FUNC_PLAY_SOUND;
  CFN_RESET(&cfn);
  applyFunctionChangeActiveState(&cfn, wasEmptyOnOpen);

  return cfn.active == 1;
}

// Repurposing an *already configured* function must still require an
// explicit re-enable: applyFunctionChangeActiveState() must be a no-op when
// the slot wasn't empty when its editor opened.
bool functionChangeOnExistingSlotStillRequiresReenableForTest()
{
  CustomFunctionData cfn;
  memset(&cfn, 0, sizeof(cfn));
  cfn.swtch = 1;       // already configured...
  cfn.func = FUNC_PLAY_SOUND;
  cfn.active = 1;       // ...and enabled

  const bool wasEmptyOnOpen = cfn.isEmpty();
  if (wasEmptyOnOpen) return false;  // sanity: slot must be non-empty

  // Pilot repurposes it to a different function.
  cfn.func = FUNC_PLAY_TRACK;
  CFN_RESET(&cfn);
  applyFunctionChangeActiveState(&cfn, wasEmptyOnOpen);

  return cfn.active == 0;
}
#endif
