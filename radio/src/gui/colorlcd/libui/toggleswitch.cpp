/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
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

#include "toggleswitch.h"

#include "etx_lv_theme.h"
#include "mainwindow.h"

static void switch_apply_edge_style(lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_ZERO);
  etx_obj_add_style(obj, styles->circle, LV_PART_MAIN);
  etx_bg_color(obj, COLOR_THEME_SECONDARY3_INDEX, LV_PART_MAIN);

  etx_obj_add_style(obj, styles->circle, LV_PART_INDICATOR);
  etx_bg_color(obj, COLOR_THEME_ACTIVE_INDEX,
               LV_PART_INDICATOR | LV_STATE_CHECKED);

  etx_obj_add_style(obj, styles->bg_opacity_cover, LV_PART_KNOB);
  etx_obj_add_style(obj, styles->circle, LV_PART_KNOB);
  etx_bg_color(obj, COLOR_THEME_SECONDARY1_INDEX, LV_PART_KNOB);

  etx_obj_add_style(obj, styles->disabled, LV_PART_KNOB | LV_STATE_DISABLED);
}

static lv_obj_t* switch_create(lv_obj_t* parent)
{
  auto obj = etx_create(&lv_switch_class, parent);
  if (!obj) return nullptr;
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_size(obj, ToggleSwitch::TOGGLE_W, EdgeTxStyles::UI_ELEMENT_HEIGHT);
  lv_switch_set_orientation(obj, LV_SWITCH_ORIENTATION_HORIZONTAL);
  switch_apply_edge_style(obj);
  return obj;
}

ToggleSwitch::ToggleSwitch(Window* parent, const rect_t& rect,
                           std::function<uint8_t()> getValue,
                           std::function<void(uint8_t)> setValue) :
    FormField(parent, rect, switch_create),
    _getValue(std::move(getValue)),
    _setValue(std::move(setValue))
{
  lv_subject_init_int(&checkedSubject, _getValue && _getValue() ? 1 : 0);
  checkedSubjectInitialized = true;

#if defined(SIMU)
  setAutomationRole("button");
#endif

  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CHECKABLE);
    if (rect.w == 0) lv_obj_set_width(obj, TOGGLE_W);
    if (rect.h == 0) lv_obj_set_height(obj, EdgeTxStyles::UI_ELEMENT_HEIGHT);
    auto binding = lv_obj_bind_checked(obj, &checkedSubject);
    auto observer = lv_subject_add_observer_obj(
        &checkedSubject, ToggleSwitch::checkedSubjectChanged, obj, this);
    lv_obj_add_event_cb(obj, ToggleSwitch::checkedValueChanged,
                        LV_EVENT_VALUE_CHANGED, this);
    if (!binding || !observer) failClosed();
  });
}

ToggleSwitch::~ToggleSwitch()
{
  if (checkedSubjectInitialized) {
    lv_subject_deinit(&checkedSubject);
    checkedSubjectInitialized = false;
  }
}

void ToggleSwitch::update() const
{
  if (!_getValue) return;
  lv_subject_set_int(const_cast<lv_subject_t*>(&checkedSubject),
                     _getValue() ? 1 : 0);
}

void ToggleSwitch::onLiveClicked(Window::LiveWindow& live)
{
  if (lvglValueChanged) {
    lvglValueChanged = false;
    return;
  }

  const bool modelValue = _getValue && _getValue();
  lv_subject_set_int(&checkedSubject, modelValue ? 0 : 1);
}

void ToggleSwitch::onLiveCheckEvents(Window::LiveWindow& live)
{
  Window::onLiveCheckEvents(live);
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    if (!_getValue) return;
    bool v = _getValue() != 0;
    bool s = (lv_obj_get_state(obj) & LV_STATE_CHECKED) == LV_STATE_CHECKED;
    if (v != s) update();
  });
}

void ToggleSwitch::checkedSubjectChanged(lv_observer_t* observer,
                                         lv_subject_t* subject)
{
  withAvailableObserver<ToggleSwitch>(observer, [&](ToggleSwitch& self) {
    self.setValue(lv_subject_get_int(subject) != 0);
  });
}

void ToggleSwitch::checkedValueChanged(lv_event_t* e)
{
  auto self = static_cast<ToggleSwitch*>(lv_event_get_user_data(e));
  if (self) self->lvglValueChanged = true;
}

#if defined(SIMU)
void etxCreateForceObjectAllocationFailureForTest(bool force);

bool toggleSwitchObjectAllocationFailureFailsClosedForTest()
{
  class TestToggleSwitch : public ToggleSwitch
  {
   public:
    TestToggleSwitch(Window* parent, std::function<uint8_t()> getValue,
                     std::function<void(uint8_t)> setValue) :
        ToggleSwitch(
            parent,
            {0, 0, ToggleSwitch::TOGGLE_W, EdgeTxStyles::UI_ELEMENT_HEIGHT},
            std::move(getValue), std::move(setValue))
    {
    }

    void exerciseCheckEvents() { checkEvents(); }
  };

  bool changed = false;
  etxCreateForceObjectAllocationFailureForTest(true);
  auto toggle = new (std::nothrow) TestToggleSwitch(
      MainWindow::instance(), []() { return 1; },
      [&](uint8_t) { changed = true; });
  etxCreateForceObjectAllocationFailureForTest(false);

  if (!toggle) return false;

  toggle->update();
  toggle->setValue(0);
  toggle->setGetValueHandler(nullptr);
  toggle->setSetValueHandler(nullptr);
  (void)toggle->getValue();
  toggle->exerciseCheckEvents();

  bool ok = !toggle->isAvailable() && !toggle->isVisible() && !changed;
  delete toggle;
  return ok;
}
#endif
