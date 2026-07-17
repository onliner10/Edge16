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

// EdgeTX color-LCD design system (DS) implementation — see DESIGN_SYSTEM.md.
//
// The raw pixel values and LVGL pad/min-size calls below are the ONLY
// authorized instances outside etx_lv_theme.cpp; the design-system guard
// (tools/design-system/check_design_system.py) enforces that boundary.

#include "ds_core.h"

#include <new>

#include "etx_lv_theme.h"
#include "static.h"

namespace ds {

// ---------------------------------------------------------------------------
// Tokens (private). TX16S: 128.3 PPI → 5 px/mm. Base unit 4 px = 0.8 mm.
// ---------------------------------------------------------------------------

namespace {

constexpr coord_t kSpace1 = LAYOUT_SCALE(4);   // micro
constexpr coord_t kSpace2 = LAYOUT_SCALE(8);   // related
constexpr coord_t kSpace3 = LAYOUT_SCALE(12);  // grouping / margins
constexpr coord_t kSpace4 = LAYOUT_SCALE(16);  // separation / dialog frame

constexpr coord_t kTouchMin = LAYOUT_SCALE(40);   // 8 mm touch floor
constexpr coord_t kRowOneLine = LAYOUT_SCALE(40);
constexpr coord_t kRowPicker = LAYOUT_SCALE(48);
constexpr coord_t kRowTwoLine = LAYOUT_SCALE(52);
constexpr coord_t kLeadingSlotW = LAYOUT_SCALE(40);
constexpr coord_t kButtonMinW = LAYOUT_SCALE(96);
constexpr coord_t kFlagBorder = 3;  // non-color structural cue

LcdColorIndex roleColor(TextRole role)
{
  switch (role) {
    case TextRole::Muted:
      return COLOR_THEME_SECONDARY1_INDEX;
    case TextRole::Warning:
      return COLOR_THEME_WARNING_INDEX;
    case TextRole::Active:
      return COLOR_THEME_ACTIVE_INDEX;
    default:
      return COLOR_THEME_PRIMARY1_INDEX;
  }
}

FontIndex roleFont(TextRole role)
{
  return role == TextRole::Strong ? FONT_BOLD_INDEX : FONT_STD_INDEX;
}

LcdFlags roleTextFlags(TextRole role)
{
  return role == TextRole::Strong ? FONT(BOLD) : 0;
}

}  // namespace

coord_t rowHeight(RowSize size)
{
  switch (size) {
    case RowSize::Picker:
      return kRowPicker;
    case RowSize::TwoLine:
      return kRowTwoLine;
    default:
      return kRowOneLine;
  }
}

coord_t embeddedControlSize() { return kSpace4; }

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

List::List(Window* parent) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
{
  withLive([](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_left(obj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_pad_top(obj, kSpace2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, kSpace2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);  // adjacency gap
  });
}

// ---------------------------------------------------------------------------
// SectionHeader
// ---------------------------------------------------------------------------

SectionHeader::SectionHeader(Window* parent, const char* text) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
{
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_style_pad_top(obj, kSpace4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, kSpace1, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj, kSpace3, LV_PART_MAIN);
    lv_obj_t* label = etx_label_create(obj, FONT_XS_INDEX);
    if (label) {
      lv_label_set_text(label, text ? text : "");
      etx_txt_color(label, COLOR_THEME_SECONDARY1_INDEX);
    }
  });
#if defined(SIMU)
  setAutomationText(text ? text : "");
#endif
}

// ---------------------------------------------------------------------------
// RowContent
// ---------------------------------------------------------------------------

