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

#include "gvar_numberedit.h"
#include "edgetx.h"

#include <new>

GVarNumberEdit::GVarNumberEdit(Window* parent, int32_t vmin,
                               int32_t vmax, std::function<int32_t()> getValue,
                               std::function<void(int32_t)> setValue,
                               LcdFlags textFlags, int32_t voffset, int32_t vdefault) :
    Window(parent, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW + GV_BTN_W + PAD_TINY * 3, EdgeTxStyles::UI_ELEMENT_HEIGHT + PAD_TINY * 2}),  // ds-allow: composite GVAR field; fixed overall width packing NumberEdit + GV button plus their gaps, not a single DS FormRow control
    vmin(vmin),
    vmax(vmax),
    getValue(getValue),
    setValue(setValue),
    voffset(voffset)
{
  setTextFlag(textFlags);
  padAll(PAD_TINY);  // ds-allow: composite GVAR field; tight internal padding around the NumberEdit + GV button pack, not a single DS FormRow control

  // GVAR field
  gvar_field = new (std::nothrow) Choice(
      this, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, -MAX_GVARS, MAX_GVARS - 1,
      [=]() {
        return GV_INDEX_FROM_VALUE(getValue());
      },
      [=](int idx) {
        // A genuine pick invalidates the pre-GV raw value cache: from now
        // on, switching back to raw mode should reflect the chosen GVAR
        // rather than resurrect whatever raw value preceded GV mode.
        rawValueBeforeGVarValid = false;
        setValue(GV_VALUE_FROM_INDEX(idx));
      });
  if (gvar_field)
    gvar_field->setTextHandler(
        [=](int32_t value) { return getGVarString(value); });

  num_field = new (std::nothrow) NumberEdit(
      this, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, vmin, vmax, [=]() { return getValue() + voffset; },
      nullptr, textFlags);
  if (num_field && vdefault != NO_DEFAULT) num_field->setDefault(vdefault);

#if defined(GVARS)
  // The GVAR button
  if (modelGVEnabled()) {
    m_gvBtn = new (std::nothrow) TextButton(this, {EdgeTxStyles::EDIT_FLD_WIDTH_NARROW + PAD_TINY, 0, GV_BTN_W, 0}, STR_GV, [=]() {  // ds-allow: composite GVAR field; GV button placed at a fixed offset right of the NumberEdit inside the packed control, not a DS FormRow
      switchGVarMode();
      return GV_IS_GV_VALUE(getValue());
    });
    if (m_gvBtn) m_gvBtn->check(GV_IS_GV_VALUE(getValue()));
  }
#endif

  // update field type based on value
  update();
}

void GVarNumberEdit::switchGVarMode()
{
#if defined(GVARS)
  if (modelGVEnabled()) {
    int32_t value = getValue();
    if (GV_IS_GV_VALUE(value)) {
      // Switching back to raw mode. If the user never actually picked a
      // GVAR while in GV mode, restore the raw value cached when GV mode
      // was entered instead of deriving a new one from GV1's unrelated
      // live value — otherwise the previously tuned raw value is lost.
      if (rawValueBeforeGVarValid) {
        setValue(rawValueBeforeGVar);
      } else {
        setValue((textFlags & PREC1)
                     ? GET_GVAR_PREC1(value, vmin, vmax, mixerCurrentFlightMode)
                     : GET_GVAR(value, vmin, vmax, mixerCurrentFlightMode));
      }
      rawValueBeforeGVarValid = false;
    } else {
      // Switching into GV mode: remember the raw value so it can be
      // restored if the user backs out again without picking a GVAR.
      rawValueBeforeGVar = value;
      rawValueBeforeGVarValid = true;
      setValue(GV_VALUE_FROM_INDEX(0));
    }

    if (m_gvBtn) m_gvBtn->check(GV_IS_GV_VALUE(value));

    // update field type based on value
    update();
  }
#endif
}

void GVarNumberEdit::setSuffix(const std::string& value)
{
  if (num_field) num_field->setSuffix(value);
}

void GVarNumberEdit::update()
{
  bool has_focus = act_field && act_field->hasFocus();

  if (gvar_field) gvar_field->hide();
  if (num_field) num_field->hide();

  int32_t value = getValue();
  if (GV_IS_GV_VALUE(value)) {
    // GVAR mode
    if (!gvar_field || !num_field) return;
    act_field = gvar_field;
    num_field->setSetValueHandler(nullptr);
    gvar_field->update();
    gvar_field->show();
  } else {
    // number edit mode
    if (!num_field) return;
    act_field = num_field;
    num_field->setSetValueHandler(
        [=](int32_t newValue) { return setValue(newValue - voffset); });
    num_field->setValue(value + voffset);
    num_field->show();
  }

  if (has_focus) {
    act_field->focus();
  }
}

void GVarNumberEdit::setDisplayHandler(std::function<std::string(int value)> function)
{
  if (num_field) num_field->setDisplayHandler(function);
}

#if defined(SIMU) && defined(GVARS)
#include "mainwindow.h"

// Toggling GV mode on and back off without ever picking a GVAR must not
// clobber the raw value that was in effect beforehand: the shared
// raw/GVAR storage would otherwise silently reset to whatever GV1
// currently evaluates to (the data-loss bug this cache/restore fixes).
bool gvarNumberEditRawValueSurvivesGvRoundTripForTest()
{
  int32_t raw = 884;  // an off-grid raw value, well outside the GV-index range
  auto fld = new (std::nothrow) GVarNumberEdit(
      MainWindow::instance(), -1000, 1000, [&]() { return raw; },
      [&](int32_t v) { raw = v; }, PREC1);
  if (!fld) return false;

  bool ok = !GV_IS_GV_VALUE(raw) && raw == 884;

  // Toggle into GV mode: the field auto-selects GV1, so the shared
  // storage now holds a GVAR reference rather than the raw value.
  fld->switchGVarMode();
  ok = ok && GV_IS_GV_VALUE(raw);

  // Toggle straight back out, never touching the GV dropdown: the raw
  // value must reappear bit-for-bit.
  fld->switchGVarMode();
  ok = ok && !GV_IS_GV_VALUE(raw) && raw == 884;

  delete fld;
  return ok;
}

// A genuinely chosen GVAR binding must keep working as before: switching
// back to raw mode should derive the raw value from the picked GVAR (the
// pre-existing conversion), not resurrect the stale pre-GV raw value.
bool gvarNumberEditGenuineGvPickIsNotClobberedForTest()
{
  int32_t raw = 884;
  auto fld = new (std::nothrow) GVarNumberEdit(
      MainWindow::instance(), -1000, 1000, [&]() { return raw; },
      [&](int32_t v) { raw = v; }, PREC1);
  if (!fld || !fld->getGvarFieldForTest()) {
    delete fld;
    return false;
  }

  fld->switchGVarMode();
  bool ok = GV_IS_GV_VALUE(raw);

  // Simulate the user actually picking GV3 from the dropdown, exactly as
  // the menu selection handler would (via Choice::setValue()).
  fld->getGvarFieldForTest()->setValue(2);
  ok = ok && GV_IS_GV_VALUE(raw) && raw != GV_VALUE_FROM_INDEX(0);

  // Toggling back out now must NOT restore the stale pre-GV raw value —
  // that would silently discard the user's GV3 binding.
  fld->switchGVarMode();
  ok = ok && !GV_IS_GV_VALUE(raw) && raw != 884;

  delete fld;
  return ok;
}
#endif
