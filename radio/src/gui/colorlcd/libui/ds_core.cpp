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

#include <cstring>
#include <new>

#include "etx_lv_theme.h"
#include "static.h"
#include "widget_palette.h"  // D2: validated button fills (leaf utility)

namespace ds {

// ---------------------------------------------------------------------------
// Tokens (private). TX16S: 128.3 PPI → 5 px/mm. Base unit 4 px = 0.8 mm.
// ---------------------------------------------------------------------------

namespace {

constexpr coord_t kSpace1 = LAYOUT_SCALE(4);   // micro
constexpr coord_t kSpace2 = LAYOUT_SCALE(8);   // related
constexpr coord_t kSpace3 = LAYOUT_SCALE(12);  // grouping / margins
constexpr coord_t kSpace4 = LAYOUT_SCALE(16);  // separation / dialog frame
constexpr coord_t kSpace5 = LAYOUT_SCALE(24);  // hero / empty-state breathing

constexpr coord_t kTouchMin = LAYOUT_SCALE(40);   // 8 mm touch floor
constexpr coord_t kEmptyMinHeight = LAYOUT_SCALE(180);  // empty-state hero band
constexpr coord_t kRowOneLine = LAYOUT_SCALE(40);
constexpr coord_t kRowPicker = LAYOUT_SCALE(48);
constexpr coord_t kRowTwoLine = LAYOUT_SCALE(52);
constexpr coord_t kLeadingSlotW = LAYOUT_SCALE(40);
constexpr coord_t kButtonMinW = LAYOUT_SCALE(96);
constexpr coord_t kFlagBorder = 3;  // non-color structural cue

// A DS row carries its own adjacency into whatever container it lands in.
//
// ds::List owns the inter-row gap, which is correct -- until a screen adds DS
// rows STRAIGHT to a page body instead. That body is a plain flex column with
// no DS spacing of its own, so the rows stack with a gap of exactly zero.
// FormRow hides this (its 32 px control sits inset in a 40 px row, so ~8 px of
// whitespace shows and reads as a gap), which is why it went unnoticed --
// but FieldGroup draws a bordered box that fills the row, so two of them in a
// row visibly TOUCH. Whether spacing appears at all should not depend on which
// row type a screen happened to use.
//
// So a row lifts its parent to the DS gap on the way in. This is idempotent
// and never shrinks: ds::List and ds::Card already set it, so it is a no-op
// there, and a container that deliberately asks for MORE separation keeps it.
// Restricted to flex parents, since pad_row means something different under a
// grid layout and a DS row is never a grid cell.
void adoptRowGap(Window* parent)
{
  if (!parent) return;
  parent->withLive([](Window::LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    if (lv_obj_get_style_layout(obj, LV_PART_MAIN) != LV_LAYOUT_FLEX) return;
    if (lv_obj_get_style_pad_row(obj, LV_PART_MAIN) < kSpace2)
      lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
  });
}

LcdColorIndex roleColor(TextRole role)
{
  switch (role) {
    case TextRole::Muted:
      return COLOR_THEME_SECONDARY1_INDEX;
    case TextRole::Warning:
      return COLOR_THEME_WARNING_INDEX;
    case TextRole::Active:
      return COLOR_THEME_ACTIVE_INDEX;
    case TextRole::Strong:
      // Explicit (not a Body fall-through): Strong is deliberately the same
      // ink as Body today — the theme palette has no distinct high-contrast
      // "emphasis" text token to alias to without risking AAA contrast on
      // some themes. The BOLD weight (see roleFont) is the reliable
      // live-emphasis cue; giving Strong its own case keeps that a conscious
      // choice instead of an accident of switch fall-through.
      return COLOR_THEME_PRIMARY1_INDEX;
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
    case RowSize::OneLine:
    case RowSize::Compact:
    default:
      return kRowOneLine;  // both one-line variants sit on the 40 px floor
  }
}

coord_t embeddedControlSize() { return kSpace4; }

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

List::List(Window* parent, Density density) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT}),
    density_(density)
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
  adoptRowGap(parent);
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
// Caption
// ---------------------------------------------------------------------------

Caption::Caption(Window* parent, const char* text) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
{
  adoptRowGap(parent);
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_style_pad_left(obj, kSpace3, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, kSpace3, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  });
  text_ = new StaticText(this, rect_t{0, 0, LV_PCT(100), 0}, text ? text : "",
                         COLOR_THEME_SECONDARY1_INDEX);
  text_->withLive([](Window::LiveWindow& live) {
    lv_obj_set_height(live.lvobj(), LV_SIZE_CONTENT);
    lv_obj_clear_flag(live.lvobj(), LV_OBJ_FLAG_CLICKABLE);
  });
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

namespace {
// D1: pick the row variant for a density. No subtitle -> a plain 40 px row;
// with a subtitle, Comfortable is the 52 px two-line row and Compact is the
// 40 px one-line row (subtitle dropped to the editor).
RowSize sizeForDensity(Density density, const ListRow::Content& content)
{
  if (!content.subtitle) return RowSize::OneLine;
  return density == Density::Compact ? RowSize::Compact : RowSize::TwoLine;
}
}  // namespace

