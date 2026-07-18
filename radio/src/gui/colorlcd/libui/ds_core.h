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
#include <vector>

#include "bitmaps.h"  // EdgeTxIcon (EmptyState)
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
  Compact,  // 40 px — DENSE one-line data row (title + trailing); D1 variant.
            // Same floor and layout as OneLine, but a distinct, sanctioned
            // choice for lists that would otherwise use TwoLine: the second
            // line's detail is relegated to the row's editor so more rows fit.
  Picker,   // 48 px — comfortable single-line pick target
  TwoLine,  // 52 px — title + subtitle (+ trailing)
};

// List/row DENSITY (D1). Comfortable is the default two-line treatment for
// rows that carry detail; Compact collapses that same content to a 40 px
// one-line row (dropping the subtitle to the editor) for dense lists — e.g. a
// 20-line mixer, where 52 px rows waste vertical space. Purely a density
// selector over the SAME semantic row API; it is not a new raw-styled row.
enum class Density : uint8_t {
  Comfortable,
  Compact,
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
  explicit List(Window* parent, Density density = Density::Comfortable);

  // The density this list was built with; rows can size themselves from it so
  // a screen declares density once (D1).
  Density density() const { return density_; }

 private:
  Density density_ = Density::Comfortable;
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
  // Density-aware ctor (D1): the SAME Content renders as a 52 px two-line row
  // under Comfortable density, or a 40 px one-line Compact row under Compact
  // density (the subtitle is relegated to the row's editor). A row with no
  // subtitle is a plain 40 px OneLine either way.
  ListRow(Window* parent, Density density, const Content& content,
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
// FormRow — one settings-form line: [ label 40% ][ control 60% ].
// The largest migration surface (every settings page). The caller builds ONE
// field widget into the control slot; the DS owns the 40/60 split, vertical
// centering and the 40 px touch floor. Absorbs the
// `FlexGridLayout` + `newLine` + `StaticText`-label boilerplate.
//
//   new ds::FormRow(form, STR_MODE, [&](Window* slot) {
//     new Choice(slot, rect_t{}, 0, n, GET_SET_DEFAULT(...));
//   });
//
// The control (a Choice / NumberEdit / ToggleSwitch / TextEdit / etc.) is the
// focus target; the row itself is inert. Its own field states apply as-is.
// ---------------------------------------------------------------------------

class FormRow : public Window
{
 public:
  FormRow(Window* parent, const char* label,
          std::function<void(Window* slot)> buildControl = nullptr);

  void setLabel(const char* text);

 protected:
  bool addChild(Window* child) override;

 private:
  lv_obj_t* labelObj = nullptr;
};

// ---------------------------------------------------------------------------
// Card — non-interactive grouping panel with an optional title. `space-3`
// inset, `space-2` internal gap, subtle border + surface. Add the grouped
// content as children of the card. Absorbs ad-hoc `Window` + `padAll` boxes.
// ---------------------------------------------------------------------------

class Card : public Window
{
 public:
  Card(Window* parent, const char* title = nullptr);

  // Children added to the card stack in its column, below the title.
  Window* content() { return this; }

 private:
  lv_obj_t* titleObj = nullptr;
};

// ---------------------------------------------------------------------------
// EmptyState — the "nothing here yet" pattern: a centered icon, a headline, an
// optional hint line and an optional primary action button. Replaces bare
// "+"-only screens with something that says what the screen is for and how to
// begin. `space-5` hero breathing room; the action is a DSButton (Primary).
// ---------------------------------------------------------------------------

class EmptyState : public Window
{
 public:
  EmptyState(Window* parent, EdgeTxIcon icon, const char* headline,
             const char* hint = nullptr, const char* actionLabel = nullptr,
             std::function<uint8_t()> onAction = nullptr);
};

// ---------------------------------------------------------------------------
// Grid — the columnar/tabular screen component.
//
// The whole point of this component is COLUMN ALIGNMENT: header cell i is
// ALWAYS x-aligned with every data row's cell i. It guarantees this
// structurally, not by convention — the header row and every data row share
// ONE immutable, fixed-pixel column template (an `lv_coord_t[]` built once and
// handed by reference to every row's LVGL grid layout). Fixed-pixel columns
// resolve to identical geometry regardless of a row's contents, so column i
// starts at the same x in the header and in every row, forever. (This is the
// class of bug the previous batch hit: collapsing a grid to a single-line list
// row dropped the shared column structure, so the FM0..FM8 header no longer sat
// above its value columns.)
//
// It also owns, from the DS scale only:
//   * a sticky/frozen header row (LVGL "floating" child — stays put while the
//     data rows scroll vertically underneath it);
//   * a frozen leading label column that stays put during horizontal scroll,
//     with the remaining value columns scrolling, when the columns overflow the
//     viewport (GVars' 9 flight-mode columns + label). On a 480 px TX16S they
//     fit, so no horizontal scroll is triggered — but a narrower target, or a
//     wider column set, scrolls rather than clipping;
//   * the 40 px touch floor: data rows are >= 40 px tall (the whole row is the
//     tap target — tap = edit, long-press = menu), and a column declared
//     `interactive` (per-cell tap) is clamped to a >= 40 px cell width too;
//   * inter-column gaps, row gaps and side margins.
//
// Columns are declared once, before any header()/addRow():
//   * Fixed(w)  — a fixed design-px column (DS-scaled); e.g. the GV name label.
//   * Fill(min) — shares the remaining viewport width equally with the other
//     Fill columns, never below `min`; if the Fill minimums don't fit, the grid
//     overflows and scrolls horizontally instead of shrinking below the floor.
//     GVars' 9 value columns are Fill(min) so they auto-fit the width the way
//     the original hand-computed GVAR_VAL_W did.
// ---------------------------------------------------------------------------

enum class CellAlign : uint8_t { Start, Center, End };

class Grid : public Window
{
 public:
  struct Column {
    enum class Kind : uint8_t { Fixed, Fill };
    Kind kind = Kind::Fixed;
    coord_t size = 0;                    // Fixed: design-px width; Fill: min px
    CellAlign align = CellAlign::Start;
    bool interactive = false;            // per-cell tap -> >= 40 px cell width
    bool frozen = false;                 // leading label column (freeze on h-scroll)

    static Column Fixed(coord_t widthPx, CellAlign a = CellAlign::Start,
                        bool frozen = false)
    {
      return {Kind::Fixed, widthPx, a, false, frozen};
    }
    static Column Fill(coord_t minPx, CellAlign a = CellAlign::Center,
                       bool interactive = false)
    {
      return {Kind::Fill, minPx, a, interactive, false};
    }
  };

  // One line of the grid — the sticky header, or a scrolling data row. A data
  // row is the interactive unit (tap = edit, long-press = menu, focusable); the
  // header is inert. Both realize their cells into the SAME shared column
  // template, which is what guarantees alignment. Screens may subclass Row and
  // override onLiveCheckEvents() to refresh cell text/highlight live.
  class Row : public Button
  {
   public:
    Row(Grid* grid, bool header,
        std::function<uint8_t()> onPress = nullptr,
        std::function<uint8_t()> onLongPress = nullptr);

    // Set (creating on first use) the text of the cell in column `col`.
    void setCell(int col, const char* text, TextRole role = TextRole::Body);
    // Small-font variant for a value that would otherwise overflow its cell.
    void setCellSmall(int col, const char* text, bool small,
                      TextRole role = TextRole::Body);
    // Active-fill highlight on a cell (e.g. the current flight-mode column).
    void highlightCell(int col, bool on);

    // One label spanning ALL columns, centered — for a full-width action row
    // (e.g. an inline "+") that still lives in the aligned grid body.
    void setSpanningCell(const char* text, TextRole role = TextRole::Body);

    lv_obj_t* cellObj(int col);  // realized cell label, for automation/tests

   protected:
    Grid* grid_ = nullptr;
    bool isHeader_ = false;

   private:
    lv_obj_t* rowObj_ = nullptr;
    std::vector<lv_obj_t*> cells_;  // one per column, realized lazily
    lv_obj_t* ensureCell(int col, TextRole role);
  };

  Grid(Window* parent, const std::vector<Column>& columns);
  Grid(Window* parent, std::initializer_list<Column> columns) :
      Grid(parent, std::vector<Column>(columns))
  {
  }

  int columnCount() const { return (int)aligns_.size(); }

  // The sticky header row (created once). Its cells align with every data row.
  Row* header();
  // A scrolling data row. `onPress`/`onLongPress` make the whole row the touch
  // target (>= 40 px). Returns a plain Row; screens needing live refresh pass
  // their own Row subclass via addRow(Row*).
  Row* addRow(std::function<uint8_t()> onPress = nullptr,
              std::function<uint8_t()> onLongPress = nullptr);
  // Adopt a caller-constructed Row subclass (already `new`ed with this Grid).
  Row* addRow(Row* row);

  // --- geometry shared with Row (DS-owned; not for screens) ---
  const lv_coord_t* colDsc() const { return colDsc_.data(); }
  coord_t totalWidth() const { return totalW_; }
  coord_t colGap() const { return colGap_; }
  coord_t sideMargin() const { return sideMargin_; }
  coord_t headerHeight() const { return headerH_; }
  coord_t dataRowHeight() const { return rowH_; }
  CellAlign columnAlign(int col) const { return aligns_[col]; }
  bool columnFrozen(int col) const { return col < (int)frozen_.size() && frozen_[col]; }
  coord_t columnWidth(int col) const;

#if defined(SIMU)
  // Test hooks: reach the vertical-scroll body (to drive a scroll) and the
  // sticky header row, so a gtest can prove alignment and stickiness against
  // real LVGL geometry rather than by eyeballing.
  Window* bodyForTest() const { return body_; }
  Row* headerForTest() const { return headerRow_; }
#endif

 private:
  std::vector<CellAlign> aligns_;
  std::vector<bool> frozen_;
  std::vector<lv_coord_t> colDsc_;  // stable address; LVGL keeps the pointer
  coord_t colGap_ = 0;
  coord_t sideMargin_ = 0;
  coord_t totalW_ = 0;
  coord_t headerH_ = 0;
  coord_t rowH_ = 0;
  Row* headerRow_ = nullptr;
  Window* body_ = nullptr;  // vertical-scroll container for the data rows
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

  // Declares a new section. The header for a section is materialized lazily:
  // a lone section (the common case — a picker with a single group of
  // options) never gets a header at all, since there is nothing for it to
  // differentiate from. The header is only created once a second section()
  // call proves there is more than one group. A header whose text is just
  // the overlay title (case/plural-insensitive) is always suppressed, even
  // in multi-section lists, since it still restates rather than
  // differentiates. See DESIGN_SYSTEM.md.
  void section(const char* label);
  ListRow* option(const ListRow::Content& content,
                  std::function<void()> onSelect = nullptr);

#if defined(SIMU)
  // Test-only introspection: how many section-header widgets were actually
  // realized (as opposed to deferred-and-never-realized, or suppressed as a
  // title echo). Lets a gtest assert the lone-header and title-echo rules
  // structurally instead of by pixel-diffing.
  int materializedHeaderCountForTest() const { return materializedHeaderCount_; }
#endif

 private:
  const char* title_ = nullptr;
  // Section #1's label, owned (not just referenced) since materializing it
  // is deferred past the section() call that provided it -- a caller must
  // not be required to keep a formatted-into-a-stack-buffer label alive
  // across later section()/option() calls just because this one might turn
  // out to be a lone section.
  char pendingSectionLabel_[48] = {};
  int sectionCount_ = 0;  // also doubles as "section #1 still pending" == 1
#if defined(SIMU)
  int materializedHeaderCount_ = 0;
#endif

  bool isTitleEcho(const char* label) const;
  void addSectionHeader(const char* label, bool moveToFront);
};

}  // namespace ds
