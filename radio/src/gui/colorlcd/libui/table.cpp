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

#include "table.h"

#include <new>

#include "debug.h"
#include "etx_lv_theme.h"
#include "keys.h"
#include "lvgl/src/core/lv_obj_class_private.h"
#include "lvgl/src/core/lv_obj_private.h"
#include "lvgl/src/widgets/table/lv_table_private.h"
#include "mainwindow.h"

static bool area_intersects(const lv_area_t& a, const lv_area_t& b)
{
  return a.x1 <= b.x2 && a.x2 >= b.x1 && a.y1 <= b.y2 && a.y2 >= b.y1;
}

// Table
static lv_style_t table_cell;
static bool table_cell_inited = false;

static void ensure_table_cell_style()
{
  if (!table_cell_inited) {
    lv_style_init(&table_cell);
    lv_style_set_border_width(&table_cell, 1);
    lv_style_set_border_side(
        &table_cell,
        (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM));
    lv_style_set_pad_top(&table_cell, PAD_TABLE_V);
    lv_style_set_pad_bottom(&table_cell, PAD_TABLE_V);
    lv_style_set_pad_left(&table_cell, PAD_TABLE_H);
    lv_style_set_pad_right(&table_cell, PAD_TABLE_H);
    table_cell_inited = true;
  }
}

static void table_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
  ensure_table_cell_style();
  etx_obj_add_style(obj, styles->pad_zero, LV_PART_MAIN);
  etx_font(obj, FONT_STD_INDEX);

  etx_scrollbar(obj);

  etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX, LV_PART_ITEMS);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, LV_PART_ITEMS);
  etx_obj_add_style(obj, table_cell, LV_PART_ITEMS);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_SECONDARY2_INDEX],
                    LV_PART_ITEMS);
  etx_obj_add_style(obj, styles->pressed, LV_PART_ITEMS | LV_STATE_PRESSED);

  etx_bg_color(obj, COLOR_THEME_PRIMARY2_INDEX,
               LV_PART_ITEMS | LV_STATE_EDITED);
  etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                LV_PART_ITEMS | LV_STATE_EDITED);
  etx_obj_add_style(obj, styles->border_color[COLOR_THEME_PRIMARY1_INDEX],
                    LV_PART_ITEMS | LV_STATE_EDITED);
}

static const lv_obj_class_t table_class = {
    .base_class = &lv_table_class,
    .constructor_cb = table_constructor,
    .destructor_cb = nullptr,
    .event_cb = TableField::table_event,
    .user_data = nullptr,
    .width_def = 0,
    .height_def = 0,
    .editable = LV_OBJ_CLASS_EDITABLE_TRUE,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_table_t),
};

static lv_obj_t* table_create(lv_obj_t* parent)
{
  return etx_create(&table_class, parent);
}