ListRow::ListRow(Window* parent, Density density, const Content& content,
                 std::function<uint8_t()> onPress,
                 std::function<uint8_t()> onLongPress) :
    ListRow(parent, sizeForDensity(density, content), content,
            std::move(onPress), std::move(onLongPress))
{
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
// FormRow
// ---------------------------------------------------------------------------

namespace {
// 40% label / 60% control, one content-height row, vertically centered.
const lv_coord_t kFormCols[] = {LV_GRID_FR(2), LV_GRID_FR(3),
                                LV_GRID_TEMPLATE_LAST};
// FR(1), not GRID_CONTENT: the track must fill the row's full height so a
// control centred in it is centred in the whole 40 px row. With GRID_CONTENT
// the track shrank to the control's own 32 px and sat at the TOP, leaving 8 px
// of dead space below and pinning the control flush to the row's top edge --
// which quietly halved the click-box expansion, since LVGL never hit-tests a
// child outside its parent's coords (lv_indev_search_obj), so the 4 px added
// above the control was unreachable and only the 4 px below counted. The row
// still sizes to content overall (LV_SIZE_CONTENT + a kTouchMin floor), so a
// taller control or a wrapped label still grows it.
const lv_coord_t kFormRows[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

// In a FormRow/FieldCell the CONTROL is the tap target -- the row sets NO_FOCUS
// on itself so the label area is inert. But the standard control height is
// EdgeTxStyles::UI_ELEMENT_HEIGHT (32 px), below the kTouchMin 8 mm floor, so
// every toggle, choice and number field in a settings form was a sub-floor
// target while the row around it merely *reserved* 40 px of space.
//
// The fix grows the control's CLICK box, not its drawn size: inflating the
// widget itself would make every switch and dropdown visibly taller than the
// design calls for, for a reason that has nothing to do with how it should
// look. The expansion is exactly what closes the gap -- (40 - 32) / 2 = 4 px a
// side -- which means it stops precisely at the row's own 40 px min-height and
// can never bleed into the neighbouring row (rows are kSpace2 = 8 px apart).
// Controls already at or above the floor are unaffected in practice: the extra
// 4 px still lands inside the row that reserved the space for them.
void expandControlToTouchFloor(lv_obj_t* obj)
{
  // Round the half-difference UP. Both terms are LAYOUT_SCALE'd, and on the
  // >=800 px layout they scale to 55 and 44 -- an ODD difference of 11, which
  // integer division truncates to 5, leaving the box at 54 and one pixel SHORT
  // of the floor it is supposed to guarantee. Rounding up costs a pixel only
  // when the difference is odd, and a floor that is missed by one pixel on one
  // display size is not a floor.
  constexpr coord_t pad = (kTouchMin - EdgeTxStyles::UI_ELEMENT_HEIGHT + 1) / 2;
  if (pad > 0) lv_obj_set_ext_click_area(obj, pad);
}
}  // namespace

FormRow::FormRow(Window* parent, const char* label,
                 std::function<void(Window*)> buildControl) :
    Window(parent, rect_t{0, 0, LV_PCT(100), rowHeight(RowSize::OneLine)})
{
  setWindowFlag(NO_FOCUS);  // the control is the focus target, not the row
  adoptRowGap(parent);
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, kSpace3, LV_PART_MAIN);
    lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj, kFormCols, kFormRows);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    labelObj = etx_label_create(obj, FONT_STD_INDEX);
    if (labelObj) {
      lv_label_set_text(labelObj, label ? label : "");
      lv_label_set_long_mode(labelObj, LV_LABEL_LONG_DOT);
      etx_txt_color(labelObj, COLOR_THEME_PRIMARY1_INDEX);
      lv_obj_set_grid_cell(labelObj, LV_GRID_ALIGN_START, 0, 1,
                           LV_GRID_ALIGN_CENTER, 0, 1);
    }
  });

  if (buildControl) buildControl(this);

#if defined(SIMU)
  setAutomationText(label ? label : "");
#endif
}

bool FormRow::addChild(Window* child)
{
  if (!Window::addChild(child)) return false;
  // The field widget the caller builds lands in the 60% control column,
  // vertically centered. (The label is a raw lv label, not a Window child, so
  // the first — and only — Window child is the control.)
  child->withLive([](Window::LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER,
                         0, 1);
    // Same min-width clamp FieldRow's cells already applied. Without it a
    // control that sizes to its own content -- an icon-only picker such as the
    // Model image folder button, 28 px wide -- stayed narrower than the floor,
    // and the click-box expansion below could not rescue it: that expansion is
    // derived from the HEIGHT shortfall, so it added 4 px a side and left the
    // button 36 px wide. Clamping the width makes the layout reserve the space
    // rather than papering over it with an oversized hit box, so the control
    // also LOOKS as big as it is tappable.
    lv_obj_set_style_min_width(obj, kTouchMin, LV_PART_MAIN);
    expandControlToTouchFloor(obj);
  });
  return true;
}

