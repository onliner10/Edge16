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

#include "choice.h"

#include "mainwindow.h"
#include "menu.h"
#include "etx_lv_theme.h"

// Choice
static void choice_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_TINY);
  lv_obj_set_style_pad_hor(obj, PAD_MEDIUM, LV_PART_MAIN);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static const lv_obj_class_t choice_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = choice_constructor,
    .destructor_cb = nullptr,
    .event_cb = nullptr,
    .user_data = nullptr,
    .width_def = LV_SIZE_CONTENT,
    .height_def = EdgeTxStyles::UI_ELEMENT_HEIGHT,
    .editable = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_obj_t),
};

static lv_obj_t* choice_create(lv_obj_t* parent)
{
  return etx_create(&choice_class, parent);
}

#if defined(SIMU)
static bool forceChoiceImageCreateFailureForTest = false;
static bool forceChoiceLabelCreateFailureForTest = false;
#endif

static lv_obj_t* choice_img_create(lv_obj_t* parent)
{
#if defined(SIMU)
  if (forceChoiceImageCreateFailureForTest) return nullptr;
#endif
  return lv_img_create(parent);
}

static lv_obj_t* choice_label_create(lv_obj_t* parent)
{
#if defined(SIMU)
  if (forceChoiceLabelCreateFailureForTest) return nullptr;
#endif
  return etx_label_create(parent);
}

ChoiceBase::ChoiceBase(Window* parent, const rect_t& rect,
                       int vmin, int vmax, const char* title,
                       std::function<int()> _getValue,
                       std::function<void(int)> _setValue,
                       ChoiceType type) :
    FormField(parent, rect, choice_create),
    vmin(vmin), vmax(vmax), menuTitle(title), type(type),
    _getValue(std::move(_getValue)),
    _setValue(std::move(_setValue))
{
  setAutomationRole("button");
  padLeft(PAD_TINY);
  padRight(PAD_SMALL);

  // Add image
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_obj_t* img = choice_img_create(obj);
    if (img) {
      lv_img_set_src(
          img, type == CHOICE_TYPE_DROPOWN ? LV_SYMBOL_DOWN : LV_SYMBOL_DIRECTORY);
      lv_obj_set_pos(img, 0, PAD_TINY);
    }
  });

  // Add label
  initRequiredLvObj(label, choice_label_create, [&](lv_obj_t* obj) {
    lv_obj_set_pos(obj, type == CHOICE_TYPE_DROPOWN ? ICON_W - 2 : ICON_W,
                   PAD_TINY);
    etx_font(obj, FONT_XS_INDEX, LV_STATE_USER_1);
  });
}

#if defined(SIMU)
std::string ChoiceBase::automationText() const
{
  std::string text;
  label.with([&](lv_obj_t* obj) {
    const char* value = lv_label_get_text(obj);
    if (value) text = value;
  });
  return text;
}
#endif

void ChoiceBase::onLiveCheckEvents(Window::LiveWindow& live)
{
  update(live);
  Window::onLiveCheckEvents(live);
}

std::string Choice::getLabelText()
{
  std::string text;

  if (currentValue != INT_MAX) {
    int val = currentValue;
    if (textHandler) {
      text = textHandler(val);
    } else {
      val -= vmin;
      if (val >= 0 && val < (int)values.size()) {
        text = values[val];
      } else {
        text = std::to_string(val + vmin);
      }
    }
  }

  return text;
}

void ChoiceBase::update()
{
  withLive([&](LiveWindow& live) {
    update(live);
  });
}

void ChoiceBase::update(Window::LiveWindow&)
{
  if (!_getValue) return;

  int v = _getValue();
  if (v != currentValue) {
    currentValue = v;
    std::string s = getLabelText();
    label.with([&](lv_obj_t* obj) {
      if (width() > 0) {
        int w = width() - (type == CHOICE_TYPE_DROPOWN ? ICON_W - 2 : ICON_W) -
                PAD_TINY * 3;
        if (getTextWidth(s.c_str(), 0, FONT(STD)) > w)
          lv_obj_add_state(obj, LV_STATE_USER_1);
        else
          lv_obj_clear_state(obj, LV_STATE_USER_1);
      }
      lv_label_set_text(obj, s.c_str());
    });
  }
}

Choice::Choice(Window* parent, const rect_t& rect, int vmin, int vmax,
               std::function<int()> getValue,
               std::function<void(int)> setValue, const char* title, ChoiceType type) :
    ChoiceBase(parent, rect, vmin, vmax, title, getValue, setValue, type)
{
  update();
}