void TableField::table_event(const lv_obj_class_t* class_p, lv_event_t* e)
{
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (obj) {
    auto tf = static_cast<TableField*>(Window::fromAvailableLvObj(obj));
    if (tf) {
      lv_event_code_t code = lv_event_get_code(e);

      switch (code) {
        case LV_EVENT_VALUE_CHANGED: {
          uint32_t row;
          uint32_t col;
          lv_table_get_selected_cell(obj, &row, &col);
          if (row != LV_TABLE_CELL_NONE && col != LV_TABLE_CELL_NONE) {
            lv_indev_type_t indev_type = lv_indev_get_type(lv_indev_get_act());
            if (indev_type == LV_INDEV_TYPE_ENCODER) {
              // Encoder send VALUE_CHANGED when selection changes
              tf->adjustScroll();
              tf->onSelected(row, col);
            } else {
              // Otherwise it's a click
              if (lv_group_get_editing((lv_group_t*)lv_obj_get_group(obj)) ||
                  indev_type == LV_INDEV_TYPE_POINTER) {
                tf->onPress(row, col);
              } else {
                tf->onClicked();
              }
              // Note: VALUE_CHANGED is generated on RELEASED
              //
              //   It must be avoided that CLICKED be generated afterwards
              //   in case the object has been deleted meanwhile and to
              //   avoid onClicked() being called.
              //
              lv_indev_reset(lv_indev_get_act(), nullptr);
            }
          }
        } break;
        case LV_EVENT_DRAW_MAIN_END: {
          lv_layer_t* layer = lv_event_get_layer(e);
          if (!layer) break;
          lv_table_t* table = (lv_table_t*)obj;
          uint16_t cols = table->col_cnt;
          if (cols) {
            lv_area_t obj_coords;
            lv_obj_get_coords(obj, &obj_coords);

            const lv_coord_t pad_left =
                lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
            const lv_coord_t pad_top =
                lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
            const lv_coord_t scroll_x = lv_obj_get_scroll_x(obj);
            const lv_coord_t scroll_y = lv_obj_get_scroll_y(obj);
            const lv_area_t& clip = layer->_clip_area;

            lv_coord_t y = obj_coords.y1 + pad_top - scroll_y;
            for (uint32_t r = 0; r < table->row_cnt; r++) {
              const lv_coord_t row_h = table->row_h[r];
              const lv_coord_t y2 = y + row_h - 1;
              if (y > clip.y2) break;
              if (y2 < clip.y1) {
                y += row_h;
                continue;
              }

              lv_coord_t x = obj_coords.x1 + pad_left - scroll_x;
              for (uint16_t c = 0; c < cols; c++) {
                const lv_coord_t col_w = table->col_w[c];
                lv_area_t cell_area = {x, y, x + col_w - 1, y2};
                x += col_w;

                if (cell_area.x1 > clip.x2) break;
                if (!area_intersects(cell_area, clip)) continue;

                tf->onDrawBegin(static_cast<uint16_t>(r), c, &cell_area, layer);
                tf->onDrawEnd(static_cast<uint16_t>(r), c, &cell_area, layer);
              }
              y += row_h;
            }
          }
        } break;
        case LV_EVENT_DRAW_POST_END: {
          bool has_focus = lv_obj_has_state(obj, LV_STATE_FOCUS_KEY);
          bool is_edited =
              lv_group_get_editing((lv_group_t*)lv_obj_get_group(obj));

          if (has_focus && !is_edited) {
            lv_layer_t* draw_ctx = lv_event_get_layer(e);

            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_opa = LV_OPA_TRANSP;
            rect_dsc.bg_image_opa = LV_OPA_TRANSP;
            rect_dsc.outline_opa = LV_OPA_TRANSP;
            rect_dsc.shadow_opa = LV_OPA_TRANSP;

            rect_dsc.border_color = makeLvColor(COLOR_THEME_PRIMARY1);
            rect_dsc.border_opa = LV_OPA_100;
            rect_dsc.border_width = lv_dpx(4);

            lv_area_t coords;
            lv_area_copy(&coords, &obj->coords);

            lv_draw_rect(draw_ctx, &rect_dsc, &coords);
          }
        } break;
        case LV_EVENT_RELEASED: {
          lv_table_t* table = (lv_table_t*)obj;
          /*From lv_table.c: handler for LV_EVENT_RELEASED*/
          lv_obj_invalidate(obj);
          lv_indev_t* indev = lv_indev_get_act();
          lv_obj_t* scroll_obj = lv_indev_get_scroll_obj(indev);
          if (table->col_act != LV_TABLE_CELL_NONE &&
              table->row_act != LV_TABLE_CELL_NONE && scroll_obj == NULL) {
            lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
          }
          // Do not call base class event handler
          return;
        }
        default:
          break;
      }
    }
  }

  /*Call the ancestor's event handler*/
  lv_obj_event_base(&table_class, e);
}

TableField::TableField(Window* parent, const rect_t& rect) :
    Window(parent, rect, table_create)
{
  setWindowFlag(OPAQUE);

  withLive([](LiveWindow& live) { lv_table_set_col_cnt(live.lvobj(), 1); });
}

void TableField::setRowCount(uint16_t rows)
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_table_set_row_cnt(obj, rows);
  });
}

uint16_t TableField::getRowCount() const
{
  uint16_t rows = 0;
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    rows = lv_table_get_row_cnt(obj);
  });
  return rows;
}

void TableField::setColumnWidth(uint16_t col, coord_t w)
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_table_set_col_width(obj, col, w);
  });
}

void TableField::select(uint16_t row, uint16_t col, bool force)
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_table_t* table = (lv_table_t*)obj;
    if (!force && table->row_act == row && table->col_act == col) return;

    if (row >= table->row_cnt || col >= table->col_cnt) {
      table->col_act = LV_TABLE_CELL_NONE;
      table->row_act = LV_TABLE_CELL_NONE;
    } else {
      table->row_act = row;
      table->col_act = col;
    }

    lv_obj_invalidate(obj);
    if (table->row_act != LV_TABLE_CELL_NONE &&
        table->col_act != LV_TABLE_CELL_NONE) {
      adjustScroll();
    }
  });
}

