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

#include "list_line_button.h"

#include <algorithm>

#include "LvglWrapper.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "mainwindow.h"
#include "lvgl/src/core/lv_obj_class_private.h"
#include "lvgl/src/widgets/button/lv_button_private.h"

#if defined(SIMU)
static bool forceFmBufferMallocFailure = false;
static bool forceListLineLabelCreateFailure = false;

void listLineButtonForceFmBufferMallocFailureForTest(bool force)
{
  forceFmBufferMallocFailure = force;
}

static void listLineButtonForceLabelCreateFailureForTest(bool force)
{
  forceListLineLabelCreateFailure = force;
}
#endif

static lv_obj_t* list_line_label_create(lv_obj_t* parent)
{
#if defined(SIMU)
  if (forceListLineLabelCreateFailure) return nullptr;
#endif
  return etx_label_create(parent);
}

static void input_mix_line_constructor(const lv_obj_class_t* class_p,
                                       lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_TINY);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static const lv_obj_class_t input_mix_line_class = {
    .base_class = &lv_button_class,
    .constructor_cb = input_mix_line_constructor,
    .destructor_cb = nullptr,
    .event_cb = nullptr,
    .user_data = nullptr,
    .width_def = ListLineButton::GRP_W,
    .height_def = ListLineButton::BTN_H,
    .editable = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_button_t),
};

static lv_obj_t* input_mix_line_create(lv_obj_t* parent)
{
  return etx_create(&input_mix_line_class, parent);
}

ListLineButton::ListLineButton(Window* parent, uint8_t index) :
    ButtonBase(parent, rect_t{}, nullptr, input_mix_line_create), index(index)
{
  setWindowFlag(NO_SCROLL);
}

void ListLineButton::onLiveCheckEvents(Window::LiveWindow& live)
{
  LineRealizationToken token;
  if (!tryRealize(token, live)) return;

  Window::onLiveCheckEvents(live);
  applyRefreshIfReady();
  if (checkHandler) checkHandler();
}

void ListLineButton::delayedInit()
{
  withLive([&](LiveWindow& live) {
    LineRealizationToken token;
    tryRealize(token, live);
  });
}

void ListLineButton::onDelete()
{
  setLiveValueUpdatesEnabled(false);
}

void ListLineButton::onFailClosed()
{
  transitionToFailedClosed();
}

void ListLineButton::onLiveVisibilityChanged(Window::LiveWindow&, bool visible)
{
  setLiveValueUpdatesEnabled(visible);
}

void ListLineButton::setLiveValueUpdatesEnabled(bool enabled)
{
  if (!enabled) {
    liveConnection.disconnect();
    liveSubscription.reset();
    return;
  }

  if (!isLineReady() || liveConnection.connected()) return;

  liveSubscription = UiEventHub::registerLiveValueConsumer();
  liveConnection = UiEventHub::subscribe(
      UiTopic::LiveChannelValues,
      [this](uint32_t) { runLiveValueUpdate(); });
}

void ListLineButton::runLiveValueUpdate()
{
  if (!isLineReady()) return;

  withLive([&](LiveWindow& live) {
    if (!isLiveOnScreen(live)) return;
    Window::onLiveCheckEvents(live);
    applyRefreshIfReady();
    if (checkHandler) checkHandler();
    check(isActive());
    updatePhase = UpdatePhase::LiveUpdating;
    onLineLiveUpdate(live);
    updatePhase = UpdatePhase::Idle;
  });
}

void ListLineButton::refresh()
{
  refreshPending = true;
  if (updatePhase == UpdatePhase::Idle) applyRefreshIfReady();
}

void ListLineButton::onLineLiveUpdate(Window::LiveWindow& live)
{
  onLoadedCheckEvents(live);
}

bool ListLineButton::isLineReady() const
{
  return lineState == LineState::Ready;
}

bool ListLineButton::tryRealize(LineRealizationToken&,
                                Window::LiveWindow& live)
{
  if (transitionToFailedClosedIfUnavailable()) return false;
  if (lineState == LineState::Ready) return true;
  if (lineState == LineState::FailedClosed || lineState == LineState::Loading)
    return false;

  lineState = LineState::Loading;
  onLineLoaded();

  if (transitionToFailedClosedIfUnavailable()) return false;

  lineState = LineState::Ready;
  refreshPending = true;
  applyRefreshIfReady();
  setLiveValueUpdatesEnabled(isLiveOnScreen(live));
  lv_obj_invalidate(live.lvobj());
  return true;
}

