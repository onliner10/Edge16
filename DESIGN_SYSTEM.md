# EdgeTX Color-LCD Design System (DS)

Spacing / layout / sizing counterpart of the merged semantic-color token system
(`radio/src/gui/colorlcd/controls/widget_palette.*`). Together they form one
system: color tokens say *what things mean*, the DS says *where things sit and
how big they are*. Neither is a convention — both are **enforced at API level**.

Scope of this document: TX16S-class landscape 480×272 first (`LAYOUT_SCALE`
keeps the derived values proportional on 320- and 800-wide targets, exactly as
the existing `PAD_*` constants do).

---

## 1. The physics (all numbers derive from this)

| Fact | Value |
|---|---|
| Panel | 4.3" diagonal, 480×272 px, IPS, capacitive touch |
| Pixel density | √(480²+272²) = 551.7 px / 4.3" = **128.3 PPI** |
| Conversion | **1 mm ≈ 5.05 px → 5 px/mm** (0.198 mm/px) |
| Input | Thumbs, often gloved, radio held in both hands, field vibration |
| Viewing | Bright sunlight, sunglasses; arm's length (~40 cm) |

Consequences, non-negotiable:

- **Touch floor 40 px ≈ 8 mm.** Industry range is 7–9 mm (MS: 7 mm min /
  9 mm ideal; Android 48 dp ≈ 9 mm; Apple 44 pt ≈ 6.9 mm). Gloves + vibration
  push us above the bottom of that range; 272 px of screen height forbids the
  top. 8 mm = 40 px is the hard floor for ANY interactive target, both axes.
  The DS layer clamps it mechanically (`min_height`/`min_width` in the shared
  styles) — a screen literally cannot render a smaller target through the DS
  API, and the guard forbids building targets outside the DS API.
- **Comfortable target 48–52 px (9.6–10.4 mm)** — pickers and two-line rows.
- **Adjacency rule.** Two individually-legal targets can still be dangerous
  when flush. Adjacent interactive edges must be **≥ 8 px (1.6 mm)** apart;
  when either target is at the 40 px floor, **≥ 12 px (2.4 mm)**. This is not
  a guideline the screen author applies — DS containers own all inter-target
  gaps (list gap 8 px + 2×2 px non-interactive border inset ⇒ 12 px between
  active cores; action-row gap 12 px), so the rule holds structurally.
- **Embedded controls** (toggle inside a list row): the control's hit slot is
  expanded to the full row height and ≥ 40 px width via `ext_click_area`
  inside the DS layer; the boundary to the row's primary tap area is a single
  vertical line, never an island surrounded by the other target.
- **No thin-hairline affordances.** Interactive surfaces are the affordance
  (full-width rows, filled/bordered buttons ≥ 2 px border), per the color
  system's ≥ 7:1 contrast roles.

## 2. Spacing scale

Base unit **4 px = 0.8 mm**; every DS dimension is a multiple. Six steps only.
The px values are **private to the DS layer** (`libui/ds_core.cpp`) — screens
never see them, they pick components, and components pick spacing.

| Token | px | mm | Used for (by the DS layer, not by screens) |
|---|---|---|---|
| `space-0` | 0 | 0 | flush edges (list rows to list edge) |
| `space-1` | 4 | 0.8 | micro: icon↔label inside one slot |
| `space-2` | 8 | 1.6 | related: list-row gap, dialog body line gap, title↔subtitle |
| `space-3` | 12 | 2.4 | grouping: component side inset, page side margin, action gap |
| `space-4` | 16 | 3.2 | separation: section top spacing, dialog frame padding |
| `space-5` | 24 | 4.8 | hero: empty-state breathing room |

Old `PAD_TINY(2)/PAD_SMALL(4)/PAD_MEDIUM(6)/PAD_LARGE(8)` are what produced the
squashed screens — 2 px gaps between 6.3 mm rows. They remain for legacy code
under the ratchet (§6) and are **forbidden in screen code** going forward.

## 3. Layout grid — 480×272

```
┌────────────────────────────────────────────┐
│ header 480×45 (existing MENU_HEADER_HEIGHT)│ 45
├────────────────────────────────────────────┤
│ 12 │        content 456 px         │ 12    │
│    │  rows: 40 (one-line)          │       │ 227
│    │        52 (two-line)          │       │
│    │        48 (picker option)     │       │
│    │  gap 8, section top 16        │       │
└────────────────────────────────────────────┘
```

