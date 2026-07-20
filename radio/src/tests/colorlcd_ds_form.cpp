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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// Structural proof for the DESIGN_SYSTEM.md form-container rule (see
// ds::Card in radio/src/gui/colorlcd/libui/ds_core.cpp). The settings form used
// by Timer / Battery / Model Setup / Radio Setup is a ds::List (owns the page
// margins) wrapping a ds::Card that groups the ds::FormRows. That Card must be
// BORDERLESS and FLUSH by default: a border there separates the form from
// nothing, and a second inset on top of the List's margins just wastes width.
//
// A genuinely-separated card (`bordered = true`) must still draw its subtle
// border + surface + inset, so the borderless default cannot silently strip a
// panel that actually wants separation. Both are asserted against real LVGL
// style state (the same technique colorlcd_ds_grid.cpp / colorlcd_ds_picker.cpp
// use).

#include "gtests.h"

#if defined(COLORLCD) && defined(SIMU)

#include <cstdlib>
#include <vector>

#include "base_dialog.h"
#include "ds_core.h"
#include "etx_lv_theme.h"  // LAYOUT_SCALE
#include "mainwindow.h"

namespace {

// Hosts the exact production shape: a ds::List with a default (form) Card and a
// bordered Card, mounted on a modal layer so it does not perturb neighbouring
// tests' global window state.
class FormDialog : public BaseDialog
{
 public:
  ds::List* list = nullptr;
  ds::Card* formCard = nullptr;
  ds::Card* borderedCard = nullptr;

  FormDialog() : BaseDialog("Form", false)
  {
    form.with([&](Window& f) {
      list = new (std::nothrow) ds::List(&f);
      formCard = new (std::nothrow) ds::Card(list);
      borderedCard =
          new (std::nothrow) ds::Card(list, nullptr, /*bordered=*/true);
    });
  }
};

void settle()
{
  for (int i = 0; i < 3; i++) MainWindow::instance()->runMainLoopTick();
}

struct Box {
  lv_coord_t x1 = 0, y1 = 0, w = 0, h = 0;
};

Box boxOf(lv_obj_t* obj)
{
  if (!obj) return {};
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return {a.x1, a.y1, lv_coord_t(a.x2 - a.x1 + 1), lv_coord_t(a.y2 - a.y1 + 1)};
}

Box cellBox(ds::FieldRow* row, int i)
{
  Box b;
  Window* cell = row ? row->fieldCellForTest(i) : nullptr;
  if (cell) cell->withLive([&](Window::LiveWindow& live) { b = boxOf(live.lvobj()); });
  return b;
}

void settleAndLayout(Window* host)
{
  settle();
  if (host)
    host->withLive(
        [](Window::LiveWindow& live) { lv_obj_update_layout(live.lvobj()); });
}

// Hosts a ds::List/Card of ds::FieldRows on a modal layer (the production form
// shape) so their real LVGL geometry can be measured without perturbing
// neighbouring tests' global window state.
class FieldRowDialog : public BaseDialog
{
 public:
  ds::List* list = nullptr;
  ds::FieldRow* rowA = nullptr;   // 2 fields
  ds::FieldRow* rowB = nullptr;   // 2 fields (alignment cross-check)
  ds::FieldRow* rowC = nullptr;   // 3 fields
  Window* ctrlA[2] = {};

  FieldRowDialog() : BaseDialog("FieldRow", false)
  {
    form.with([&](Window& f) {
      list = new (std::nothrow) ds::List(&f);
      auto* card = new (std::nothrow) ds::Card(list);
      rowA = new (std::nothrow) ds::FieldRow(
          card, {{"Min", [&](Window* s) { ctrlA[0] = new Window(s, rect_t{}); }},
                 {"Max", [&](Window* s) { ctrlA[1] = new Window(s, rect_t{}); }}});
      rowB = new (std::nothrow) ds::FieldRow(
          card, {{"A", [](Window* s) { new Window(s, rect_t{}); }},
                 {"B", [](Window* s) { new Window(s, rect_t{}); }}});
      rowC = new (std::nothrow) ds::FieldRow(
          card, {{"X", [](Window* s) { new Window(s, rect_t{}); }},
                 {"Y", [](Window* s) { new Window(s, rect_t{}); }},
                 {"Z", [](Window* s) { new Window(s, rect_t{}); }}});
    });
  }
};

// Hosts ds::FieldGroups on a modal layer (the production form shape) so their
// real LVGL geometry can be measured. `group` flows four equal controls, each
// wide enough (45% of the control column) that two share the first line and the
// rest WRAP; `tiny` flows a single zero-size control to prove the touch-floor
// clamp.
class FieldGroupDialog : public BaseDialog
{
 public:
  ds::List* list = nullptr;
  ds::FieldGroup* group = nullptr;
  ds::FieldGroup* tiny = nullptr;
  std::vector<Window*> ctrls;
  Window* tinyCtrl = nullptr;

