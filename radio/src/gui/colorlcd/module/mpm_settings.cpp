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

#include "mpm_settings.h"

#include "choice.h"
#include "ds_core.h"
#include "edgetx.h"
#include "getset_helpers.h"
#include "io/multi_protolist.h"
#include "multi_rfprotos.h"
#include "numberedit.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

class MPMProtoOption : public ds::FieldGroup {
 public:
  // The protocol option field is one of Choice / NumberEdit / ToggleSwitch (only
  // one visible at a time) plus an optional RSSI readout, flowed side-by-side
  // into the FieldGroup's content (which owns the layout, gaps and touch floor;
  // the RSSI's former padTop is absorbed by that layout). The leading label is
  // protocol-dependent and set live in update(); it is built with a placeholder
  // so the FieldGroup materializes its label column, then retargeted via
  // setLabel() before the row is ever shown.
  MPMProtoOption(Window* form) :
      ds::FieldGroup(form, " ")
  {
    Window* box = content();

    choice = new Choice(box, rect_t{}, 0, 0, nullptr);
    edit = new NumberEdit(box, rect_t{}, 0, 0, nullptr);
    cb = new ToggleSwitch(box, rect_t{}, nullptr, nullptr);
    rssi = new DynamicNumber<uint16_t>(
        box, rect_t{}, [] { return (uint16_t)TELEMETRY_RSSI(); }, COLOR_THEME_PRIMARY1_INDEX, 0,
        getRxStatLabels()->label, getRxStatLabels()->unit);
  }

  void update(const MultiRfProtocols::RfProto* rfProto, ModuleData* md,
              uint8_t moduleIdx)
  {
    if (!rfProto || !getMultiOptionTitle(moduleIdx)) {
      hide();
      return;
    }

    show();

    const char* title = getMultiOptionTitle(moduleIdx);
    setLabel(title);

    choice->hide();
    edit->hide();
    cb->hide();
    rssi->hide();

    int8_t min, max;
    getMultiOptionValues(rfProto->proto, min, max);

    if (title == STR_MULTI_RFPOWER) {
      choice->setValues(STR_MULTI_POWER);
      choice->setMin(0);
      choice->setMax(15);
      choice->setGetValueHandler(GET_DEFAULT(md->multi.optionValue));
      choice->setSetValueHandler(SET_DEFAULT(md->multi.optionValue));
      choice->show();
      choice->update();
    } else if (title == STR_MULTI_TELEMETRY) {  // Bayang
      choice->setValues(STR_MULTI_BAYANG_OPTIONS);
      choice->setMin(min);
      choice->setMax(max);
      choice->setGetValueHandler(GET_DEFAULT(md->multi.optionValue));
      choice->setSetValueHandler(SET_DEFAULT(md->multi.optionValue));
      choice->show();
      choice->update();
    } else if (title ==
               STR_MULTI_WBUS) {  // e.g. WFLY2 but may not be used anymore
      choice->setValues(STR_MULTI_WBUS_MODE);
      choice->setMin(0);
      choice->setMax(1);
      choice->setGetValueHandler(GET_DEFAULT(md->multi.optionValue));
      choice->setSetValueHandler(SET_DEFAULT(md->multi.optionValue));
      choice->show();
      choice->update();
    } else if (rfProto->proto == MODULE_SUBTYPE_MULTI_FS_AFHDS2A) {
      edit->setMin(50);
      edit->setMax(400);
      edit->setGetValueHandler(GET_DEFAULT(50 + 5 * md->multi.optionValue));
      edit->setSetValueHandler(
          SET_VALUE(md->multi.optionValue, (newValue - 50) / 5));
      edit->setStep(5);
      edit->update();
      edit->show();
    } else if (rfProto->proto == MODULE_SUBTYPE_MULTI_DSM2) {
      cb->setGetValueHandler([=]() { return md->multi.optionValue & 0x01; });
      cb->setSetValueHandler([=](int8_t newValue) {
        md->multi.optionValue = (md->multi.optionValue & 0xFE) + newValue;
        SET_DIRTY();
      });
      cb->update();
      cb->show();
    } else {
      if (min == 0 && max == 1) {
        cb->setGetValueHandler(GET_DEFAULT(md->multi.optionValue));
        cb->setSetValueHandler(SET_DEFAULT(md->multi.optionValue));
        cb->update();
        cb->show();
      } else {
        edit->setMin(min);
        edit->setMax(max);
        edit->setGetValueHandler(GET_DEFAULT(md->multi.optionValue));
        edit->setSetValueHandler(SET_DEFAULT(md->multi.optionValue));
        edit->show();
        edit->update();
        if (title == STR_MULTI_RFTUNE) {
          rssi->setPrefix(getRxStatLabels()->label);
          rssi->setSuffix(getRxStatLabels()->unit);
          rssi->show();
        }
      }
    }
  }