Choice::Choice(Window* parent, const rect_t& rect, const char* const values[],
               int vmin, int vmax, std::function<int()> getValue,
               std::function<void(int)> setValue, const char* title) :
    ChoiceBase(parent, rect, vmin, vmax, title, getValue, setValue, CHOICE_TYPE_DROPOWN)
{
  setValues(values);
}

Choice::Choice(Window* parent, const rect_t& rect,
               std::vector<std::string> values, int vmin, int vmax,
               std::function<int()> getValue,
               std::function<void(int)> setValue, const char* title) :
    ChoiceBase(parent, rect, vmin, vmax, title, getValue, setValue, CHOICE_TYPE_DROPOWN)
{
  setValues(values);
}

void Choice::addValue(const char* value)
{
  values.emplace_back(value);
  vmax += 1;
}

void Choice::setValues(std::vector<std::string> values)
{
  this->values.clear();
  this->values = std::move(values);
  currentValue = INT_MAX; // Force update
  update();
}

void Choice::setValues(const char* const values[])
{
  this->values.clear();
  if (values) {
    auto value = &values[0];
    for (int i = vmin; i <= vmax; i++) {
      this->values.emplace_back(*value++);
    }
  }
  currentValue = INT_MAX; // Force update
  update();
}

void Choice::setValue(int val)
{
  if (_setValue) {
    if (recentList) recentList->touch(val);
    _setValue(val);
    update();
  }
}

void Choice::onLiveClicked(Window::LiveWindow&)
{
  openMenu();
}

void Choice::fillMenu(Menu* menu, const FilterFct& filter)
{
  if (menu->count() > 0)
    menu->removeLines();
  auto value = getIntValue();

  int count = 0;
  int selectedIx = -1;
  selectedIx0 = -1;

  // Values already emitted in the pinned MRU block below, so the full-list
  // loop can skip them - a value pinned at the top must not also appear a
  // second time in its natural position.
  int pinnedValues[MRUList::CAPACITY];
  int pinnedCount = 0;

  // Most-recently-used boost (opt-in): pin recent choices at the top, visually
  // separated by a divider. The full list below skips any value pinned here,
  // so each value has exactly one row; selection resolves to whichever row
  // (pinned or natural) ends up on screen.
  if (recentList) {
    int recentsAdded = 0;
    for (uint8_t r = 0; r < recentList->size(); ++r) {
      int i = (*recentList)[r];
      if (i < vmin || i > vmax) continue;
      if (filter && !filter(i)) continue;
      if (isValueAvailable && !isValueAvailable(inverted ? -i : i)) continue;
      if (textHandler) {
        menu->addLineBuffered(textHandler(i), [=]() { setValue(i); });
      } else if (unsigned(i - vmin) < values.size()) {
        menu->addLineBuffered(values[i - vmin], [=]() { setValue(i); });
      } else {
        menu->addLineBuffered(std::to_string(i), [=]() { setValue(i); });
      }
      // Highlight the pinned copy of the current value so the menu opens showing
      // the recents at the top rather than scrolled to the full-list copy.
      if (value == i && selectedIx < 0) selectedIx = count;
      if (i == 0 && selectedIx0 < 0) selectedIx0 = count;
      pinnedValues[pinnedCount++] = i;
      ++recentsAdded;
      ++count;
    }
    if (recentsAdded > 0) {
      menu->addLineBuffered(MRU_DIVIDER_TEXT, nullptr);
      ++count;
    }
  }

  for (int i = vmin; i <= vmax; ++i) {
    if (filter && !filter(i)) continue;
    if (isValueAvailable && !isValueAvailable(inverted ? -i : i)) continue;
    bool alreadyPinned = false;
    for (int p = 0; p < pinnedCount; ++p) {
      if (pinnedValues[p] == i) {
        alreadyPinned = true;
        break;
      }
    }
    if (alreadyPinned) continue;
    if (textHandler) {
      menu->addLineBuffered(textHandler(i), [=]() { setValue(i); });
    } else if (unsigned(i - vmin) < values.size()) {
      menu->addLineBuffered(values[i - vmin], [=]() { setValue(i); });
    } else {
      menu->addLineBuffered(std::to_string(i), [=]() { setValue(i); });
    }
    if (value == i && selectedIx < 0) {
      selectedIx = count;
    }
    if (i == 0 && selectedIx0 < 0) {
      selectedIx0 = count;
    }
    ++count;
  }
  if (fillMenuHandler) {
    fillMenuHandler(menu, value, selectedIx);
  }
  menu->updateLines();
  // Force update - in case selected row is first row
  menu->select(-1);
  if (selectedIx >= 0)
    menu->select(selectedIx);
  else if (selectedIx0 >= 0)
    menu->select(selectedIx0);
  else {
    menu->select(0);
  }
}