  FieldGroupDialog() : BaseDialog("FieldGroup", false)
  {
    form.with([&](Window& f) {
      list = new (std::nothrow) ds::List(&f);
      auto* card = new (std::nothrow) ds::Card(list);
      group = new (std::nothrow) ds::FieldGroup(card, "Opts", [&](Window* c) {
        for (int i = 0; i < 4; i++)
          ctrls.push_back(new (std::nothrow) Window(c, rect_t{0, 0, LV_PCT(45), 24}));
      });
      tiny = new (std::nothrow) ds::FieldGroup(card, "T", [&](Window* c) {
        tinyCtrl = new (std::nothrow) Window(c, rect_t{});
      });
    });
  }
};

Box winBox(Window* w)
{
  Box b;
  if (w) w->withLive([&](Window::LiveWindow& live) { b = boxOf(live.lvobj()); });
  return b;
}

}  // namespace

// ds::FieldGroup flows a VARIABLE number of controls left-to-right after its
// leading label and WRAPS them to the next line when they overflow the control
// column — the shape FieldRow's fixed even columns can't express (the RF
// module-protocol option boxes). Two 45%-wide controls share the first line
// (proving left-to-right order), and the group spills onto a second line
// (proving the wrap).
TEST(DesignSystemForm, FieldGroupFlowsLeftToRightAndWraps)
{
  auto* dlg = new (std::nothrow) FieldGroupDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->group, nullptr);
  ASSERT_EQ(dlg->ctrls.size(), 4u);
  settleAndLayout(dlg);

  Box c0 = winBox(dlg->ctrls[0]);
  Box c1 = winBox(dlg->ctrls[1]);
  Box c3 = winBox(dlg->ctrls[3]);

  // First two controls sit side-by-side on the same line, left-to-right.
  EXPECT_GT(c1.x1, c0.x1) << "controls are not flowed left-to-right";
  EXPECT_EQ(c1.y1, c0.y1) << "first two controls are not on the same line";

  // With four 45%-wide controls the group cannot fit on one line, so a later
  // control wraps below the first.
  EXPECT_GT(c3.y1, c0.y1) << "the control area did not wrap to a second line";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// Every control flowed into a FieldGroup is clamped to the 40 px touch floor