void ListLineButton::transitionToFailedClosed()
{
  lineState = LineState::FailedClosed;
  setLiveValueUpdatesEnabled(false);
}

bool ListLineButton::transitionToFailedClosedIfUnavailable()
{
  if (isAvailable()) return false;

  transitionToFailedClosed();
  return true;
}

void ListLineButton::applyRefreshIfReady()
{
  if (!refreshPending) return;
  if (!isLineReady()) return;
  if (updatePhase != UpdatePhase::Idle) return;

  withLive([&](LiveWindow& live) {
    updatePhase = UpdatePhase::Refreshing;
    refreshPending = false;
    check(isActive());
    onRefresh();
    if (transitionToFailedClosedIfUnavailable()) return;
    onLineAfterRefresh();
    if (transitionToFailedClosedIfUnavailable()) return;
    updatePhase = UpdatePhase::Idle;
  });

  updatePhase = UpdatePhase::Idle;
}

void ListLineButton::onLiveClicked(Window::LiveWindow& live)
{
  LineRealizationToken token;
  if (!tryRealize(token, live)) return;

  ButtonBase::onLiveClicked(live);
}

bool ListLineButton::onLiveCustomEvent(Window::LiveWindow& live,
                                       lv_event_t* event)
{
  if (lv_event_get_code(event) == LV_EVENT_DRAW_MAIN_END && !isLineReady()) {
    drawPlaceholder(live, event);
  }
  return false;
}

static void draw_placeholder_bar(lv_layer_t* layer, lv_area_t area,
                                 lv_coord_t radius, lv_color_t color,
                                 lv_opa_t opa)
{
  if (area.x2 < area.x1 || area.y2 < area.y1) return;

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.radius = radius;
  dsc.bg_color = color;
  dsc.bg_opa = opa;
  lv_draw_rect(layer, &dsc, &area);
}

void ListLineButton::drawPlaceholder(Window::LiveWindow& live,
                                     lv_event_t* event)
{
  if (lineState == LineState::FailedClosed) return;

  lv_layer_t* layer = lv_event_get_layer(event);
  if (!layer) return;

  lv_area_t row;
  lv_obj_get_coords(live.lvobj(), &row);
  const lv_coord_t rowW = lv_area_get_width(&row);
  const lv_coord_t rowH = lv_area_get_height(&row);
  if (rowW <= PAD_LARGE * 2 || rowH <= PAD_SMALL * 2) return;

  const lv_coord_t inset = PAD_MEDIUM;
  const lv_coord_t barH = std::max<lv_coord_t>(
      4, std::min<lv_coord_t>(8, rowH / 5));
  const lv_coord_t gap = std::max<lv_coord_t>(3, barH / 2);
  const lv_coord_t blockH = barH * 2 + gap;
  const lv_coord_t y = row.y1 + std::max<lv_coord_t>(PAD_TINY,
                                                     (rowH - blockH) / 2);

  const lv_color_t color = makeLvColor(COLOR_THEME_SECONDARY2);
  const lv_opa_t primaryOpa = LV_OPA_50;
  const lv_opa_t secondaryOpa = LV_OPA_30;
  const lv_coord_t radius = barH / 2;

  lv_area_t left = {
      row.x1 + inset,
      y,
      row.x1 + inset + std::min<lv_coord_t>(rowW / 5, 64),
      y + barH - 1,
  };
  draw_placeholder_bar(layer, left, radius, color, primaryOpa);

  lv_area_t main = {
      left.x2 + PAD_LARGE,
      y,
      row.x1 + inset + std::min<lv_coord_t>((rowW * 3) / 5, rowW - 80),
      y + barH - 1,
  };
  draw_placeholder_bar(layer, main, radius, color, primaryOpa);

  lv_area_t detail = {
      row.x1 + inset,
      y + barH + gap,
      row.x1 + inset + std::min<lv_coord_t>(rowW / 3, 120),
      y + barH * 2 + gap - 1,
  };
  draw_placeholder_bar(layer, detail, radius, color, secondaryOpa);

  const lv_coord_t affordance = std::min<lv_coord_t>(barH * 2, 18);
  lv_area_t right = {
      row.x2 - inset - affordance,
      row.y1 + (rowH - affordance) / 2,
      row.x2 - inset,
      row.y1 + (rowH + affordance) / 2,
  };
  draw_placeholder_bar(layer, right, PAD_TINY, color, secondaryOpa);
}

