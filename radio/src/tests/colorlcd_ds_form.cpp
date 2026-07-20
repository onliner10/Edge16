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

}  // namespace

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

#endif  // COLORLCD && SIMU