// (min width AND height), so a bare toggle or a narrow edit stays tappable even
// sharing the line — the DS floor the hand-rolled boxes never guaranteed.
TEST(DesignSystemForm, FieldGroupControlsMeetTouchFloor)
{
  const lv_coord_t kFloor = LAYOUT_SCALE(40);

  auto* dlg = new (std::nothrow) FieldGroupDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->tinyCtrl, nullptr);
  settleAndLayout(dlg);

  Box t = winBox(dlg->tinyCtrl);
  EXPECT_GE(t.w, kFloor) << "flowed control below 40 px touch width";
  EXPECT_GE(t.h, kFloor) << "flowed control below 40 px touch height";

  // The larger controls clear the floor vertically too.
  for (auto* ctrl : dlg->ctrls) {
    Box c = winBox(ctrl);
    EXPECT_GE(c.h, kFloor) << "flowed control below 40 px touch height";
  }

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// The default Card (every settings form) is borderless and flush: no border and
// no inset, so its rows align to the enclosing ds::List's page margins with
// nothing separating the group from the rest of the page.
TEST(DesignSystemForm, DefaultFormCardIsBorderlessAndFlush)
{
  auto* dlg = new (std::nothrow) FormDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->formCard, nullptr);
  settle();

  dlg->formCard->withLive([](Window::LiveWindow& live) {
    lv_obj_t* o = live.lvobj();
    EXPECT_EQ(lv_obj_get_style_border_width(o, LV_PART_MAIN), 0)
        << "form Card must not draw a border";
    EXPECT_EQ(lv_obj_get_style_pad_left(o, LV_PART_MAIN), 0)
        << "form Card must not add a second inset over the List margins";
    EXPECT_EQ(lv_obj_get_style_pad_right(o, LV_PART_MAIN), 0);
    EXPECT_EQ(lv_obj_get_style_pad_top(o, LV_PART_MAIN), 0);
    EXPECT_EQ(lv_obj_get_style_pad_bottom(o, LV_PART_MAIN), 0);
  });

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// A card explicitly asked to separate (`bordered = true`) still draws its
// border and grouping inset -- the borderless default does not strip panels
// that genuinely want to stand apart.
TEST(DesignSystemForm, BorderedCardKeepsBorderAndInset)
{
  auto* dlg = new (std::nothrow) FormDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->borderedCard, nullptr);
  settle();

  dlg->borderedCard->withLive([](Window::LiveWindow& live) {
    lv_obj_t* o = live.lvobj();
    EXPECT_GT(lv_obj_get_style_border_width(o, LV_PART_MAIN), 0)
        << "a bordered Card must still draw its separating border";
    EXPECT_EQ(lv_obj_get_style_pad_left(o, LV_PART_MAIN), LAYOUT_SCALE(12))
        << "a bordered Card keeps its space-3 grouping inset";
  });

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// ds::FieldRow lays N side-by-side fields into N EVEN columns, each clearing the
// 40 px touch floor, and clamps every control slot to that floor — the
// structural guarantee that a multi-control settings line (Min | Max, etc.)
// stays tappable and aligned, DS-owned, without per-screen coordinates.
TEST(DesignSystemForm, FieldRowEvenColumnsMeetTouchFloor)
{
  const lv_coord_t kFloor = LAYOUT_SCALE(40);

  auto* dlg = new (std::nothrow) FieldRowDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->rowA, nullptr);
  ASSERT_NE(dlg->rowC, nullptr);
  settleAndLayout(dlg);

  EXPECT_EQ(dlg->rowA->fieldCount(), 2);
  EXPECT_EQ(dlg->rowC->fieldCount(), 3);

  // Two-field row: two columns, even width, each >= the touch floor, laid out
  // left-to-right.
  Box a0 = cellBox(dlg->rowA, 0);
  Box a1 = cellBox(dlg->rowA, 1);
  EXPECT_GE(a0.w, kFloor) << "field 0 below 40 px";
  EXPECT_GE(a1.w, kFloor) << "field 1 below 40 px";
  EXPECT_LE(std::abs(a0.w - a1.w), 1) << "two fields are not even columns";
  EXPECT_GT(a1.x1, a0.x1) << "fields are not laid out side-by-side";

  // Each control slot is itself clamped to the touch floor (several controls on
  // one line must never shrink a control below a tappable width).
  {
    ASSERT_NE(dlg->ctrlA[0], nullptr);
    Box slot0;
    dlg->ctrlA[0]->withLive(
        [&](Window::LiveWindow& l) { slot0 = boxOf(l.lvobj()); });
    EXPECT_GE(slot0.w, kFloor) << "control slot below 40 px touch floor";
  }

  // Three-field row: three even columns, each still >= the touch floor.
  Box c0 = cellBox(dlg->rowC, 0);
  Box c1 = cellBox(dlg->rowC, 1);
  Box c2 = cellBox(dlg->rowC, 2);
  EXPECT_GE(c0.w, kFloor);
  EXPECT_GE(c1.w, kFloor);
  EXPECT_GE(c2.w, kFloor);
  EXPECT_LE(std::abs(c0.w - c1.w), 1);
  EXPECT_LE(std::abs(c1.w - c2.w), 1);
  EXPECT_GT(c1.x1, c0.x1);
  EXPECT_GT(c2.x1, c1.x1);

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// Two FieldRows built with the same field count share ONE column template, so
// field i sits at the same x with the same width in both — the same structural
// alignment guarantee ds::Grid gives its cells, here for stacked form lines.
TEST(DesignSystemForm, FieldRowColumnsShareTemplateAcrossRows)
{
  auto* dlg = new (std::nothrow) FieldRowDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->rowA, nullptr);
  ASSERT_NE(dlg->rowB, nullptr);
  settleAndLayout(dlg);

  for (int c = 0; c < 2; c++) {
    Box a = cellBox(dlg->rowA, c);
    Box b = cellBox(dlg->rowB, c);
    EXPECT_EQ(a.x1, b.x1) << "column " << c << " drifted between rows";
    EXPECT_EQ(a.w, b.w) << "column " << c << " width drifted between rows";
  }

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// highlightField() latches an active-state cue on a field's label (the live
// range-end highlight Output edit drives) and clears it again.
TEST(DesignSystemForm, FieldRowHighlightTogglesActiveState)
{
  auto* dlg = new (std::nothrow) FieldRowDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->rowA, nullptr);
  settleAndLayout(dlg);

  lv_obj_t* label0 = dlg->rowA->labelForTest(0);
  ASSERT_NE(label0, nullptr);
  EXPECT_FALSE(lv_obj_has_state(label0, LV_STATE_CHECKED));

  dlg->rowA->highlightField(0, true);
  EXPECT_TRUE(lv_obj_has_state(label0, LV_STATE_CHECKED))
      << "highlightField(true) did not latch the active state";

  dlg->rowA->highlightField(0, false);
  EXPECT_FALSE(lv_obj_has_state(label0, LV_STATE_CHECKED))
      << "highlightField(false) did not clear the active state";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

#endif  // COLORLCD && SIMU
