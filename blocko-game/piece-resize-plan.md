# Piece resizing in the model editor — plan

Let the editor change a piece's prism (dims + corner) instead of only its
paint, joints and wiring — and make the PAINT follow the geometry, so a
resized piece never has stale or misaligned texels. Everything lives in the
existing `struct pm_piece` ints and `texel[][][]` bytes, so no format or net
change — model.dat and MSG_PMODEL carry it as-is, and `pm_sanitize` already
clamps dims/corner into the 16³ space.

## UI: GROW and SHRINK mode buttons

- Two new buttons in the piece column after TYPE: **GROW** at y 272-332 and
  **SHRINK** at 336-396; ANIMATE and STYLE shift down to 400-460 and
  464-524 (64px stride from y=16 as today). Greyed like the other piece
  buttons when nothing is selected or ANIMATE plays. The box-count split in
  the two vkCmdDraw calls grows by two.
- Mutually exclusive with JOINT / SOCKET / PARENT and each other.
- **Left-click only.** No right-click meaning in these modes.

## Interaction: click a face to push/pull it

- In GROW mode, left-clicking a face of the selected prism moves that face
  **1 px outward**. In SHRINK mode, 1 px inward. `pmedit_pick` already
  returns the hit face. Hold-to-repeat like paint drags if it feels slow.
- Arrow keys / Space / LShift keep their camera-relative nudge grammar, but
  move the whole prism (corner) 1 px. A pure slide changes no extents, so
  paint follows for free. (Grow one side + shrink the opposite is the same
  slide, also paint-safe.)

## Geometry arithmetic (all ints, px grid)

The rule: the new layer looks purely added — every other face, the joint,
and the children stay visually put. Grow only refuses when `dims[a] == 16`
(the prism fills the axis); shrink only at `dims[a] == 1`.

**The joint stays put for free (usual case).** `origin` lives in the
piece's fixed 16³ space, not relative to the prism — pm_resolve places the
piece by attach/origin alone; corner/dims only choose which box of that
space gets drawn. So while there's room in the space:

- grow the max face:  `dims[a]++`
- grow the min face:  `corner[a]--, dims[a]++`
- shrink the max face: `dims[a]--`
- shrink the min face: `corner[a]++, dims[a]--`

and nothing else moves. Shrink never needs more than this.

**Grow at the space wall: slide under the hood.** When the growing side is
already at the boundary (max: `corner+dims == 16`; min: `corner == 0`) but
`dims < 16`, the grow still happens — the content slides one px away from
the growing face inside the space, and everything positional shifts along
to keep appearances: `corner` (max case: `corner--`; min case: corner
stays 0), `origin`, and the `attach` of every CHILD of this piece (child
attaches live in this piece's space). Texels don't move — the texel edit
depends only on which face grew, not on where the prism sits in the space.

Each shifted point clamps to its own 0..16 wall. A clamped point gets
dragged 1 px toward the growing face — unavoidable and correct: origin is
storable only inside the 16³ space, so a joint can never sit more than 16
px from any prism wall; grow far enough and the joint must "max out" and
follow. Same for a child's attach.

Growing past the joint (or shrinking away from it) leaves origin outside
or inside the prism — legal today (JOINT mode can already place it
anywhere) and sometimes wanted. `attach` of THIS piece is in the parent's
space; resizing never touches it.

## Texels follow the geometry

Each face samples texels `[0..eu)×[0..ev)` of its 16×16 tile, anchored at
texel (0,0) (`uv = uvs[i] * extent / 16` in pmodel.vert). So when an axis
changes, the four LATERAL faces (the ones whose extent includes that axis)
each gain or lose one row/column of texels at the end that moved:

- **GROW**: insert one row/column at that end, filled by **copying the
  adjacent existing row** (clamp-extend). If the end maps to texel index 0,
  shift the existing rows up by one first; if it maps to the far end, just
  write row `e` (no shift).
- **SHRINK**: delete the row/column at that end (shift back down if it was
  index 0). The removed paint is gone — same finality as painting over it.

The clicked face and its opposite keep their texels untouched (their
extents don't involve the changed axis — the face just moves).

Which end is which comes from the shader's UV winding (WEST/SOUTH/UP are
u-mirrored so paint reads unflipped):

    face   u axis   v axis        face   u axis   v axis
    UP     −x       +z            EAST   +z       +y
    DOWN   +x       +z            WEST   −z       +y
    NORTH  −x       +y            SOUTH  +x       +y

e.g. growing the +x side edits u on UP/DOWN/NORTH/SOUTH: append on DOWN and
SOUTH (+x maps to the far end), insert-at-0 + shift on UP and NORTH.
One helper does it all: `pm_texel_resize(mo, piece, face, uv_axis, at_zero,
grow)` — insert/delete a row with clamp-copy; the six call sites are table
lookups.

## Knock-on effects

- **pm_paint's dark border**: the clamp-copy duplicates the border row
  outward, so the new edge stays border-colored (with the old border line
  one px in as a double line until repainted). Cosmetic.
- **Selection hull / gizmo / zoom-to-fit**: all read dims per frame, follow
  automatically. The eased zoom absorbs the size change.
- **Feet on the ground**: lengthening legs sinks feet / floats the model —
  attach points decide where things hang; MOVE SOCKET is the existing fix.
- **Editor close**: save + net announce already send the whole struct.

## Order of work

1. GROW + SHRINK buttons + mode flags + button shuffle (ANIMATE/STYLE down
   two slots).
2. `pm_texel_resize` helper + the face/axis/end table.
3. Face grow/shrink on left-click with clamps + texel edit; repeat on hold.
4. Corner slide on arrows/Space/LShift in GROW/SHRINK modes.
5. Help text: the editor's G-screen entry mentions GROW/SHRINK.

Out of scope: undo, mirror-the-other-arm, drop-to-ground.
