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

// Hosts the shape that produced the reported spacing bug: a form where an
// inline status line sits between two ordinary fields. Built with ds::Caption,
// the status line must NOT behave like a third field.
class CaptionDialog : public BaseDialog
{
 public:
  ds::List* list = nullptr;
  ds::FormRow* rowA = nullptr;
  ds::Caption* caption = nullptr;
  ds::FormRow* rowB = nullptr;
  ds::FormRow* rowC = nullptr;

  CaptionDialog() : BaseDialog("Caption", false)
  {
    form.with([&](Window& f) {
      list = new (std::nothrow) ds::List(&f);
      auto* card = new (std::nothrow) ds::Card(list);
      rowA = new (std::nothrow) ds::FormRow(
          card, "A", [](Window* s) { new Window(s, rect_t{}); });
      caption = new (std::nothrow) ds::Caption(card, "ID is unique");
      rowB = new (std::nothrow) ds::FormRow(
          card, "B", [](Window* s) { new Window(s, rect_t{}); });
      rowC = new (std::nothrow) ds::FormRow(
          card, "C", [](Window* s) { new Window(s, rect_t{}); });
    });
  }
};

// Vertical gap between two stacked siblings.
lv_coord_t gapBetween(const Box& above, const Box& below)
{
  return lv_coord_t(below.y1 - (above.y1 + above.h));
}

// Hosts DS rows added STRAIGHT to a bare flex-column body -- no ds::List, no
// ds::Card -- which is what several screens actually do (see the "added
// directly to the flex body" comment in model_telemetry.cpp). The body has no
// DS spacing of its own, so without the row's own adjacency the rows stack
// with a gap of exactly zero.
class BareBodyDialog : public BaseDialog
{
 public:
  Window* body = nullptr;
  ds::FieldGroup* groupA = nullptr;
  ds::FieldGroup* groupB = nullptr;
  ds::FormRow* rowA = nullptr;
  ds::FormRow* rowB = nullptr;

  BareBodyDialog() : BaseDialog("BareBody", false)
  {
    form.with([&](Window& f) {
      body = new (std::nothrow) Window(&f, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT});
      if (!body) return;
      // A plain flex column, exactly like a page body -- deliberately NOT a
      // ds::List, and deliberately given no row gap.
      body->withLive([](Window::LiveWindow& live) {
        lv_obj_t* obj = live.lvobj();
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(obj, 0, LV_PART_MAIN);
      });
      groupA = new (std::nothrow) ds::FieldGroup(body, "Range", [](Window* c) {
        new Window(c, rect_t{0, 0, 60, 24});
        new Window(c, rect_t{0, 0, 60, 24});
      });
      groupB = new (std::nothrow) ds::FieldGroup(body, "Center", [](Window* c) {
        new Window(c, rect_t{0, 0, 60, 24});
        new Window(c, rect_t{0, 0, 60, 24});
      });
      rowA = new (std::nothrow) ds::FormRow(body, "A", [](Window* s) {
        new Window(s, rect_t{0, 0, 60, 24});
      });
      rowB = new (std::nothrow) ds::FormRow(body, "B", [](Window* s) {
        new Window(s, rect_t{0, 0, 60, 24});
      });
    });
  }
};

// Hosts a FormRow and a FieldRow each carrying one control built at the
// STANDARD control height (EdgeTxStyles::UI_ELEMENT_HEIGHT, 32 px) -- what a
// ToggleSwitch/Choice/NumberEdit actually is -- so the touch floor can be
// measured against the real production geometry rather than a zero-size stub.
class TouchFloorDialog : public BaseDialog
{
 public:
  Window* formCtrl = nullptr;
  Window* fieldCtrl = nullptr;

  explicit TouchFloorDialog(lv_coord_t ctrlH) : BaseDialog("TouchFloor", false)
  {
    form.with([&](Window& f) {
      auto* list = new (std::nothrow) ds::List(&f);
      auto* card = new (std::nothrow) ds::Card(list);
      new (std::nothrow) ds::FormRow(card, "Toggle", [&](Window* s) {
        formCtrl = new (std::nothrow) Window(s, rect_t{0, 0, 52, ctrlH});
      });
      new (std::nothrow) ds::FieldRow(
          card, {{"A",
                  [&](Window* s) {
                    fieldCtrl = new (std::nothrow) Window(s, rect_t{0, 0, 52, ctrlH});
                  }},
                 {"B", [](Window* s) { new Window(s, rect_t{}); }}});
    });
  }
};

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