InputMixButtonBase::InputMixButtonBase(Window* parent, uint8_t index) :
    ListLineButton(parent, index)
{
  setWidth(BTN_W);
  setHeight(ListLineButton::BTN_H);
  padAll(PAD_ZERO);
}

InputMixButtonBase::~InputMixButtonBase()
{
  if (fm_buffer) free(fm_buffer);
}

static void setLineLabelText(RequiredLvObj& label, const char* text,
                             coord_t width)
{
  label.with([&](lv_obj_t* obj) {
    if (getTextWidth(text, 0, FONT(STD)) > width)
      lv_obj_add_state(obj, LV_STATE_USER_1);
    else
      lv_obj_clear_state(obj, LV_STATE_USER_1);
    lv_label_set_text(obj, text);
  });
}

bool InputMixButtonBase::ensureLineLabel(RequiredLvObj& label, coord_t x,
                                         coord_t y, coord_t w, coord_t h)
{
  if (label.isPresent()) return true;

  return initRequiredLvObj(label, list_line_label_create, [&](lv_obj_t* obj) {
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    etx_font(obj, FONT_XS_INDEX, LV_STATE_USER_1);
  });
}

void InputMixButtonBase::setWeight(gvar_t value, gvar_t min, gvar_t max)
{
  if (!isLineReady()) return;
  if (!ensureLineLabel(weight, WGT_X, WGT_Y, WGT_W, WGT_H)) return;

  char s[32];
  getValueOrSrcVarString(s, sizeof(s), value, 0, "%");
  setLineLabelText(weight, s, WGT_W);
}

void InputMixButtonBase::setSource(mixsrc_t idx)
{
  if (!isLineReady()) return;
  if (!ensureLineLabel(source, SRC_X, SRC_Y, SRC_W, SRC_H)) return;

  char* s = getSourceString(idx);
  setLineLabelText(source, s, SRC_W);
}

void InputMixButtonBase::setOpts(const char* s)
{
  if (!isLineReady()) return;
  if (!ensureLineLabel(opts, OPT_X, OPT_Y, OPT_W, OPT_H)) return;

  setLineLabelText(opts, s, OPT_W);
}