void TableField::adjustScroll()
{
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_table_t* table = (lv_table_t*)obj;
    if (table->row_act == LV_TABLE_CELL_NONE ||
        table->row_act >= table->row_cnt) {
      return;
    }

    // only vertical scroll for now
    const uint32_t row = table->row_act;
    lv_coord_t h_before = 0;
    for (uint32_t i = 0; i < row; i++) h_before += table->row_h[i];

    lv_coord_t row_h = table->row_h[row];
    lv_coord_t scroll_y = lv_obj_get_scroll_y(obj);

    lv_obj_update_layout(obj);
    lv_coord_t h = lv_obj_get_height(obj);

    lv_coord_t diff_y = 0;
    if (h_before < scroll_y) {
      diff_y = scroll_y - h_before;
    } else if (scroll_y + h < h_before + row_h) {
      diff_y = scroll_y + h - h_before - row_h;
    } else {
      return;
    }

    lv_obj_scroll_by_bounded(obj, 0, diff_y, LV_ANIM_OFF);
  });
}

int TableField::getSelected() const
{
  uint32_t row = LV_TABLE_CELL_NONE;
  uint32_t col = LV_TABLE_CELL_NONE;
  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    lv_table_get_selected_cell(obj, &row, &col);
  });
  if (row != LV_TABLE_CELL_NONE) {
    return row;
  }
  return -1;
}

bool TableField::onLiveLongPress(Window::LiveWindow&)
{
  TRACE("LONG_PRESS");
  if (longPressHandler) {
    longPressHandler();
    lv_indev_wait_release(lv_indev_get_act());
    return false;
  }
  return true;
}

void TableField::setAutoEdit()
{
  if (autoedit) return;

  withLive([&](LiveWindow& live) {
    auto obj = live.lvobj();
    autoedit = true;

    oldGroup = lv_group_get_default();
    group = lv_group_create();
    if (!group) {
      autoedit = false;
      return;
    }
    lv_group_add_obj(group, obj);
    assignLvGroup(group, true);

    lv_group_set_editing(group, true);

    setFocusHandler([=](bool focus) {
      if (focus) {
        lv_group_set_focus_cb(group, TableField::force_editing);
      } else {
        lv_group_set_focus_cb(group, nullptr);
      }
    });
  });
}

void TableField::onDelete()
{
  if (autoedit) {
    if (group) lv_group_del(group);
    if (oldGroup)
      assignLvGroup(oldGroup, true);
    else
      lv_group_set_default(nullptr);
    group = nullptr;
    oldGroup = nullptr;
    autoedit = false;
  }
}

#if defined(SIMU)
void etxCreateForceObjectAllocationFailureForTest(bool force);

bool tableFieldObjectAllocationFailureFailsClosedForTest()
{
  class TestTableField : public TableField
  {
   public:
    explicit TestTableField(Window* parent) :
        TableField(parent, {0, 0, 100, 40})
    {
    }

    void exercise()
    {
      setRowCount(1);
      (void)getRowCount();
      setColumnWidth(0, 100);
      select(0, 0);
      adjustScroll();
      setAutoEdit();
      setLongPressHandler([]() {});
      (void)onLongPress();
    }
  };

  etxCreateForceObjectAllocationFailureForTest(true);
  auto table = new (std::nothrow) TestTableField(MainWindow::instance());
  etxCreateForceObjectAllocationFailureForTest(false);

  if (!table) return false;
  table->exercise();

  bool ok = !table->isAvailable() && !table->isVisible() &&
            !table->automationClickable();
  delete table;
  return ok;
}

bool tableFieldInvalidSelectionClearsWithoutScrollForTest()
{
  auto table =
      new (std::nothrow) TableField(MainWindow::instance(), {0, 0, 100, 40});
  if (!table || !table->isAvailable()) {
    delete table;
    return false;
  }

  table->setRowCount(1);
  table->select(0, 0);
  table->select(99, 0);
  bool ok = table->getSelected() == -1;
  delete table;
  return ok;
}

bool tableFieldSelectMovesAcrossColumnsForTest()
{
  auto table =
      new (std::nothrow) TableField(MainWindow::instance(), {0, 0, 100, 40});
  if (!table || !table->isAvailable()) {
    delete table;
    return false;
  }

  table->setRowCount(1);
  table->withLive(
      [](Window::LiveWindow& live) { lv_table_set_col_cnt(live.lvobj(), 2); });
  table->select(0, 0);
  table->select(0, 1);

  uint32_t row = LV_TABLE_CELL_NONE;
  uint32_t col = LV_TABLE_CELL_NONE;
  table->withLive([&](Window::LiveWindow& live) {
    lv_table_get_selected_cell(live.lvobj(), &row, &col);
  });

  bool ok = row == 0 && col == 1;
  delete table;
  return ok;
}
#endif