void FormRow::setLabel(const char* text)
{
  if (labelObj) lv_label_set_text(labelObj, text ? text : "");
}

// ---------------------------------------------------------------------------
// FieldRow
// ---------------------------------------------------------------------------

namespace {

// One field sub-cell: the SAME 40% label / 60% control split as FormRow (it
// reuses kFormCols), in a container that occupies one even top-level column.
// Reusing kFormCols keeps a field's label/control x-aligned with a plain
// FormRow, and its control-slot column clamped so the control clears the touch
// floor. The one Window child the caller builds lands in the 60% control
// column, exactly like FormRow.
class FieldCell : public Window
{
 public:
  FieldCell(Window* parent, const char* label) :
      Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
  {
    setWindowFlag(NO_FOCUS);  // the control is the focus target, not the cell
    withLive([&](LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_width(obj, LV_PCT(100));
      lv_obj_set_height(obj, LV_SIZE_CONTENT);
      lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
      lv_obj_set_style_min_width(obj, kTouchMin, LV_PART_MAIN);
      lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_column(obj, kSpace2, LV_PART_MAIN);
      lv_obj_set_layout(obj, LV_LAYOUT_GRID);
      lv_obj_set_grid_dsc_array(obj, kFormCols, kFormRows);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

      labelObj = etx_label_create(obj, FONT_STD_INDEX);
      if (labelObj) {
        lv_label_set_text(labelObj, label ? label : "");
        lv_label_set_long_mode(labelObj, LV_LABEL_LONG_DOT);
        etx_txt_color(labelObj, COLOR_THEME_PRIMARY1_INDEX);
        lv_obj_set_grid_cell(labelObj, LV_GRID_ALIGN_START, 0, 1,
                             LV_GRID_ALIGN_CENTER, 0, 1);
      }
    });
  }

  lv_obj_t* label() const { return labelObj; }

 protected:
  bool addChild(Window* child) override
  {
    if (!Window::addChild(child)) return false;
    // The field widget lands in the 60% control column, vertically centered,
    // and is clamped to the 40 px touch floor so several fields on one line
    // never shrink a control below a tappable width. (The label is a raw lv
    // label, not a Window child, so the first — and only — Window child is the
    // control.)
    child->withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER,
                           0, 1);
      lv_obj_set_style_min_width(obj, kTouchMin, LV_PART_MAIN);
      expandControlToTouchFloor(obj);
    });
    return true;
  }

 private:
  lv_obj_t* labelObj = nullptr;
};

}  // namespace

FieldRow::FieldRow(Window* parent, const std::vector<Field>& fields) :
    Window(parent, rect_t{0, 0, LV_PCT(100), rowHeight(RowSize::OneLine)})
{
  setWindowFlag(NO_FOCUS);  // the controls are the focus targets, not the row
  adoptRowGap(parent);

  const int n = (int)fields.size();

  // Even top-level columns: N equal fractions built once and handed by
  // reference to LVGL, so field i lands at the same x — and spans the same
  // width — in every FieldRow with N fields, independent of contents.
  colDsc_.reserve(n + 1);
  for (int i = 0; i < n; ++i) colDsc_.push_back(LV_GRID_FR(1));
  colDsc_.push_back(LV_GRID_TEMPLATE_LAST);

  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, kSpace3, LV_PART_MAIN);  // between fields
    lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj, colDsc_.data(), kFormRows);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  });

  cells_.reserve(n);
  labels_.reserve(n);
  highlightReady_.assign(n, false);

  int col = 0;
  for (const auto& f : fields) {
    auto* cell = new (std::nothrow) FieldCell(this, f.label);
    cells_.push_back(cell);
    labels_.push_back(cell ? cell->label() : nullptr);
    if (cell) {
      cell->withLive([col](Window::LiveWindow& live) {
        // STRETCH to fill the (even) column track, so every field container is
        // the same width and its internal 40/60 split resolves identically.
        lv_obj_set_grid_cell(live.lvobj(), LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_CENTER, 0, 1);
      });
      if (f.buildControl) f.buildControl(cell);
    }
    ++col;
  }

#if defined(SIMU)
  setAutomationText(n > 0 && fields.begin()->label ? fields.begin()->label
                                                   : "");
#endif
}