void InputMixButtonBase::setFlightModes(uint16_t modes)
{
  if (!isLineReady()) return;

  bool handled = withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();

    if (!modelFMEnabled()) return;
    if (modes == fm_modes) return;
    fm_modes = modes;

    if (!fm_modes) {
      if (!fm_canvas) return;
      lv_obj_del(fm_canvas);
      free(fm_buffer);
      fm_canvas = nullptr;
      fm_buffer = nullptr;
      updateHeight();
      return;
    }

    if (!fm_canvas) {
      auto newCanvas = lv_canvas_create(obj);
      if (!newCanvas) {
        fm_modes = 0;
        updateHeight();
        return;
      }

#if defined(SIMU)
      auto newBuffer = forceFmBufferMallocFailure
                           ? nullptr
                           : malloc(FM_CANVAS_WIDTH * FM_CANVAS_HEIGHT);
#else
      auto newBuffer = malloc(FM_CANVAS_WIDTH * FM_CANVAS_HEIGHT);
#endif
      if (!newBuffer) {
        lv_obj_del(newCanvas);
        fm_modes = 0;
        updateHeight();
        return;
      }

      fm_canvas = newCanvas;
      fm_buffer = newBuffer;
      lv_canvas_set_buffer(fm_canvas, fm_buffer, FM_CANVAS_WIDTH,
                           FM_CANVAS_HEIGHT, LV_COLOR_FORMAT_A8);
      lv_obj_set_pos(fm_canvas, FM_X, FM_Y);

      lv_obj_set_style_img_recolor(fm_canvas, makeLvColor(COLOR_THEME_SECONDARY1), LV_PART_MAIN);
      lv_obj_set_style_img_recolor_opa(fm_canvas, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_img_recolor(fm_canvas, makeLvColor(COLOR_THEME_PRIMARY1), LV_PART_MAIN | LV_STATE_CHECKED);
      lv_obj_set_style_img_recolor_opa(fm_canvas, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    }

    lv_canvas_fill_bg(fm_canvas, lv_color_black(), LV_OPA_TRANSP);

    const MaskBitmap* mask = getBuiltinIcon(ICON_TEXTLINE_FM);
    lv_coord_t w = mask->width;
    lv_coord_t h = mask->height;

    coord_t x = 0;
    {
      lv_draw_buf_t src_buf;
      if (lv_draw_buf_init(&src_buf, w, h, LV_COLOR_FORMAT_A8, 0,
                           (void*)mask->data, w * h) == LV_RESULT_OK) {
        lv_area_t canvas_area;
        canvas_area.x1 = x;
        canvas_area.y1 = 0;
        canvas_area.x2 = x + w - 1;
        canvas_area.y2 = h - 1;
        lv_canvas_copy_buf(fm_canvas, &canvas_area, &src_buf, NULL);
      }
    }
    x += (w + PAD_TINY);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;

    const lv_font_t* font = getFont(FONT(XS));
    label_dsc.font = font;

    lv_layer_t canvas_layer;
    lv_canvas_init_layer(fm_canvas, &canvas_layer);
    for (int i = 0; i < MAX_FLIGHT_MODES; i++) {
      char s[] = " ";
      s[0] = '0' + i;
      if (fm_modes & (1 << i)) {
        label_dsc.color = lv_color_make(0x7f, 0x7f, 0x7f);
      } else {
        lv_area_t rect_area;
        rect_area.x1 = x;
        rect_area.y1 = 0;
        rect_area.x2 = x + FM_W - 1;
        rect_area.y2 = PAD_THREE - 1;
        lv_draw_rect(&canvas_layer, &rect_dsc, &rect_area);
        label_dsc.color = lv_color_white();
      }
      lv_area_t text_area;
      text_area.x1 = x;
      text_area.y1 = 0;
      text_area.x2 = x + FM_W - 1;
      text_area.y2 = PAD_THREE - 1;
      label_dsc.text = s;
      lv_draw_label(&canvas_layer, &label_dsc, &text_area);
      x += FM_W;
    }
    lv_canvas_finish_layer(fm_canvas, &canvas_layer);

    updateHeight();
  });
  if (!handled) {
    fm_modes = 0;
    return;
  }
}

#if defined(SIMU)
bool listLineButtonMissingFmBufferLeavesNoCanvasForTest()
{
  class TestInputMixButton : public InputMixButtonBase
  {
   public:
    TestInputMixButton(Window* parent) : InputMixButtonBase(parent, 0) {}

    void onRefresh() override {}
    void updatePos(coord_t, coord_t) override {}
    void forceFirstLoadForTest() { delayedInit(); }

    bool hasFlightModeCanvas() const { return fm_canvas != nullptr; }
    bool hasFlightModeBuffer() const { return fm_buffer != nullptr; }

   protected:
    bool isActive() const override { return false; }
  };

  g_eeGeneral.modelFMDisabled = 0;
  g_model.modelFMDisabled = OVERRIDE_ON;

  auto button = new TestInputMixButton(MainWindow::instance());
  button->forceFirstLoadForTest();
  listLineButtonForceFmBufferMallocFailureForTest(true);
  button->setFlightModes(1);
  listLineButtonForceFmBufferMallocFailureForTest(false);

  return !button->hasFlightModeCanvas() && !button->hasFlightModeBuffer();
}

bool listLineButtonLabelAllocationFailureFailsClosedForTest()
{
  class TestInputMixButton : public InputMixButtonBase
  {
   public:
    TestInputMixButton(Window* parent) : InputMixButtonBase(parent, 0) {}

    void onRefresh() override {}
    void updatePos(coord_t, coord_t) override {}
    void forceFirstLoadForTest() { delayedInit(); }

   protected:
    bool isActive() const override { return false; }
  };

  auto button = new (std::nothrow) TestInputMixButton(MainWindow::instance());
  if (!button || !button->isAvailable()) {
    delete button;
    return false;
  }

  button->forceFirstLoadForTest();
  listLineButtonForceLabelCreateFailureForTest(true);
  button->setWeight(100, -100, 100);
  listLineButtonForceLabelCreateFailureForTest(false);

  button->setSource(MIXSRC_FIRST_INPUT);
  button->setOpts("Opt");
  button->setFlightModes(1);
  button->checkEvents();

  bool ok = !button->isAvailable() && !button->isVisible() &&
            !button->automationClickable();
  delete button;
  return ok;
}