RowContent::RowContent(Window* row, RowSize size) : row(row), size(size)
{
  if (!row) return;
  row->setHeight(rowHeight(size));
  row->withLive([&](Window::LiveWindow& live) {
    rowObj = live.lvobj();
    lv_obj_set_style_pad_left(rowObj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_pad_right(rowObj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_pad_top(rowObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(rowObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(rowObj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_min_height(rowObj, kTouchMin, LV_PART_MAIN);
    lv_obj_set_flex_flow(rowObj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(rowObj, LV_OBJ_FLAG_SCROLLABLE);

    textCol = window_create(rowObj);
    if (!textCol) return;
    lv_obj_set_height(textCol, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(textCol, 1);
    lv_obj_set_flex_flow(textCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(textCol, 0, LV_PART_MAIN);
    lv_obj_clear_flag(textCol, LV_OBJ_FLAG_CLICKABLE);

    titleLabel = etx_label_create(textCol, FONT_BOLD_INDEX);
    if (titleLabel) {
      lv_obj_set_width(titleLabel, LV_PCT(100));
      lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
      lv_label_set_text(titleLabel, "");
      etx_txt_color(titleLabel, COLOR_THEME_PRIMARY1_INDEX);
    }

    if (size == RowSize::TwoLine) {
      subtitleLabel = etx_label_create(textCol, FONT_STD_INDEX);
      if (subtitleLabel) {
        lv_obj_set_width(subtitleLabel, LV_PCT(100));
        lv_label_set_long_mode(subtitleLabel, LV_LABEL_LONG_DOT);
        lv_label_set_text(subtitleLabel, "");
        etx_txt_color(subtitleLabel, COLOR_THEME_SECONDARY1_INDEX);
      }
    }
  });
}

void RowContent::setTitle(const char* text)
{
  if (titleLabel) lv_label_set_text(titleLabel, text ? text : "");
}

void RowContent::setSubtitle(const char* text)
{
  if (subtitleLabel) lv_label_set_text(subtitleLabel, text ? text : "");
}

void RowContent::ensureTrailing(TextRole role)
{
  if (!trailingLabel && rowObj) {
    trailingLabel = etx_label_create(rowObj, roleFont(role));
    if (trailingLabel) {
      lv_obj_set_width(trailingLabel, LV_SIZE_CONTENT);
      lv_obj_set_style_text_align(trailingLabel, LV_TEXT_ALIGN_RIGHT,
                                  LV_PART_MAIN);
    }
  }
  if (trailingLabel) etx_txt_color(trailingLabel, roleColor(role));
}

void RowContent::setTrailing(const char* text, TextRole role)
{
  ensureTrailing(role);
  if (trailingLabel) lv_label_set_text(trailingLabel, text ? text : "");
}

Window* RowContent::leadingSlot()
{
  if (leading || !row) return leading;
  leading = new (std::nothrow)
      Window(row, rect_t{0, 0, kLeadingSlotW, LV_PCT(100)});
  if (leading) {
    leading->withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_move_to_index(obj, 0);  // always at the leading edge
      lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    });
  }
  return leading;
}

void RowContent::expandHitArea(Window* control)
{
  if (!control) return;
  control->withLive([](Window::LiveWindow& live) {
    // Touch floor: a small embedded control (e.g. 16 px checkbox) taps as a
    // >= 40x40 px target filling its slot.
    lv_obj_set_ext_click_area(live.lvobj(), kSpace3);
  });
}

// ---------------------------------------------------------------------------
// ListRow
// ---------------------------------------------------------------------------

ListRow::ListRow(Window* parent, const Content& content,
                 std::function<uint8_t()> onPress,
                 std::function<uint8_t()> onLongPress) :
    ListRow(parent,
            content.subtitle ? RowSize::TwoLine : RowSize::OneLine, content,
            std::move(onPress), std::move(onLongPress))
{
}

ListRow::ListRow(Window* parent, RowSize size, const Content& content,
                 std::function<uint8_t()> onPress,
                 std::function<uint8_t()> onLongPress) :
    Button(parent, rect_t{0, 0, LV_PCT(100), rowHeight(size)}),
    content_(this, size)
{
  if (content.title) content_.setTitle(content.title);
  if (content.subtitle) content_.setSubtitle(content.subtitle);
  if (content.trailing)
    content_.setTrailing(content.trailing, content.trailingRole);
  if (onPress) setPressHandler(std::move(onPress));
  if (onLongPress) setLongPressHandler(std::move(onLongPress));
#if defined(SIMU)
  setAutomationText(content.title ? content.title : "");
#endif
}

void ListRow::setFlagged(bool flagged)
{
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    if (flagged) {
      lv_obj_set_style_border_width(obj, kFlagBorder, LV_PART_MAIN);
      etx_border_color(obj, COLOR_THEME_WARNING_INDEX, LV_PART_MAIN);
    } else {
      lv_obj_remove_local_style_prop(obj, LV_STYLE_BORDER_WIDTH, LV_PART_MAIN);
      etx_remove_border_color(obj, LV_PART_MAIN);
    }
  });
}

// ---------------------------------------------------------------------------
// DSButton
// ---------------------------------------------------------------------------

DSButton::DSButton(Window* parent, const char* text, ButtonRole role,
                   std::function<uint8_t()> onPress) :
    TextButton(parent, rect_t{0, 0, LV_SIZE_CONTENT, kTouchMin},
               text ? text : "", std::move(onPress))
{
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_style_min_width(obj, kButtonMinW, LV_PART_MAIN);
    lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj, kSpace4, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, kSpace4, LV_PART_MAIN);

    switch (role) {
      case ButtonRole::Primary:
        etx_bg_color(obj, COLOR_THEME_ACTIVE_INDEX, LV_PART_MAIN);
        etx_bg_color(obj, COLOR_THEME_ACTIVE_INDEX,
                     LV_PART_MAIN | LV_STATE_FOCUSED);
        etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, LV_PART_MAIN);
        etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                      LV_PART_MAIN | LV_STATE_FOCUSED);
        break;
      case ButtonRole::Destructive:
        etx_txt_color(obj, COLOR_THEME_WARNING_INDEX, LV_PART_MAIN);
        etx_border_color(obj, COLOR_THEME_WARNING_INDEX, LV_PART_MAIN);
        break;
      default:
        break;
    }
  });
  label.with([&](lv_obj_t* obj) {
    if (role == ButtonRole::Primary) etx_font(obj, FONT_BOLD_INDEX);
    lv_obj_center(obj);
  });
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