void FieldRow::highlightField(int index, bool on)
{
  if (index < 0 || index >= (int)labels_.size()) return;
  lv_obj_t* label = labels_[index];
  if (!label) return;
  if (on) {
    // Install the active-fill + bold emphasis the first time this field is
    // highlighted (a CHECKED-state style, exactly the treatment the legacy
    // Min/Max range-end cue applied to its label), then latch the state on.
    if (!highlightReady_[index]) {
      etx_solid_bg(label, COLOR_THEME_ACTIVE_INDEX, LV_STATE_CHECKED);
      etx_font(label, FONT_BOLD_INDEX, LV_STATE_CHECKED);
      highlightReady_[index] = true;
    }
    lv_obj_add_state(label, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(label, LV_STATE_CHECKED);
  }
}

// ---------------------------------------------------------------------------
// FieldGroup
// ---------------------------------------------------------------------------

namespace {

// The wrapping control area of a FieldGroup: a ROW-flow box that WRAPS to the
// next line, owning the inter-control gap (space-2) and wrapped-line gap
// (space-1). Every control the caller flows into it is clamped to the 40 px
// touch floor (min width AND height) so a small toggle or narrow edit stays a
// tappable target even when several share the line. This is the DS-owned
// replacement for the hand-rolled `Window` + `padAll` + `setFlexLayout(ROW)`
// box the module option fields used.
class WrapContent : public Window
{
 public:
  explicit WrapContent(Window* parent) :
      Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
  {
    setWindowFlag(NO_FOCUS);  // the flowed controls are the focus targets
    withLive([&](LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_width(obj, LV_PCT(100));  // fill its (stretched) column cell
      lv_obj_set_height(obj, LV_SIZE_CONTENT);
      lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
      lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_column(obj, kSpace2, LV_PART_MAIN);  // between controls
      lv_obj_set_style_pad_row(obj, kSpace1, LV_PART_MAIN);     // wrapped lines
      lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW_WRAP);
      lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_START);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    });
  }

 protected:
  bool addChild(Window* child) override
  {
    if (!Window::addChild(child)) return false;
    // Touch floor: every flowed control taps as a >= 40x40 px target, so a bare
    // toggle or a narrow edit is never shrunk below a tappable size by sharing
    // the line with its neighbours.
    child->withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_style_min_width(obj, kTouchMin, LV_PART_MAIN);
      lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
    });
    return true;
  }
};

}  // namespace

FieldGroup::FieldGroup(Window* parent, const char* label,
                       std::function<void(Window*)> buildContent) :
    Window(parent, rect_t{0, 0, LV_PCT(100), rowHeight(RowSize::OneLine)})
{
  setWindowFlag(NO_FOCUS);  // the controls are the focus targets, not the row
  adoptRowGap(parent);

  const bool hasLabel = label && label[0];

  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(obj, kTouchMin, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, kSpace3, LV_PART_MAIN);
    lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    // Same 40/60 template as FormRow, so a FieldGroup's label sits at the same x
    // as the FormRows stacked around it. When there is no label the control area
    // spans both columns for the full width.
    lv_obj_set_grid_dsc_array(obj, kFormCols, kFormRows);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    if (hasLabel) {
      labelObj = etx_label_create(obj, FONT_STD_INDEX);
      if (labelObj) {
        lv_label_set_text(labelObj, label);
        lv_label_set_long_mode(labelObj, LV_LABEL_LONG_DOT);
        etx_txt_color(labelObj, COLOR_THEME_PRIMARY1_INDEX);
        lv_obj_set_grid_cell(labelObj, LV_GRID_ALIGN_START, 0, 1,
                             LV_GRID_ALIGN_CENTER, 0, 1);
      }
    }
  });

  // The wrapping control area is the single direct Window child; its grid
  // placement is set in addChild (col 1 when labelled, else spanning both).
  content_ = new (std::nothrow) WrapContent(this);

  if (buildContent && content_) buildContent(content_);

#if defined(SIMU)
  setAutomationText(hasLabel ? label : "");
#endif
}

bool FieldGroup::addChild(Window* child)
{
  if (!Window::addChild(child)) return false;
  // The content area (the ONLY direct Window child — the caller's controls are
  // children of content_, not of the row) lands in the 60% control column, or
  // spans the full width when the leading label is omitted.
  const bool labelled = labelObj != nullptr;
  child->withLive([labelled](Window::LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    if (labelled)
      lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 1, 1,
                           LV_GRID_ALIGN_CENTER, 0, 1);
    else
      lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 2,
                           LV_GRID_ALIGN_CENTER, 0, 1);
  });
  return true;
}

void FieldGroup::setLabel(const char* text)
{
  if (labelObj) lv_label_set_text(labelObj, text ? text : "");
}

// ---------------------------------------------------------------------------
// Card
// ---------------------------------------------------------------------------

Card::Card(Window* parent, const char* title, bool bordered) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
{
  setWindowFlag(NO_FOCUS);
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);   // internal gap
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    if (bordered) {
      // Genuinely separated card: subtle border + surface, inset from its
      // neighbours so the grouping reads as a distinct raised panel.
      lv_obj_set_style_pad_all(obj, kSpace3, LV_PART_MAIN);  // grouping inset
      lv_obj_set_style_radius(obj, kSpace1, LV_PART_MAIN);
      lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
      etx_border_color(obj, COLOR_THEME_SECONDARY2_INDEX, LV_PART_MAIN);
      etx_solid_bg(obj, COLOR_THEME_PRIMARY2_INDEX, LV_PART_MAIN);  // surface
    } else {
      // Form/settings container (the common case): no border, no surface and no
      // inset — the enclosing ds::List already owns the page margins, so the
      // grouped rows sit flush with the rest of the page. There is nothing here
      // to separate a border from.
      lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    }

    if (title) {
      titleObj = etx_label_create(obj, FONT_BOLD_INDEX);
      if (titleObj) {
        lv_label_set_text(titleObj, title);
        etx_txt_color(titleObj, COLOR_THEME_PRIMARY1_INDEX);
      }
    }
  });
}