// The DS promises a 40 px / 8 mm touch floor, but a FormRow/FieldRow sets
// NO_FOCUS on itself -- the CONTROL is the tap target, and the standard control
// height is 32 px. Reserving 40 px of row space around a 32 px control does not
// make the control tappable to the floor; only its click box does. Assert the
// effective touch box (drawn box + ext_click_area) clears the floor in both
// containers, for a control built at the standard control height.
TEST(DesignSystemForm, FormControlsMeetTheTouchFloor)
{
  const lv_coord_t floor = ds::rowHeight(ds::RowSize::OneLine);
  const lv_coord_t ctrlH = EdgeTxStyles::UI_ELEMENT_HEIGHT;
  ASSERT_LT(ctrlH, floor) << "premise: the standard control is under the floor";

  auto* dlg = new (std::nothrow) TouchFloorDialog(ctrlH);
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->formCtrl, nullptr);
  ASSERT_NE(dlg->fieldCtrl, nullptr);
  settleAndLayout(dlg);

  // Measure through lv_obj_get_click_area -- the exact area LVGL's own
  // lv_obj_hit_test() consults -- rather than the drawn coords, so the test
  // asserts what a finger can actually reach.
  auto hitBox = [](Window* w) -> Box {
    Box b;
    if (!w) return b;
    w->withLive([&](Window::LiveWindow& live) {
      lv_area_t a;
      lv_obj_get_click_area(live.lvobj(), &a);
      b = {a.x1, a.y1, lv_coord_t(a.x2 - a.x1 + 1), lv_coord_t(a.y2 - a.y1 + 1)};
    });
    return b;
  };

  Box form = hitBox(dlg->formCtrl);
  EXPECT_GE(form.h, floor) << "FormRow control hit box is " << form.h
                           << " px tall, under the " << floor << " px floor";
  EXPECT_GE(form.w, floor) << "FormRow control hit box is " << form.w
                           << " px wide, under the " << floor << " px floor";

  Box field = hitBox(dlg->fieldCtrl);
  EXPECT_GE(field.h, floor) << "FieldRow control hit box is " << field.h
                            << " px tall, under the " << floor << " px floor";
  EXPECT_GE(field.w, floor) << "FieldRow control hit box is " << field.w
                            << " px wide, under the " << floor << " px floor";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// DS rows must never TOUCH, even when a screen adds them straight to a bare
// page body instead of a ds::List. ds::List owns the inter-row gap, so
// bypassing it produced a gap of exactly zero -- invisible on FormRow, whose
// 32 px control sits inset in a 40 px row and shows ~8 px of whitespace that
// reads as a gap, but plainly visible on FieldGroup, which draws a bordered
// box filling the row: two adjacent groups had their borders flush against
// each other. Whether spacing exists at all must not depend on which row type
// a screen happened to reach for, so a row carries its adjacency into whatever
// container it lands in.
TEST(DesignSystemForm, RowsDoNotTouchInABareBody)
{
  auto* dlg = new (std::nothrow) BareBodyDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->groupA, nullptr);
  ASSERT_NE(dlg->groupB, nullptr);
  ASSERT_NE(dlg->rowA, nullptr);
  ASSERT_NE(dlg->rowB, nullptr);
  settleAndLayout(dlg);

  Box ga = winBox(dlg->groupA);
  Box gb = winBox(dlg->groupB);
  Box ra = winBox(dlg->rowA);
  Box rb = winBox(dlg->rowB);

  // The bordered FieldGroup boxes are the visible case.
  EXPECT_GT(gapBetween(ga, gb), 0)
      << "two ds::FieldGroups in a bare body are touching (gap "
      << gapBetween(ga, gb) << ")";
  // ...and plain rows get the identical gap, so spacing is uniform down the
  // page regardless of row type.
  EXPECT_EQ(gapBetween(ra, rb), gapBetween(ga, gb))
      << "FormRow and FieldGroup disagree on the inter-row gap";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// An inline status line is an ANNOTATION, not a field. ds::List gives every
// child the same inter-row gap, sized on the assumption that a child is a full
// touch-floor row; a status line built as an empty-label FormRow therefore
// inherits the 40 px floor and reads as a second, mostly-empty row, roughly
// tripling the apparent distance to its neighbours (the reported "gap between
// Channel Range and Receiver is bigger than everywhere else" bug). ds::Caption
// sizes to its own text instead, so it stays strictly under the touch floor.
TEST(DesignSystemForm, CaptionStaysUnderTheTouchFloor)
{
  auto* dlg = new (std::nothrow) CaptionDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->caption, nullptr);
  ASSERT_NE(dlg->rowA, nullptr);
  settleAndLayout(dlg);

  const lv_coord_t floor = ds::rowHeight(ds::RowSize::OneLine);
  Box cap = winBox(dlg->caption);
  Box row = winBox(dlg->rowA);

  EXPECT_GT(cap.h, 0) << "caption collapsed to nothing";
  EXPECT_LT(cap.h, floor)
      << "caption claims a full touch-floor row (" << cap.h << " >= " << floor
      << ") -- it is not a focus target and must size to its own text";
  EXPECT_GE(row.h, floor)
      << "a real form row must still meet the touch floor";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

// ...and the gap ABOVE and BELOW a caption is the same uniform inter-row gap
// every other pair of rows gets. Spacing down a form page must not depend on
// what kind of row happens to be adjacent.
TEST(DesignSystemForm, CaptionKeepsTheUniformRowGap)
{
  auto* dlg = new (std::nothrow) CaptionDialog();
  ASSERT_NE(dlg, nullptr);
  ASSERT_TRUE(dlg->isAvailable());
  ASSERT_NE(dlg->caption, nullptr);
  settleAndLayout(dlg);

  Box a = winBox(dlg->rowA);
  Box cap = winBox(dlg->caption);
  Box b = winBox(dlg->rowB);
  Box c = winBox(dlg->rowC);

  // Reference: the gap between two ordinary adjacent form rows.
  const lv_coord_t reference = gapBetween(b, c);
  EXPECT_GE(reference, 0) << "rows are not stacked top-to-bottom";

  EXPECT_EQ(gapBetween(a, cap), reference)
      << "gap above the caption differs from the standard inter-row gap";
  EXPECT_EQ(gapBetween(cap, b), reference)
      << "gap below the caption differs from the standard inter-row gap";

  dlg->deleteLater();
  for (int i = 0; i < 8; i++) MainWindow::instance()->runMainLoopTick();
}

#endif  // COLORLCD && SIMU