Dialog::Dialog(const char* title) : BaseDialog(title, true)
{
  form.with([&](Window& f) {
    f.withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_style_pad_all(obj, kSpace4, LV_PART_MAIN);
      lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
    });
  });
}

StaticText* Dialog::body(const char* text, TextRole role)
{
  StaticText* line = nullptr;
  form.with([&](Window& f) {
    line = new (std::nothrow)
        StaticText(&f, rect_t{0, 0, LV_PCT(100), 0}, text ? text : "",
                   roleColor(role), roleTextFlags(role));
    if (line) line->setLongMode(LV_LABEL_LONG_WRAP);
  });
  return line;
}

DSButton* Dialog::action(const char* text, ButtonRole role,
                         std::function<void()> onPress)
{
  DSButton* btn = nullptr;
  form.with([&](Window& f) {
    if (!actionsRow) {
      actionsRow = new (std::nothrow)
          Window(&f, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT});
      if (actionsRow) {
        actionsRow->withLive([](Window::LiveWindow& live) {
          lv_obj_t* obj = live.lvobj();
          lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
          lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                                LV_FLEX_ALIGN_CENTER);
          lv_obj_set_style_pad_column(obj, kSpace3, LV_PART_MAIN);
          lv_obj_set_style_pad_top(obj, kSpace2, LV_PART_MAIN);
        });
      }
    }
    if (!actionsRow) return;
    btn = new (std::nothrow)
        DSButton(actionsRow, text, role, [this, onPress]() -> uint8_t {
          if (onPress) onPress();
          deleteLater();
          return 0;
        });
  });
  return btn;
}

// ---------------------------------------------------------------------------
// PickerOverlay
// ---------------------------------------------------------------------------

PickerOverlay::PickerOverlay(const char* title) : BaseDialog(title, true)
{
  form.with([&](Window& f) {
    f.withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_style_pad_all(obj, kSpace2, LV_PART_MAIN);
      lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
    });
  });
}

void PickerOverlay::section(const char* label)
{
  form.with([&](Window& f) { new (std::nothrow) SectionHeader(&f, label); });
}

ListRow* PickerOverlay::option(const ListRow::Content& content,
                               std::function<void()> onSelect)
{
  ListRow* row = nullptr;
  form.with([&](Window& f) {
    RowSize size = content.subtitle ? RowSize::TwoLine : RowSize::Picker;
    row = new (std::nothrow)
        ListRow(&f, size, content, [this, onSelect]() -> uint8_t {
          if (onSelect)
            onSelect();  // caller decides whether the overlay closes
          else
            deleteLater();
          return 0;
        });
  });
  return row;
}

}  // namespace ds