bool listLineGroupLabelAllocationFailureFailsClosedForTest()
{
  class TestInputMixGroup : public InputMixGroupBase
  {
   public:
    explicit TestInputMixGroup(Window* parent) :
        InputMixGroupBase(parent, MIXSRC_FIRST_INPUT)
    {
    }
  };

  listLineButtonForceLabelCreateFailureForTest(true);
  auto group = new (std::nothrow) TestInputMixGroup(MainWindow::instance());
  listLineButtonForceLabelCreateFailureForTest(false);

  if (!group) return false;
  group->refresh();
  group->adjustHeight();

  bool ok = !group->isAvailable() && !group->isVisible();
  delete group;
  return ok;
}

bool listLinePageLookupRequiresGroupAndLineForTest()
{
  static const PageDef testPageDef = {
      ICON_MODEL_INPUTS, STR_DEF(STR_QM_INPUTS), STR_DEF(STR_MENUINPUTS),
      PAGE_CREATE, QM_MODEL_INPUTS, nullptr};

  class TestInputMixButton : public InputMixButtonBase
  {
   public:
    TestInputMixButton(Window* parent, uint8_t index) :
        InputMixButtonBase(parent, index)
    {
    }

    void onRefresh() override {}
    void updatePos(coord_t, coord_t) override {}

   protected:
    bool isActive() const override { return false; }
  };

  class TestInputMixGroup : public InputMixGroupBase
  {
   public:
    explicit TestInputMixGroup(Window* parent) :
        InputMixGroupBase(parent, MIXSRC_FIRST_INPUT)
    {
    }
  };

  class TestInputMixPage : public InputMixPageBase
  {
   public:
    TestInputMixPage() : InputMixPageBase(testPageDef) {}

    void build(Window*) override {}

    void setGroup(InputMixGroupBase* group)
    {
      groups.clear();
      groupForIndex = group;
      if (group) groups.emplace_back(group);
    }

    void setLine(InputMixButtonBase* line)
    {
      lines.clear();
      if (line) lines.emplace_back(line);
    }

    bool visitGroupAndLine(uint8_t index, int& calls,
                           InputMixGroupBase*& seenGroup,
                           InputMixButtonBase*& seenLine)
    {
      return withGroupAndLineByIndex(
          index, [&](InputMixGroupBase& group, InputMixButtonBase& line) {
            calls += 1;
            seenGroup = &group;
            seenLine = &line;
          });
    }

   protected:
    InputMixButtonBase* createLineButton(InputMixGroupBase*, uint8_t) override
    {
      return nullptr;
    }

    InputMixGroupBase* createGroup(Window*, mixsrc_t) override
    {
      return nullptr;
    }

    InputMixGroupBase* getGroupByIndex(uint8_t index) override
    {
      return index == expectedIndex ? groupForIndex : nullptr;
    }

   private:
    uint8_t expectedIndex = 3;
    InputMixGroupBase* groupForIndex = nullptr;
  };

  auto group = new (std::nothrow) TestInputMixGroup(MainWindow::instance());
  if (!group || !group->isAvailable()) {
    delete group;
    return false;
  }

  auto line = new (std::nothrow) TestInputMixButton(group, 3);
  if (!line || !line->isAvailable()) {
    delete group;
    return false;
  }
  group->addLine(line);

  TestInputMixPage page;
  int calls = 0;
  InputMixGroupBase* seenGroup = nullptr;
  InputMixButtonBase* seenLine = nullptr;

  page.setGroup(group);
  if (page.visitGroupAndLine(3, calls, seenGroup, seenLine)) {
    delete group;
    return false;
  }
  if (calls != 0) {
    delete group;
    return false;
  }

  page.setLine(line);
  if (!page.visitGroupAndLine(3, calls, seenGroup, seenLine)) {
    delete group;
    return false;
  }
  if (calls != 1 || seenGroup != group || seenLine != line) {
    delete group;
    return false;
  }

  page.setGroup(nullptr);
  if (page.visitGroupAndLine(3, calls, seenGroup, seenLine)) {
    delete group;
    return false;
  }
  if (calls != 1) {
    delete group;
    return false;
  }

  delete group;
  return true;
}