 protected:
  Choice* choice;
  NumberEdit* edit;
  ToggleSwitch* cb;
  DynamicNumber<uint16_t>* rssi;
};

struct MPMSubtype : public ds::FormRow {
  MPMSubtype(MultimoduleSettings* parent, uint8_t moduleIdx) :
      ds::FormRow(parent, STR_RF_PROTOCOL)
  {
    this->moduleIdx = moduleIdx;
    this->DSM2lastSubType = g_model.moduleData[this->moduleIdx].subType;
    this->DSM2autoUpdated = false;

    auto md = &g_model.moduleData[moduleIdx];
    choice = new Choice(
        this, rect_t{}, 0, 0, [=]() { return md->subType; },
        [=](int16_t newValue) {
          md->subType = newValue;
          if (!DSM2autoUpdated)  // reset MPM options only if user triggered
            resetMultiProtocolsOptions(moduleIdx);
          DSM2autoUpdated = false;
          SET_DIRTY();
          parent->update();
        });
  }

  void update(const MultiRfProtocols::RfProto* rfProto, uint8_t moduleIdx)
  {
    if (!rfProto || rfProto->subProtos.size() == 0) {
      hide();
      return;
    }

    choice->setValues(rfProto->subProtos);
    choice->update();
    choice->setMax(rfProto->subProtos.size() - 1);

    show();
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Window::onLiveCheckEvents(live);

    //
    // DSM2: successful bind in auto mode changes DSM2 subType
    //
    ModuleData* md = &g_model.moduleData[this->moduleIdx];

    if (md->multi.rfProtocol ==
        MODULE_SUBTYPE_MULTI_DSM2) {  // do this only for DSM2
      uint8_t subType = md->subType;  // fetch DSM2 subType

      if (subType != DSM2lastSubType) {  // if DSM2 subType has auto changed
        DSM2autoUpdated = true;          // indicate this was not user triggered
        DSM2lastSubType = subType;       // memorize new DSM2 subType
        choice->setValue(subType);       // set new DSM2 subType
      }
    }
  }

 protected:
  Choice* choice;
  uint8_t moduleIdx;
  uint8_t DSM2lastSubType;
  bool DSM2autoUpdated;
};

struct MPMDSMCloned : public ds::FormRow {
  MPMDSMCloned(Window* form, uint8_t moduleIdx) :
      ds::FormRow(form, STR_SUBTYPE)
  {
    auto md = &g_model.moduleData[moduleIdx];
    choice = new Choice(this, rect_t{}, STR_MULTI_DSM_CLONE, 0, 1, 0, 0);

    choice->setGetValueHandler(
        GET_DEFAULT((md->multi.optionValue & 0x04) >> 2));
    choice->setSetValueHandler(
        SET_VALUE(md->multi.optionValue,
                  (md->multi.optionValue & 0xFB) + (newValue << 2)));
  }

  void update() const
  {
    choice->update();
  }

 private:
  Choice* choice;
};

struct MPMServoRate : public ds::FormRow {
  MPMServoRate(Window* form, uint8_t moduleIdx) :
      ds::FormRow(form, STR_MULTI_SERVOFREQ)
  {
    auto md = &g_model.moduleData[moduleIdx];
    choice = new Choice(this, rect_t{}, STR_MULTI_DSM_OPTIONS, 0, 1, 0, 0);

    choice->setGetValueHandler(
        GET_DEFAULT((md->multi.optionValue & 0x02) >> 1));
    choice->setSetValueHandler(
        SET_VALUE(md->multi.optionValue,
                  (md->multi.optionValue & 0xFD) + (newValue << 1)));
  }

  void update() const
  {
    choice->update();
  }

 private:
  Choice* choice;
};

