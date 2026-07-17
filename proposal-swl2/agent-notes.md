# Switch Layout v2 — design prototype (fake data, not production)

v2 shape: model configuration references switch ROLES only. Role→switch
assignment is global (radio level); one switch may carry several roles
(SD = Flaps + OSD layout) because no single model uses all roles. Models
never remap — they just use roles. A conflict exists iff one model uses two
exclusive roles resolving to the same switch/position; conflicts are caught
at config time, layout-edit time, and model load. The layout also drives
voice feedback on every switch flip (announces the role as resolved for the
current model — wrong-switch protection).

## Recommended design (global multi-assignment + usage conflicts)

- `06-layout-coassign.png` — layout page: SD carries "Flaps + OSD layout";
  per-card announce indicator (voice / quiet).
- `06b-layout-quiet.png` — quiet indicators (SG, SH momentary) further down.
- `07-picker-conflict.png` — role picker in a model editor: roles shown with
  their global switch ("OSD layout (SD)"); "RTH (SA)" carries a live
  config-time conflict mark "• in use: Rates".
- `08-conflict-ack-dialog.png` — config-time acknowledgment: "RTH and Rates
  share switch SA. Flipping SA will trigger BOTH on this model." Use anyway
  (warning-styled) / Cancel; acknowledged conflicts stay flagged.
- `09-layout-edit-impact.png` — layout-edit-time impact preview: adding OSD
  layout to SD lists the models that would newly conflict (SKYWALKER).
- `10-load-conflict-warning.png` — model-load backstop alert for an
  unresolved/imported conflict, in the stock warning style.
- `11-role-editor-announce.png` — role editor: per-role Announce toggle with
  the voice ladder ("rates" + high/mid/low).

## Explored and superseded: per-model overlays (kept for the record)

An earlier v2 iteration allowed per-model remap/unbind overlays. Global
multi-assignment replaced it: the quad case needs zero per-model config, and
the "ran out of switches" endgame is served by a visible global alias role
instead of a hidden per-model remap.

- `01-role-picker.png`, `01b`, `01c` — overlay-era picker (This model /
  free switches / repurpose sections).
- `02-model-overlay.png` — per-model overlay page (superseded).
- `03-load-warning.png` — overlay acknowledgment alert (superseded; reworked
  into 10).
- `04-layout-change-preview.png` — early "move role" preview (reworked into 09).
- `05-inputs-roles.png` — model Inputs list with role names as the switch
  column, incl. a warning-colored "RTH • unbound" line — still representative
  of the recommended design's unassigned-role degradation.

Prototype code (static fake data): `radio/src/gui/colorlcd/radio/radio_switch_layout.cpp`
(v1 page + "V2 mock demos" dev strip opening each mock). No storage/YAML changes.
