# Piece resizing in the model editor — plan

Let the editor change a piece's prism (dims + corner) instead of only its
paint, joints and wiring. Everything lives in the existing `struct pm_piece`
ints, so no format or net change — model.dat and MSG_PMODEL carry it as-is,
and `pm_sanitize` already clamps dims/corner into the 16³ space.

## UI: a RESIZE mode button

- New button in the piece column, between TYPE and ANIMATE (RESIZE gets
  272-332; ANIMATE and STYLE shift down to 336-396 and 400-460). Greyed
  like the other piece buttons when nothing is selected or ANIMATE plays.
  The box-count split in the two vkCmdDraw calls grows by one.
- Mutually exclusive with JOINT / SOCKET / PARENT, same as they are with
  each other.

## Interaction: click a face to push/pull it

The editor's grammar is "click a texel, nudge with keys" — resize follows it:

- In RESIZE mode the pick ray already returns the hit FACE of the selected
  prism (`pmedit_pick` gives face + texel). **Left-click grows** that face
  1 px outward, **right-click shrinks** it 1 px — the same L/R = more/less
  convention as the TYPE button. Hold-to-repeat like paint drags if it
  feels slow.
- Arrow keys / Space / LShift keep working as camera-relative nudges, but
  in RESIZE they move the whole prism (corner) instead of a point — grab
  the box and shove it a px at a time. That covers "grew the wrong way,
  slide it over" without leaving the mode.

## The arithmetic (all ints, px grid)

Growing the max side of axis a: `dims[a]++`.
Growing the min side: `corner[a]--, dims[a]++` (the far wall stays put).
Shrinking mirrors that. Clamps:

    1 <= dims[a] <= 16
    0 <= corner[a],  corner[a] + dims[a] <= 16

At a wall of the 16³ space the grow just refuses (no auto-shift — the wall
is real, use the corner nudge).

- `origin` and `attach` are free points in their spaces; resizing does NOT
  touch them. A joint can end up outside the prism — that's legal today
  (JOINT mode can already place it anywhere) and sometimes wanted.
- Odd dims are fine (the randomizer prefers even for symmetry; hand edits
  don't have to).

## Knock-on effects (all no-ops or cosmetic)

- **UVs/paint**: face extents (`pm_face_extent`) derive from dims, so the
  visible eu×ev window just grows/shrinks over the existing 16×16 tile.
  Paint survives; the dark border pm_paint drew around the OLD extent
  becomes an interior line. Cosmetic; repaint by hand. (Optional later: a
  "redraw borders" that only rewrites texels matching the border color.)
- **Selection hull / gizmo / zoom-to-fit**: all read dims per frame, follow
  automatically. The eased zoom absorbs the size change.
- **Feet on the ground**: lengthening legs sinks feet / floats the model —
  the attach points decide where things hang, and MOVE SOCKET is the
  existing fix. (Optional later: a one-key "drop feet to ground" that
  slides the root piece's attach y.)
- **Editor close**: save + net announce already send the whole struct.

## Order of work

1. RESIZE button + mode flag + button shuffle (ANIMATE/STYLE down a slot).
2. Face grow/shrink on click, with clamps; repeat on hold.
3. Corner nudge on arrows/Space/LShift in RESIZE mode.
4. Help text: the editor's G-screen entry mentions RESIZE.

Out of scope: undo, mirror-the-other-arm, border repaint, drop-to-ground.