- **Page margins:** 12 px sides, 8 px top/bottom of scrollable content.
- **Content column:** 456 px, full-bleed rows (one tap column — no horizontal
  hunting). Forms: label 40% / control 60% (grid FR units, vertically centered).
- **Row heights:** one-line/control/button **40 px**, picker option **48 px**,
  two-line row **52 px** (2×21 px STD lines + 2×5 px padding). All heights are
  fixed per variant — a row cannot be "a bit shorter to fit more".
- **Fit check:** 227 px content ⇒ 4.7 one-line rows or 3.8 two-line rows per
  screenful. That is the *correct* density for 8 mm touch; scrolling is
  cheaper than mis-taps.
- **Dialogs:** width 384 px (80%), frame padding 16 px, body line gap 8 px,
  16 px before the action row, actions right-aligned with 12 px gaps,
  action buttons 40 px tall.

## 4. Type scale (existing LVGL fonts, semantic roles)

| Role | Font | Line height | Use |
|---|---|---|---|
| `title` | theme header font | — | page header only (owned by `Page`) |
| `body` | `FONT_STD` | 21 px | values, dialog text, row subtitles |
| `strong` | `FONT_BOLD` | 21 px | row titles, emphasized values |
| `caption` | `FONT_XS` | ~16 px | section headers, badges — never primary info |
| `hero` | `FONT_XL/XXL` | — | main-view widget values only |

`FONT_XXS` is **banned** in screens (sunlight + sunglasses). Color roles come
from the token system: `body`→PRIMARY1 ink, `muted`→SECONDARY1 (labels only,
≥4.5:1), `warning`/`active` accents per `widget_palette` rules.

## 5. Component inventory

Every component: **semantic parameters only** — content, roles, callbacks.
No `rect_t`, no `coord_t`, no `PaddingSize`, no positions. States
(normal/focused/pressed/disabled) come from the shared theme styles
(`etx_lv_theme.cpp`), never per-instance.

Implemented in the prototype (`radio/src/gui/colorlcd/libui/ds_core.h/.cpp`) —
marked ●; spec-only marked ○.

| # | Component | API sketch (semantic params only) | States | Absorbs / replaces |
|---|---|---|---|---|
| 1 | ● `ds::List` | `ds::List(Window* body)` — scrollable column, margins+gaps owned | — | every ad-hoc `setFlexLayout(COLUMN, PAD_TINY)` body |
| 2 | ● `ds::ListRow` | `ListRow(list, Content{title, subtitle?, trailing?, trailingRole}, onPress, onLongPress?)`; `leadingSlot()` for one embedded control; setters for live text | normal/focus/press/disabled via theme; `setFlagged(role)` border cue | `ListLineButton` content layout, `OverlayCard`, `RolePickerRow`, hand-rolled row buttons |
| 3 | ● `ds::RowStyle` adapter | `ds::RowStyle::apply(existingRow, RowSize)` + `ds::rowHeight(RowSize)` | as row | migration bridge: keeps `ListLineButton` machinery, DS owns geometry |
| 4 | ● `ds::SectionHeader` | `SectionHeader(list, "FREE SWITCHES")` | — | ad-hoc `StaticText` + `padTop` headers |
| 5 | ● `ds::Dialog` | `Dialog(title)`; `.body(text, TextRole)`; `.action(label, ButtonRole, fn)` — actions auto-laid right-aligned, primary rightmost | per-button theme states | raw `BaseDialog` + hand-packed button boxes |
| 6 | ● `ds::Button` | `Button(parent, label, ButtonRole{Primary,Secondary,Destructive}, fn)` — 40 px tall, min-width 96 px, clamped | theme states + role colors | `TextButton` sizing free-for-all |
| 7 | ● `ds::PickerOverlay` | `PickerOverlay(title)`; `.section(label)`; `.option(Content, onSelect)` — 48 px options, scrollable, modal | option = row states | squashed picker dialogs; long `Menu` lists with rich rows |
| 8 | ○ `ds::FormRow` | `FormRow(form, label, [](Window* slot){ new Choice(slot,…); })` — 40%/60% grid, 40 px min row | control's own | `FlexGridLayout` + `newLine` boilerplate |
| 9 | ○ `ds::Card` | `Card(parent, title?)` — non-interactive group, `space-3` inset, `space-2` internal gap | — | `Window` + `padAll` boxes |
| 10 | ○ `ds::PageScaffold` | `Page` already enforces header 45 px + routed body; DS addition: body defaults to `ds::List` unless a form is requested | — | per-page `body->padAll(...)` variance |
| 11 | ○ `ds::EmptyState` | `EmptyState(body, icon, headline, hint, primaryAction?)` | button states | ad-hoc "+" only screens |