struct MPMAutobind : public ds::FormRow {
  MPMAutobind(Window* form, uint8_t moduleIdx) :
      ds::FormRow(form, STR_MULTI_AUTOBIND)
  {
    auto md = &g_model.moduleData[moduleIdx];
    cb = new ToggleSwitch(this, rect_t{},
                          GET_SET_DEFAULT(md->multi.autoBindMode));
  }

  void update() const { cb->update(); }

 private:
  ToggleSwitch* cb;
};

struct MPMChannelMap : public ds::FormRow {
  MPMChannelMap(Window* form, uint8_t moduleIdx) :
      ds::FormRow(form, STR_CHANNEL_MAP)
  {
    auto md = &g_model.moduleData[moduleIdx];
    // disableMapping is stored negatively (1 = mapping disabled); invert so
    // the switch reads ON when channel mapping is enabled.
    cb = new ToggleSwitch(this, rect_t{},
                          GET_SET_INVERTED(md->multi.disableMapping));
  }

  void update(const MultiRfProtocols::RfProto* rfProto)
  {
    if (rfProto && rfProto->supportsDisableMapping()) {
      show();
      cb->update();
    } else {
      hide();
    }
  }

 private:
  ToggleSwitch* cb;
};

MultimoduleSettings::MultimoduleSettings(Window* parent,
                                         const FlexGridLayout& /*g*/,
                                         uint8_t moduleIdx) :
    Window(parent, rect_t{}),
    md(&g_model.moduleData[moduleIdx]),
    moduleIdx(moduleIdx)
{
  setFlexLayout();

  // TODO: needs to be placed differently
  // MultiModuleStatus &status = getMultiModuleStatus(moduleIdx);
  // if (status.protocolName[0] && status.isValid()) {
  //   new StaticText(this, grid.getFieldSlot(2, 1), status.protocolName,
  //                  COLOR_THEME_PRIMARY1);
  //   grid.nextLine();
  // }

  // Multimodule status
  new ds::FormRow(this, STR_MODULE_STATUS, [=](Window* slot) {
    new DynamicText(
        slot, rect_t{},
        [=] {
          char msg[64] = "";
          getModuleStatusString(moduleIdx, msg);
          return std::string(msg);
        });
  });

  st_line = new MPMSubtype(this, moduleIdx);

  cl_line = new MPMDSMCloned(this, moduleIdx);
  opt_line = new MPMProtoOption(this);
  sr_line = new MPMServoRate(this, moduleIdx);
  ab_line = new MPMAutobind(this, moduleIdx);

  // Low power mode
  new ds::FormRow(this, STR_MULTI_LOWPOWER, [=](Window* slot) {
    lp_mode =
        new ToggleSwitch(slot, rect_t{}, GET_SET_DEFAULT(md->multi.lowPowerMode));
  });

#if defined(MANUFACTURER_FRSKY)
  // Telemetry: disableTelemetry is stored negatively (1 = telemetry
  // disabled); invert so the switch reads ON when telemetry is enabled.
  new ds::FormRow(this, STR_MULTI_TELEMETRY, [=](Window* slot) {
    disable_telem = new ToggleSwitch(
        slot, rect_t{}, GET_SET_INVERTED(md->multi.disableTelemetry));
  });
#endif

  cm_line = new MPMChannelMap(this, moduleIdx);

  // Ensure elements properly initalised
  update();
}

void MultimoduleSettings::update()
{
  auto mpm_rfprotos = MultiRfProtocols::instance(moduleIdx);
  auto rfProto = mpm_rfprotos->getProto(md->multi.rfProtocol);

  st_line->update(rfProto, moduleIdx);
  opt_line->update(rfProto, md, moduleIdx);

  auto multi_proto = md->multi.rfProtocol;
  if (multi_proto == MODULE_SUBTYPE_MULTI_DSM2) {
    sr_line->show();
    sr_line->update();
    ab_line->hide();
  } else {
    sr_line->hide();
    ab_line->show();
    ab_line->update();
  }

  if (isMultiProtocolDSMCloneAvailable(moduleIdx)) {
    cl_line->show();
    cl_line->update();
  } else {
    cl_line->hide();
  }

  lp_mode->update();
#if defined(MANUFACTURER_FRSKY)
  disable_telem->update();
#endif
  cm_line->update(rfProto);
}