// ---------------------------------------------------------------------------
// EmptyState
// ---------------------------------------------------------------------------

EmptyState::EmptyState(Window* parent, EdgeTxIcon icon, const char* headline,
                       const char* hint, const char* actionLabel,
                       std::function<uint8_t()> onAction) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_SIZE_CONTENT})
{
  setWindowFlag(NO_FOCUS);
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_flex_grow(obj, 1);  // fill remaining height when the body allows
    // ...but a page body is often content-sized, so also claim a hero minimum
    // height (below the 227 px content band) so the state is always visible and
    // its content stays vertically centered even when there is nothing to grow
    // into.
    lv_obj_set_style_min_height(obj, kEmptyMinHeight, LV_PART_MAIN);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(obj, kSpace5, LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  });

  new (std::nothrow) StaticIcon(this, 0, 0, icon, COLOR_THEME_SECONDARY1_INDEX);

  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_t* h = etx_label_create(obj, FONT_BOLD_INDEX);
    if (h) {
      lv_label_set_text(h, headline ? headline : "");
      lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      etx_txt_color(h, COLOR_THEME_PRIMARY1_INDEX);
    }
    if (hint) {
      lv_obj_t* sub = etx_label_create(obj, FONT_STD_INDEX);
      if (sub) {
        lv_label_set_text(sub, hint);
        lv_obj_set_width(sub, LV_PCT(90));
        lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        etx_txt_color(sub, COLOR_THEME_SECONDARY1_INDEX);
      }
    }
  });

  if (actionLabel && onAction)
    new (std::nothrow)
        DSButton(this, actionLabel, ButtonRole::Primary, std::move(onAction));

#if defined(SIMU)
  setAutomationText(headline ? headline : "");
#endif
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------

namespace {

constexpr coord_t kGridSideMargin = kSpace2;   // list side margin
constexpr coord_t kGridColGap = kSpace1;       // between columns
constexpr coord_t kGridRowGap = kSpace1;       // between data rows
constexpr coord_t kGridHeaderH = LAYOUT_SCALE(28);  // caption band
constexpr coord_t kGridContentW = LCD_W;       // width budget for auto-fit

// One filling row so cells vertically center in the fixed row height.
const lv_coord_t kGridRowDsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

}  // namespace