void Choice::openMenu()
{
  setEditMode(true);  // this needs to be done first before menu is created.

  auto menu = new Menu(false, popupWidth);
  activeMenu = menu;
  if (menuTitle) menu->setTitle(menuTitle);

  fillMenu(menu);

  std::weak_ptr<bool> lifetime(lifetimeToken);
  auto choice = this;
  menu->setCloseHandler([choice, lifetime]() {
    auto alive = lifetime.lock();
    if (!alive) return;
    choice->activeMenu = nullptr;
    choice->setEditMode(false);
  });
}

void Choice::onDelete()
{
  lifetimeToken.reset();
  auto menu = activeMenu;
  activeMenu = nullptr;
  if (menu) {
    menu->setCloseHandler({});
    menu->setWaitHandler({});
    menu->setLongPressHandler({});
    menu->deleteLater();
  }
  ChoiceBase::onDelete();
}

#if defined(SIMU)
bool choiceImageCreateFailureLeavesChoiceUsableForTest()
{
  forceChoiceImageCreateFailureForTest = true;
  auto choice = new (std::nothrow) Choice(
      MainWindow::instance(), rect_t{0, 0, 100, EdgeTxStyles::UI_ELEMENT_HEIGHT},
      0, 1, [] { return 0; });
  forceChoiceImageCreateFailureForTest = false;

  bool ok = choice && choice->getLvObjForTest() != nullptr && choice->isVisible();
  delete choice;
  return ok;
}

bool choiceLabelCreateFailureFailsClosedForTest()
{
  forceChoiceLabelCreateFailureForTest = true;
  auto choice = new (std::nothrow) Choice(
      MainWindow::instance(), rect_t{0, 0, 100, EdgeTxStyles::UI_ELEMENT_HEIGHT},
      0, 1, [] { return 0; });
  forceChoiceLabelCreateFailureForTest = false;

  bool ok = choice && choice->getLvObjForTest() != nullptr &&
            !choice->isAvailable() && !choice->isVisible() &&
            !choice->automationClickable();
  delete choice;
  return ok;
}

// Item 13: a value pinned in the MRU block must not also be emitted a
// second time in its natural position further down the list.
bool choiceFillMenuSkipsPinnedValuesInFullListForTest()
{
  static const char* const values[] = {"Zero", "One", "Two", "Three", "Four"};

  int current = 2;  // "Two" - deliberately NOT one of the recent values
  MRUList recent;
  recent.touch(1);  // "One"
  recent.touch(3);  // "Three", most recent -> pinned as [Three, One]

  auto choice = new (std::nothrow) Choice(
      MainWindow::instance(), rect_t{0, 0, 100, EdgeTxStyles::UI_ELEMENT_HEIGHT},
      values, 0, 4, [&]() { return current; }, [&](int v) { current = v; });
  if (!choice) return false;
  choice->setRecentList(&recent);

  auto menu = new (std::nothrow) Menu();
  if (!menu) {
    delete choice;
    return false;
  }

  choice->fillMenu(menu);

  // 2 pinned rows ("Three", "One") + 1 divider + the 5 full-list rows minus
  // the 2 already pinned (3 left: "Zero", "Two", "Four") = 6 rows total,
  // each value appearing exactly once.
  bool countOk = menu->count() == 6;

  int oneCount = 0, threeCount = 0;
  for (unsigned i = 0; i < menu->count(); ++i) {
    std::string text = menu->lineTextForTest(i);
    if (text == "One") ++oneCount;
    if (text == "Three") ++threeCount;
  }
  bool noDuplicates = oneCount == 1 && threeCount == 1;

  // The current value ("Two") isn't pinned, so it must still resolve to a
  // single, correctly-highlighted row down in the full list.
  int sel = menu->selection();
  bool selectionOk =
      sel >= 0 && menu->lineTextForTest((unsigned)sel) == "Two";

  delete choice;
  return countOk && noDuplicates && selectionOk;
}
#endif