bool listLineButtonRefreshBeforeVisibleLoadRunsOnFirstLoadForTest()
{
  class TestLineButton : public ListLineButton
  {
   public:
    explicit TestLineButton(Window* parent) : ListLineButton(parent, 0) {}

    int loadCalls = 0;
    int refreshCalls = 0;

    void forceFirstLoadForTest()
    {
      delayedInit();
    }

   protected:
    bool isActive() const override { return false; }
    void onLineLoaded() override { loadCalls += 1; }
    void onRefresh() override { refreshCalls += 1; }
  };

  auto mainWindow = MainWindow::instance();
  mainWindow->loadLvglScreen();

  auto button = new (std::nothrow) TestLineButton(mainWindow);
  if (!button || !button->isAvailable()) {
    delete button;
    return false;
  }

  button->refresh();
  if (button->refreshCalls != 0 || button->loadCalls != 0) {
    button->deleteLater();
    mainWindow->runMainLoopTick();
    mainWindow->runMainLoopTick();
    return false;
  }

  button->forceFirstLoadForTest();
  const bool firstLoadApplied =
      button->loadCalls == 1 && button->refreshCalls == 1;

  button->refresh();
  const bool loadedRefreshRuns = button->refreshCalls == 2;

  button->deleteLater();
  mainWindow->runMainLoopTick();
  mainWindow->runMainLoopTick();

  return firstLoadApplied && loadedRefreshRuns;
}

bool listLineButtonRefreshFromLiveUpdateDefersForTest()
{
  class TestLineButton : public ListLineButton
  {
   public:
    explicit TestLineButton(Window* parent) : ListLineButton(parent, 0) {}

    int refreshCalls = 0;
    int liveUpdateCalls = 0;

    void forceFirstLoadForTest() { delayedInit(); }
    void forceLiveUpdateForTest() { runLiveValueUpdate(); }

   protected:
    bool isActive() const override { return false; }
    void onRefresh() override { refreshCalls += 1; }

    void onLineLiveUpdate(LiveWindow&) override
    {
      liveUpdateCalls += 1;
      refresh();
    }
  };

  auto mainWindow = MainWindow::instance();
  mainWindow->loadLvglScreen();

  auto button = new (std::nothrow) TestLineButton(mainWindow);
  if (!button || !button->isAvailable()) {
    delete button;
    return false;
  }

  button->forceFirstLoadForTest();
  if (button->refreshCalls != 1 || button->liveUpdateCalls != 0) {
    button->deleteLater();
    mainWindow->runMainLoopTick();
    mainWindow->runMainLoopTick();
    return false;
  }

  button->forceLiveUpdateForTest();
  const bool refreshDeferred =
      button->refreshCalls == 1 && button->liveUpdateCalls == 1;

  button->forceLiveUpdateForTest();
  const bool deferredRefreshAppliedOnce =
      button->refreshCalls == 2 && button->liveUpdateCalls == 2;

  button->deleteLater();
  mainWindow->runMainLoopTick();
  mainWindow->runMainLoopTick();

  return refreshDeferred && deferredRefreshAppliedOnce;
}
#endif

void InputMixButtonBase::onLoadedCheckEvents(Window::LiveWindow& live)
{
  if (fm_canvas) {
    bool chkd = lv_obj_get_state(fm_canvas) & LV_STATE_CHECKED;
    if (chkd != this->checked()) {
      if (chkd)
        lv_obj_clear_state(fm_canvas, LV_STATE_CHECKED);
      else
        lv_obj_add_state(fm_canvas, LV_STATE_CHECKED);
    }
  }
}

void InputMixButtonBase::updateHeight()
{
#if NARROW_LAYOUT
  coord_t h = ListLineButton::BTN_H;
  if (fm_canvas)
    h += FM_CANVAS_HEIGHT + PAD_TINY;
  setHeight(h);
#endif
}

static void group_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_TINY);
}

static const lv_obj_class_t group_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = group_constructor,
    .destructor_cb = nullptr,
    .event_cb = nullptr,
    .user_data = nullptr,
    .width_def = ListLineButton::GRP_W,
    .height_def = LV_SIZE_CONTENT,
    .editable = LV_OBJ_CLASS_EDITABLE_FALSE,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_FALSE,
    .instance_size = sizeof(lv_obj_t),
};

static lv_obj_t* group_create(lv_obj_t* parent)
{
  return etx_create(&group_class, parent);
}