Grid::Grid(Window* parent, const std::vector<Column>& columns) :
    Window(parent, rect_t{0, 0, LV_PCT(100), LV_PCT(100)})
{
  colGap_ = kGridColGap;
  sideMargin_ = kGridSideMargin;
  headerH_ = kGridHeaderH;
  rowH_ = kTouchMin;  // data rows sit on the 40 px touch floor

  const int n = (int)columns.size();

  // Pass 1: fixed widths, count fill columns.
  coord_t fixedSum = 0;
  int nFill = 0;
  for (const auto& c : columns) {
    if (c.kind == Column::Kind::Fill) {
      ++nFill;
    } else {
      coord_t w = LAYOUT_SCALE(c.size);
      if (c.interactive && w < kTouchMin) w = kTouchMin;
      fixedSum += w;
    }
  }

  const coord_t gapsTotal = n > 1 ? (n - 1) * colGap_ : 0;
  const coord_t inner = kGridContentW - 2 * sideMargin_;
  coord_t fillAvail = inner - fixedSum - gapsTotal;
  if (fillAvail < 0) fillAvail = 0;
  const coord_t fillShare = nFill > 0 ? fillAvail / nFill : 0;

  // Pass 2: materialize the immutable column template (fixed px throughout —
  // this is what makes column i land at the same x in the header and in every
  // row, independent of any row's contents).
  colDsc_.reserve(n + 1);
  aligns_.reserve(n);
  frozen_.reserve(n);
  coord_t colsW = 0;
  for (const auto& c : columns) {
    coord_t w;
    if (c.kind == Column::Kind::Fill) {
      coord_t minW = LAYOUT_SCALE(c.size);
      if (c.interactive && minW < kTouchMin) minW = kTouchMin;
      w = fillShare > minW ? fillShare : minW;
    } else {
      w = LAYOUT_SCALE(c.size);
      if (c.interactive && w < kTouchMin) w = kTouchMin;
    }
    colDsc_.push_back(w);
    aligns_.push_back(c.align);
    frozen_.push_back(c.frozen);
    colsW += w;
  }
  colDsc_.push_back(LV_GRID_TEMPLATE_LAST);
  totalW_ = colsW + gapsTotal + 2 * sideMargin_;

  // Grid = a fixed vertical stack: the sticky header row, then a scrolling body
  // holding the data rows. The header is a separate flex slot ABOVE the body,
  // so it simply never scrolls vertically -- a genuine sticky/frozen header.
  // Both the header and every data row realize into the SAME shared column
  // template (colDsc_), which is what aligns column i across all of them.
  withLive([&](LiveWindow& live) {
    lv_obj_t* obj = live.lvobj();
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  });

  body_ = new (std::nothrow) Window(this, rect_t{0, 0, LV_PCT(100), 0});
  if (body_) {
    body_->setWindowFlag(NO_FOCUS);
    body_->withLive([&](LiveWindow& live) {
      lv_obj_t* b = live.lvobj();
      lv_obj_set_width(b, LV_PCT(100));
      lv_obj_set_flex_grow(b, 1);
      lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_left(b, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_right(b, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_top(b, kGridRowGap, LV_PART_MAIN);
      lv_obj_set_style_pad_bottom(b, kSpace2, LV_PART_MAIN);
      lv_obj_set_style_pad_row(b, kGridRowGap, LV_PART_MAIN);
      lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
      lv_obj_set_scroll_dir(b, LV_DIR_VER);  // columns fit width on TX16S
      lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    });
  }
}

coord_t Grid::columnWidth(int col) const
{
  if (col < 0 || col >= (int)aligns_.size()) return 0;
  return colDsc_[col];
}

Grid::Row* Grid::header()
{
  if (headerRow_) return headerRow_;
  headerRow_ = new (std::nothrow) Row(this, /*header=*/true);
  return headerRow_;
}

Grid::Row* Grid::addRow(std::function<uint8_t()> onPress,
                        std::function<uint8_t()> onLongPress)
{
  return new (std::nothrow)
      Row(this, /*header=*/false, std::move(onPress), std::move(onLongPress));
}

Grid::Row* Grid::addRow(Row* row) { return row; }

// ---- Grid::Row ----

Grid::Row::Row(Grid* grid, bool header, std::function<uint8_t()> onPress,
               std::function<uint8_t()> onLongPress) :
    Button(header ? (Window*)grid : grid->body_,
           rect_t{0, 0, grid->totalW_,
                  header ? grid->headerH_ : grid->dataRowHeight()}),
    grid_(grid),
    isHeader_(header)
{
  cells_.assign(grid->columnCount(), nullptr);
  cellFont_.assign(grid->columnCount(), 0xFF);
  cellColor_.assign(grid->columnCount(), 0xFF);
  highlightBg_.assign(grid->columnCount(), false);

  setWidth(grid->totalW_);
  setHeight(header ? grid->headerH_ : grid->dataRowHeight());

  withLive([&](LiveWindow& live) {
    rowObj_ = live.lvobj();
    lv_obj_set_layout(rowObj_, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(rowObj_, grid->colDsc_.data(), kGridRowDsc);
    lv_obj_set_style_pad_top(rowObj_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(rowObj_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(rowObj_, grid->sideMargin_, LV_PART_MAIN);
    lv_obj_set_style_pad_right(rowObj_, grid->sideMargin_, LV_PART_MAIN);
    lv_obj_set_style_pad_column(rowObj_, grid->colGap_, LV_PART_MAIN);
    lv_obj_set_style_min_height(rowObj_, isHeader_ ? grid->headerH_ : kTouchMin,
                                LV_PART_MAIN);
    lv_obj_set_style_border_width(rowObj_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rowObj_, LV_OBJ_FLAG_SCROLLABLE);

    if (isHeader_) {
      etx_solid_bg(rowObj_, COLOR_THEME_SECONDARY3_INDEX, LV_PART_MAIN);
      lv_obj_move_to_index(rowObj_, 0);  // flex slot above the scrolling body
    }
  });

  if (isHeader_) {
    grid->headerRow_ = this;  // register the sticky header
    setWindowFlag(NO_FOCUS);
    withLive([&](LiveWindow& live) {
      lv_obj_clear_flag(live.lvobj(), LV_OBJ_FLAG_CLICKABLE);
    });
  } else {
    if (onPress) setPressHandler(std::move(onPress));
    if (onLongPress) setLongPressHandler(std::move(onLongPress));
  }
}

lv_obj_t* Grid::Row::ensureCell(int col, TextRole role)
{
  if (col < 0 || col >= (int)cells_.size() || !rowObj_) return nullptr;
  if (cells_[col]) return cells_[col];
  lv_obj_t* label =
      etx_label_create(rowObj_, isHeader_ ? FONT_STD_INDEX : roleFont(role));
  if (!label) return nullptr;
  // The cell STRETCHES to fill its whole column track (vertically centered).
  // This is what makes column i land at the same x — and span the same width —
  // in the header and in every row, independent of each cell's text: cell.x1 ==
  // track left for all, so alignment is structural, not content-luck. It also
  // means an `interactive` column's cell is as wide as its (>= 40 px) track, so
  // a per-cell tap target clears the touch floor. Text is positioned WITHIN the
  // filled cell by the column's alignment.
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, col, 1,
                       LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_style_text_align(
      label,
      grid_->columnAlign(col) == CellAlign::End
          ? LV_TEXT_ALIGN_RIGHT
          : (grid_->columnAlign(col) == CellAlign::Center ? LV_TEXT_ALIGN_CENTER
                                                          : LV_TEXT_ALIGN_LEFT),
      LV_PART_MAIN);
  lv_label_set_text(label, "");
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  etx_txt_color(label, roleColor(role));
  // The active-flight-mode highlight (a CHECKED-state background) is installed
  // lazily by highlightCell(), not here: only a handful of cells are ever
  // highlighted, and etx_solid_bg() is expensive (it sweeps every shared
  // bg-colour style off the object), so paying it once per cell at build was
  // pure load-time overhead on a full GV x FM grid.
  // Record the font/colour this label was born with so the setCell/setCellSmall
  // that follows (with the same role) skips a redundant second styling pass.
  cellFont_[col] = (uint8_t)(isHeader_ ? FONT_STD_INDEX : roleFont(role));
  cellColor_[col] = (uint8_t)roleColor(role);
  cells_[col] = label;
  return label;
}

void Grid::Row::applyCellStyle(int col, lv_obj_t* label, uint8_t font,
                               uint8_t color)
{
  // etx_font()/etx_txt_color() are not cheap — each removes every shared
  // font/colour style from the object before adding the new one. A cell that
  // toggles e.g. Body <-> Strong across refreshes (the logical-switches "active
  // operand" live cue) must have its rendered weight track the role, so we
  // still re-apply whenever the resolved font/colour actually changes; but the
  // common cases — the second styling pass every cell would take right after
  // ensureCell() at build, and a text-only value refresh — become no-ops.
  // Skipping them is invisible: the identical style is already on the object.
  if (cellFont_[col] != font) {
    etx_font(label, (FontIndex)font, LV_PART_MAIN);
    cellFont_[col] = font;
  }
  if (cellColor_[col] != color) {
    etx_txt_color(label, (LcdColorIndex)color);
    cellColor_[col] = color;
  }
}

void Grid::Row::setCell(int col, const char* text, TextRole role)
{
  lv_obj_t* label = ensureCell(col, role);
  if (label) {
    lv_label_set_text(label, text ? text : "");
    applyCellStyle(col, label, isHeader_ ? FONT_STD_INDEX : roleFont(role),
                   roleColor(role));
  }
}

void Grid::Row::setCellSmall(int col, const char* text, bool small,
                             TextRole role)
{
  lv_obj_t* label = ensureCell(col, role);
  if (label) {
    lv_label_set_text(label, text ? text : "");
    // Strong wins over `small`: a live bold cue (e.g. the logical-switch
    // "currently matches" operand highlight, which drives V1 through this
    // small-font path) must still render bold even when the column also
    // wants the small font to avoid overflow — otherwise Strong routed
    // through setCellSmall would never be visible as bold.
    FontIndex font;
    if (isHeader_)
      font = FONT_STD_INDEX;
    else if (role == TextRole::Strong)
      font = roleFont(role);  // bold
    else
      font = small ? FONT_XS_INDEX : FONT_STD_INDEX;
    applyCellStyle(col, label, font, roleColor(role));
  }
}

void Grid::Row::highlightCell(int col, bool on)
{
  lv_obj_t* label = ensureCell(col, TextRole::Body);
  if (!label) return;
  if (on) {
    // Install the highlight background the first time this cell is highlighted
    // (see ensureCell): a pure background change on the CHECKED state, exactly
    // as the original did — the ink stays put so the highlighted cell reads as
    // the same value, boxed.
    if (!highlightBg_[col]) {
      etx_solid_bg(label, COLOR_THEME_ACTIVE_INDEX, LV_STATE_CHECKED);
      highlightBg_[col] = true;
    }
    lv_obj_add_state(label, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(label, LV_STATE_CHECKED);
  }
}

void Grid::Row::setSpanningCell(const char* text, TextRole role)
{
  if (!rowObj_ || cells_.empty()) return;
  if (!cells_[0]) {
    lv_obj_t* label = etx_label_create(rowObj_, roleFont(role));
    if (!label) return;
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, grid_->columnCount(),
                         LV_GRID_ALIGN_CENTER, 0, 1);
    etx_txt_color(label, roleColor(role));
    cells_[0] = label;
  }
  lv_label_set_text(cells_[0], text ? text : "");
}

lv_obj_t* Grid::Row::cellObj(int col)
{
  if (col < 0 || col >= (int)cells_.size()) return nullptr;
  return cells_[col];
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

    // D2: filled roles draw a contrast-VALIDATED fill (widget_palette) so the
    // fixed label ink stays legible on the fill in every theme, not the raw
    // theme accent which fails AAA on some themes (e.g. red WARNING under a
    // white label is only ~5:1). The label colour here is exactly the one the
    // fill was validated against.
    switch (role) {
      case ButtonRole::Primary: {
        lv_color_t fill = buttonFillLvColor(BTN_FILL_PRIMARY);
        lv_obj_set_style_bg_color(obj, fill, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, fill, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
        etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX, LV_PART_MAIN);
        etx_txt_color(obj, COLOR_THEME_PRIMARY1_INDEX,
                      LV_PART_MAIN | LV_STATE_FOCUSED);
        break;
      }
      case ButtonRole::Destructive: {
        lv_color_t fill = buttonFillLvColor(BTN_FILL_DESTRUCTIVE);
        lv_obj_set_style_bg_color(obj, fill, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, fill, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
        etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX, LV_PART_MAIN);
        etx_txt_color(obj, COLOR_THEME_PRIMARY2_INDEX,
                      LV_PART_MAIN | LV_STATE_FOCUSED);
        break;
      }
      default:
        break;
    }
  });
  label.with([&](lv_obj_t* obj) {
    if (role != ButtonRole::Secondary) etx_font(obj, FONT_BOLD_INDEX);
    lv_obj_center(obj);
  });
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

Dialog::Dialog(const char* title, bool closeIfClickedOutside) :
    BaseDialog(title, closeIfClickedOutside)
{
  form.with([&](Window& f) {
    f.withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_style_pad_all(obj, kSpace4, LV_PART_MAIN);
      lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
    });
  });
}

StaticText* Dialog::body(const char* text, TextRole role, bool centered)
{
  StaticText* line = nullptr;
  form.with([&](Window& f) {
    line = new (std::nothrow)
        StaticText(&f, rect_t{0, 0, LV_PCT(100), 0}, text ? text : "",
                   roleColor(role),
                   roleTextFlags(role) | (centered ? CENTERED : 0));
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
          // Centered, not right-aligned. Right-aligned action rows are the
          // desktop convention, and both of its premises fail here: a desktop
          // dialog is wide with left-aligned prose, so the eye naturally ends
          // bottom-right. Every dialog in this app centers its body text
          // instead, which left a compact 480 px alert with centered prose
          // above buttons jammed against the right edge and a wide empty gap
          // on the left. Centering also stops biasing every action toward the
          // right thumb on a radio held in two hands. Order is unchanged --
          // the affirmative action is still added last and so still sits on
          // the right, which is consistent everywhere else in the UI.
          lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
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
  title_ = title;
  form.with([&](Window& f) {
    f.withLive([](Window::LiveWindow& live) {
      lv_obj_t* obj = live.lvobj();
      lv_obj_set_style_pad_all(obj, kSpace2, LV_PART_MAIN);
      lv_obj_set_style_pad_row(obj, kSpace2, LV_PART_MAIN);
    });
  });
}

namespace {

// Lower-cases into a fixed buffer and folds one trailing English plural
// (naive — good enough for the short caption-style words section headers
// and titles are made of): "...ies" -> "...y" (battery/batteries,
// category/categories), else a bare trailing 's' is dropped
// (role/roles). Not a general stemmer -- just enough to keep the DS
// title-echo check from being defeated by pluralizing the header.
void foldForCompare(const char* text, char* out, size_t outSize)
{
  size_t n = 0;
  if (text) {
    for (; text[n] && n + 1 < outSize; ++n) {
      char c = text[n];
      if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
      out[n] = c;
    }
  }
  // Trim trailing whitespace.
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) --n;
  // Naive plural fold.
  if (n > 3 && out[n - 3] == 'i' && out[n - 2] == 'e' && out[n - 1] == 's') {
    out[n - 3] = 'y';  // "...ies" -> "...y"
    n -= 2;
  } else if (n > 1 && out[n - 1] == 's') {
    --n;  // "...s" -> "..."
  }
  out[n] = '\0';
}

}  // namespace

bool PickerOverlay::isTitleEcho(const char* label) const
{
  if (!label || !title_) return false;
  char a[64];
  char b[64];
  foldForCompare(label, a, sizeof(a));
  foldForCompare(title_, b, sizeof(b));
  return a[0] != '\0' && strcmp(a, b) == 0;
}

void PickerOverlay::addSectionHeader(const char* label, bool moveToFront)
{
  if (isTitleEcho(label)) return;  // restates the title — adds nothing
  form.with([&](Window& f) {
    auto* header = new (std::nothrow) SectionHeader(&f, label);
    if (!header) return;
#if defined(SIMU)
    ++materializedHeaderCount_;
#endif
    if (moveToFront) {
      header->withLive([](Window::LiveWindow& live) {
        lv_obj_move_to_index(live.lvobj(), 0);
      });
    }
  });
}

void PickerOverlay::section(const char* label)
{
  ++sectionCount_;
  if (sectionCount_ == 1) {
    // Defer: this may turn out to be the only section, in which case a
    // header would just restate the overlay title above its one group.
    // Own a copy -- materializing it is deferred past this call, so the
    // caller's pointer (possibly a formatted stack buffer) is not required
    // to stay valid that long.
    strncpy(pendingSectionLabel_, label ? label : "",
            sizeof(pendingSectionLabel_) - 1);
    pendingSectionLabel_[sizeof(pendingSectionLabel_) - 1] = '\0';
    return;
  }
  if (sectionCount_ == 2) {
    // A second section proves section #1 needs differentiating after all —
    // realize its header now, ahead of the rows it already collected.
    addSectionHeader(pendingSectionLabel_, /*moveToFront=*/true);
  }
  addSectionHeader(label, /*moveToFront=*/false);
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