Notes:
- `ds::ListRow::Content.title` renders `strong`, subtitle `body`+muted,
  trailing `body` with caller-chosen **role** (Muted/Warning/Active) — the only
  "styling" a screen can express, and it is semantic.
- `leadingSlot()` returns a fixed 40×full-height slot; DS wires
  `ext_click_area` so the embedded control meets the floor; only one embedded
  control per row (adjacency).
- `ds::embeddedControlSize()` returns the DS-owned pixel size (`space-4`,
  16 px) for a small icon-style control placed inside `leadingSlot()` (e.g.
  an enable checkbox) — screens size the control from this instead of
  inventing their own `LAYOUT_*_SCALED` constant for a control that only
  ever appears in a slot.

## 6. Enforcement

Three layers — API shape, mechanical clamp, CI guard:

**(a) API shape.** DS components accept no geometry. The temptation "just add
a small margin" has no parameter to land on; the nearest legal move is picking
a different variant or filing a DS change (one place, reviewed).

**(b) Mechanical clamp.** Inside `ds_core.cpp` every interactive object gets
`min_height/min_width = 40 px` via shared styles, and embedded-slot hit areas
are expanded. Even a future DS bug that shrinks a variant cannot produce a
sub-floor target.

**(c) CI guard.** `tools/design-system/check_design_system.py`:

- Scans `radio/src/gui/colorlcd/**` **excluding the allowlisted DS layer**
  (`libui/` — the only place raw styling may live) for forbidden patterns:
  `lv_obj_set_style_pad*`, `lv_obj_set_style_margin*`, `lv_obj_set_pos(`,
  `padAll/padTop/padBottom/padLeft/padRight(`, `PAD_ZERO/TINY/SMALL/MEDIUM/
  LARGE/THREE/SCROLL/TABLE_*` tokens, `LAYOUT_VAL_SCALED`/`LAYOUT_SIZE*`/
  `LAYOUT_ORIENTATION*` (new ad-hoc scaled constants), and `rect_t{` with a
  nonzero literal origin (absolute positioning of content).
- **Baseline ratchet** (`tools/design-system/ds_baseline.json`): per-file
  violation counts of the ~100 existing screens are recorded once. The build
  **fails** if any file's count rises above its baseline or a non-allowlisted
  file appears with new violations; counts may only fall. When they fall, the
  run reports "ratchet can tighten" and `--update-baseline` re-records (a
  lowered baseline is committed with the cleanup — improvement is locked in).
- Runs as a gtest (`TEST(DesignSystem, GuardClean)` shells the script) and as
  a standalone CI step, so violations fail the build, not a review comment.
- Escape hatch: a line-level `// ds-allow: <reason>` comment suppresses one
  finding; the suppression count itself is baselined so it cannot grow
  silently.

**Layer boundary.** Allowlisted directory: `radio/src/gui/colorlcd/libui/`
(the DS layer plus the legacy primitives it is absorbing). Everything else —
`model/`, `radio/`, `mainview/`, `setup_menus/`, `controls/`, `widgets/`,
`layouts/`, `themes/` — is screen code and subject to the guard. Migration of
a legacy primitive out of raw styling shrinks the allowlist surface over time;
the end state is: raw LVGL styling exists only in `etx_lv_theme.cpp` +
`ds_core.cpp`.

## 7. Migration story (~100 screens)

1. **Land the layer + guard with today's counts as baseline.** Nothing
   breaks; nothing new gets in.
2. **New/touched screens must be DS-only** (guard makes regressions fail).
3. **Migrate by template, not by page:** (i) list pages → `ds::List` +
   `ds::ListRow`/`RowStyle` (mixes, inputs, curves, GVARs, LS, SF, telemetry);
   (ii) form pages → `ds::FormRow` (model/radio setup, module config);
   (iii) dialogs → `ds::Dialog`; (iv) pickers → `ds::PickerOverlay`.
   Each migration deletes the file's baseline entry.
4. **Absorb primitives:** move `ListLineButton` geometry into `RowStyle`,
   delete per-screen `*_X/*_Y/*_W` constants; retire `PaddingSize` from public
   headers.
5. **Tighten:** when a directory hits zero, add it to the guard's
   zero-tolerance set so it can never regress.
