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

// EdgeTX color-LCD design system (DS) — see DESIGN_SYSTEM.md.
//
// This is the ONLY layer (together with etx_lv_theme.cpp) allowed to touch
// raw LVGL spacing/position styling. Screens compose these components and
// express intent through semantic parameters exclusively: content, roles and
// callbacks. There are NO pixel, padding, margin or position parameters on
// purpose — spacing, sizing and touch-target floors (40 px ≈ 8 mm on TX16S)
// are owned and mechanically clamped here.

#pragma once

#include <functional>

#include "button.h"
#include "dialog.h"
#include "window.h"

class StaticText;

namespace ds {

// ---------------------------------------------------------------------------
// Semantic vocabulary — the only "styling" a screen may express.
// ---------------------------------------------------------------------------

enum class TextRole : uint8_t {
  Body,     // primary ink, standard font
  Strong,   // primary ink, bold — row titles, emphasis
  Muted,    // secondary ink — labels/annotations only
  Warning,  // needs attention
  Active,   // positive/engaged accent
};

enum class ButtonRole : uint8_t {
  Primary,      // the one main action (filled)
  Secondary,    // neutral action
  Destructive,  // dangerous action (warning-colored)
};

enum class RowSize : uint8_t {
  OneLine,  // 40 px — title (+ trailing)
  Picker,   // 48 px — comfortable single-line pick target
  TwoLine,  // 52 px — title + subtitle (+ trailing)
};

// DS-owned row height for the given variant. Exposed so legacy list
// machinery (ListLineButton subclasses) can size rows without inventing
// PAD_* arithmetic during migration.
coord_t rowHeight(RowSize size);

// DS-owned affordance size for a small icon-style control embedded in a
// row's leadingSlot() (e.g. an enable checkbox). Keeps the scaled-constant
// arithmetic private to the DS layer instead of screens inventing their own
// LAYOUT_*_SCALED constant for a control that only ever appears in a slot.
coord_t embeddedControlSize();

// ---------------------------------------------------------------------------
// List — the scrollable content column of a page body.
// Owns page margins and inter-row gaps (adjacency rule lives here).
// ---------------------------------------------------------------------------

class List : public Window
{
 public:
  explicit List(Window* parent);
};

// ---------------------------------------------------------------------------
// SectionHeader — caption-role divider between row groups.
// ---------------------------------------------------------------------------

class SectionHeader : public Window
{
 public:
  SectionHeader(Window* parent, const char* text);
};

// ---------------------------------------------------------------------------
// RowContent — DS slot layout inside an interactive row:
//   [ leading slot? ][ title / subtitle ........ ][ trailing ]
// Used directly by ListRow, and as the migration bridge for existing
// ListLineButton-based rows (the legacy machinery keeps refresh/focus,
// the DS owns all geometry).
// ---------------------------------------------------------------------------

class RowContent
{
 public:
  RowContent(Window* row, RowSize size);

  void setTitle(const char* text);
  void setSubtitle(const char* text);                  // TwoLine only
  void setTrailing(const char* text, TextRole role);   // right-aligned value

  // Fixed-width, full-height slot for ONE embedded control (e.g. an enable
  // toggle). Created on first call, always at the row's leading edge.
  Window* leadingSlot();

  // Expand an embedded control's hit area to fill its slot (touch floor).
  void expandHitArea(Window* control);

 private:
  lv_obj_t* rowObj = nullptr;
  lv_obj_t* textCol = nullptr;
  lv_obj_t* titleLabel = nullptr;
  lv_obj_t* subtitleLabel = nullptr;
  lv_obj_t* trailingLabel = nullptr;
  Window* leading = nullptr;
  Window* row = nullptr;
  RowSize size;

  void ensureTrailing(TextRole role);
};

// ---------------------------------------------------------------------------
// ListRow — the standard interactive list row.
// ---------------------------------------------------------------------------

class ListRow : public Button
{
 public:
  struct Content {
    const char* title = nullptr;
    const char* subtitle = nullptr;               // makes the row TwoLine
    const char* trailing = nullptr;               // right-aligned value/badge
    TextRole trailingRole = TextRole::Muted;
  };

  ListRow(Window* parent, const Content& content,
          std::function<uint8_t()> onPress = nullptr,
          std::function<uint8_t()> onLongPress = nullptr);
  // Explicit size override (e.g. Picker) — content must match the variant.
  ListRow(Window* parent, RowSize size, const Content& content,
          std::function<uint8_t()> onPress = nullptr,
          std::function<uint8_t()> onLongPress = nullptr);

  void setTitle(const char* text) { content_.setTitle(text); }
  void setSubtitle(const char* text) { content_.setSubtitle(text); }
  void setTrailing(const char* text, TextRole role = TextRole::Muted)
  {
    content_.setTrailing(text, role);
  }
  Window* leadingSlot() { return content_.leadingSlot(); }
  void expandHitArea(Window* control) { content_.expandHitArea(control); }

  // Non-color structural cue for flagged rows (warning border), mirroring
  // the widget-palette card cue.
  void setFlagged(bool flagged);

 private:
  RowContent content_;
};

// ---------------------------------------------------------------------------
// DSButton — role-styled push button, touch floor clamped.
// ---------------------------------------------------------------------------

class DSButton : public TextButton
{
 public:
  DSButton(Window* parent, const char* label, ButtonRole role,
           std::function<uint8_t()> onPress);
};

// ---------------------------------------------------------------------------
// Dialog — title / body lines / actions, all spacing DS-owned.
// Actions are right-aligned; add the primary action LAST.
// ---------------------------------------------------------------------------

class Dialog : public BaseDialog
{
 public:
  explicit Dialog(const char* title);

  StaticText* body(const char* text, TextRole role = TextRole::Body);
  DSButton* action(const char* label, ButtonRole role,
                   std::function<void()> onPress);

 private:
  Window* actionsRow = nullptr;
};

// ---------------------------------------------------------------------------
// PickerOverlay — modal list of comfortable (48 px) pick targets, with
// optional section headers and rich option rows.
// ---------------------------------------------------------------------------

class PickerOverlay : public BaseDialog
{
 public:
  explicit PickerOverlay(const char* title);

  void section(const char* label);
  ListRow* option(const ListRow::Content& content,
                  std::function<void()> onSelect = nullptr);
};

}  // namespace ds