InputMixGroupBase::InputMixGroupBase(Window* parent, mixsrc_t idx) :
    Window(parent, rect_t{}, group_create), idx(idx)
{
  setWindowFlag(NO_FOCUS | NO_CLICK);
  initRequiredLvObj(label, list_line_label_create, [&](lv_obj_t* obj) {
    etx_font(obj, FONT_XS_INDEX, LV_STATE_USER_1);
  });
}

void InputMixGroupBase::_adjustHeight(coord_t y)
{
  if (getLineCount() == 0) setHeight(ListLineButton::BTN_H + PAD_SMALL * 2);

  for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
    auto line = *it;
    line->updateHeight();
    line->updatePos(InputMixButtonBase::LN_X, y);
    y += line->height() + PAD_OUTLINE;
  }
  setHeight(y + PAD_BORDER * 2 + PAD_OUTLINE);
}

void InputMixGroupBase::adjustHeight()
{
  _adjustHeight(0);
}

void InputMixGroupBase::addLine(InputMixButtonBase* line)
{
  if (!line || !line->withLive([](Window::LiveWindow&) { return true; }))
    return;

  auto l = std::find_if(lines.begin(), lines.end(),
                        [=](const InputMixButtonBase* l) -> bool {
                          return line->getIndex() <= l->getIndex();
                        });

  if (l != lines.end())
    lines.insert(l, line);
  else
    lines.emplace_back(line);

  adjustHeight();
}

bool InputMixGroupBase::removeLine(InputMixButtonBase* line)
{
  auto l = std::find_if(
      lines.begin(), lines.end(),
      [=](const InputMixButtonBase* l) -> bool { return l == line; });

  if (l != lines.end()) {
    lines.erase(l);
    adjustHeight();
    return true;
  }

  return false;
}

void InputMixGroupBase::refresh()
{
  char* s = getSourceString(idx);
  setLineLabelText(label, s, InputMixButtonBase::LN_X - PAD_TINY);
}

int InputMixGroupBase::getLineNumber(uint8_t index)
{
  int n = 0;
  auto l = std::find_if(lines.begin(), lines.end(), [&](InputMixButtonBase* l) {
    n += 1;
    return l->getIndex() == index;
  });

  if (l != lines.end()) return n;

  return -1;
}

InputMixGroupBase* InputMixPageBase::getGroupBySrc(mixsrc_t src)
{
  auto g = std::find_if(
      groups.begin(), groups.end(),
      [=](InputMixGroupBase* g) -> bool { return g->getMixSrc() == src; });

  if (g != groups.end()) return *g;

  return nullptr;
}

void InputMixPageBase::removeGroup(InputMixGroupBase* g)
{
  auto group = std::find_if(groups.begin(), groups.end(),
                            [=](InputMixGroupBase* lh) -> bool { return lh == g; });
  if (group != groups.end()) groups.erase(group);
}

InputMixButtonBase* InputMixPageBase::getLineByIndex(uint8_t index)
{
  auto l = std::find_if(lines.begin(), lines.end(), [=](InputMixButtonBase* l) {
    return l->getIndex() == index;
  });

  if (l != lines.end()) return *l;

  return nullptr;
}

void InputMixPageBase::bindPageWindow(Window* window)
{
  pageWindow = window;
}

void InputMixPageBase::rebuildFromModel(uint8_t focusIndex)
{
  if (!pageWindow) return;
  _copyMode = 0;
  _copySrc = nullptr;
  requestedFocusIndex = focusIndex;
  pageWindow->clear();
  build(pageWindow);
  requestedFocusIndex = NO_REQUESTED_FOCUS;
}

bool InputMixPageBase::shouldFocusLine(uint8_t index, bool& focusSet) const
{
  if (focusSet) return false;
  if (requestedFocusIndex != NO_REQUESTED_FOCUS &&
      requestedFocusIndex != index) {
    return false;
  }
  focusSet = true;
  return true;
}

void InputMixPageBase::removeLine(InputMixButtonBase* l)
{
  auto line = std::find_if(lines.begin(), lines.end(),
                           [=](InputMixButtonBase* lh) -> bool { return lh == l; });
  if (line == lines.end()) return;

  line = lines.erase(line);
  while (line != lines.end()) {
    (*line)->setIndex((*line)->getIndex() - 1);
    ++line;
  }
}
