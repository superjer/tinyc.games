#ifndef BLOCKO_PMEDIT_C_INCLUDED
#define BLOCKO_PMEDIT_C_INCLUDED
#include "blocko.c"

// In-game player model editor (U key). The model floats in front of the
// frozen camera in a plain standing pose; WASD spins it like a turntable
// (easing to 360 deg/s over half a second). Clicking a prism selects it: the
// view eases to center on that prism (zoomed to fit), it gets a white outline
// while the rest of the model draws full-color behind it (Z-tested, so colors
// stay matchable), and left-clicks paint the targeted pixel with the palette
// panel's selected swatch (right-click, or shift/ctrl/alt/cmd + click,
// eyedrops instead: the pointed-at texel's color becomes the swatch). The
// panel, top-left in this view, holds all 16 palette slots - transparent
// first (painting with it erases: the shader discards sub-half alpha), then
// the 15 colors - over a hue-lightness picker that recolors the selected
// swatch's slot live, every texel wearing it following, since paint is
// palette-indexed. Under the picker, FLOOD FILL swaps the brush for a
// bucket (a click repaints the connected same-color region on that face)
// and SUPER FLOOD for a firehose (a click recoats the piece's every texel,
// all six sides); clicking the lit button, or leaving the piece view,
// hands the brush back. The PLACE ATTACHMENT POINT button
// switches the piece view to placing the rotation origin instead, and MOVE
// PART to editing the attach point in the parent's space. PLACE ATTACHMENT
// POINT shows only the active piece (rest of the model hidden) and its
// origin as a small color-cycling cube with ruled axes through it (1-texel
// bright/dim segments, depth-tested so the surface intersections read
// exactly); the gizmo rides the cursor while it's on the piece, a click
// plants the point there and leaves the mode, and the zoom keeps both the
// piece and a nudged-away gizmo in frame.
// MOVE PART shows the piece and its pink-rimmed parent; pointing anywhere
// on the parent replaces the piece with a half-transparent preview of it
// laid flush against that face at the cursor (the attach point shifts
// along the face normal so the piece's near side kisses the surface - the
// joint never moves), and a click commits that spot and leaves the mode.
// Off the parent the piece sits where it is and the
// arrow keys nudge the point a px at a time (camera-relative; repeats work;
// WASD still rotates the turntable). PLACE ATTACHMENT POINT moves only the
// pivot - the attach point shifts by the same delta so the piece itself
// stays put. MOVE PART never touches the origin. The parent button is two
// buttons in one: with a parented piece selected it reads DETACH - one
// click re-hangs the piece on the invisible player box, folding the old
// parent chain's offsets into the attach point so the piece stays visually
// put (the attach range walls permitting) - and with a root-hung piece
// it's SELECT PARENT. PARENT mode
// re-wires the hierarchy in one click: the selection moves to its own
// little view on the left (same turntable spin, wearing the white outline,
// hovering with a gentle 1Hz half-texel bob - it's unattached)
// while the main view keeps only the eligible parents (pieces that would
// loop the chain are hidden). Hovering one previews it with a pink rim; the
// click makes it the parent AND lays the piece flush against the clicked
// spot, MOVE PART-style, then it's back to the whole model, nothing
// selected. ESC or a far miss backs out. In the plain selected view the
// arrow keys nudge the piece around just like MOVE PART (mouse placement
// stays that mode's). The TYPE button shows the
// selected
// piece's animation type (HEAD,
// LEFT ARM, ...) - left-click cycles forward, right-click back. RESIZE
// reshapes the prism: left-click a face to pick it (it wears a see-through
// cycling-hue slab), then drag it - the face pushes out and pulls in to
// follow the cursor - or nudge with the arrow keys a px at a time, by
// relative angle: the face grows when the arrow leans toward its outward
// normal on screen, shrinks when it leans away, and does nothing when the
// arrow reads near-perpendicular to it (rotate a bit). The paint follows
// (the lateral faces gain a copy of their edge row, or lose one) and the
// joint stays put; with no face picked the arrows are idle.
// RESTING ANGLE poses the piece's standing-still pitch/yaw/roll, +-25 deg a
// degree at a time: arrows pitch/yaw, Q/E roll, still relative to the
// parent. The angles live in the model (saved and sent like everything
// else) and the world always shows them, animations swinging on top; in the
// editor the pose shows ONLY in this mode and in ANIMATE - the geometry
// modes keep the plain standing pose their math needs; clicking another
// piece switches to it.
// DELETE (or the Del/Backspace keys) removes the piece and its whole
// subtree (no undo yet). NEW PART spawns a fresh 4x4x4 piece in a gray
// checkerboard coat and drops straight into the PARENT one-click flow to
// hang it somewhere; on the click its joint snaps to the middle of the side
// facing the parent, so the piece hangs outward
// (backing all the way out cancels the piece). MAKE COPY (or ctrl-C) clones
// the selected piece - prism, joint, paint, type - into the same flow, except
// the clone keeps the original's joint so it swings the same. The ANIMATE
// button plays the model's animation (walking in place)
// on the whole model - WASD still spins it, the piece buttons grey out, any
// click (or ESC) stops it right back where you were, and the zoom stays fit
// to the standing pose so it doesn't pump. The STYLE button below it flips
// the model between WALK and FLAIL animation styles (a model property that
// travels with it over the net) and is click-exempt so you can A/B styles
// while ANIMATE plays. HIDE gets overlapping pieces out of the way: click
// it, then every piece clicked goes 90% see-through and dead to clicks
// (unpickable everywhere) until a click lands somewhere else. With pieces
// hidden the button reads UNHIDE (n); one click brings them all back, no
// mode. Clicking on
// another piece switches straight to it (in paint and RESIZE
// modes - wherever a click on it means nothing else); clicking off every
// piece or ESC goes back to the whole model; U closes the editor and
// announces the new look over the net. Saving is automatic: a session that
// changes anything writes its own numbered snapshot (00001.model, ...)
// under the per-user save dir (SDL_GetPrefPath), at most once a second, final
// write at close - the newest snapshot is what the game loads next run. A half-transparent
// dark green quad, one block's top face, floats at the in-game ground level
// in every view that shows the whole model (near-level pitches only), so a
// floating or sunken model reads at a glance; a wireframe box in the same
// green traces the player's collision box so the model can be sized to it.

// The editor's code lives in pmedit/, one concern per file, included here in
// definition-before-use order (a single translation unit, like everything):
#include "pmedit/state.c"   // shared state: selection, modes, camera, matrices
#include "pmedit/layout.c"  // button layout + hit tests
#include "pmedit/palette.c" // color picker math, paint + flood brushes
#include "pmedit/piece.c"   // piece ops: parenting, flush placement, copy, delete
#include "pmedit/undo.c"    // session snapshots + the undo/redo ring
#include "pmedit/draw2d.c"  // 2D helpers: solid rects, colored quads, glyphs
#include "pmedit/glyphs.c"  // the pixel-art tool cursors
#include "pmedit/picker.c"  // the LOAD model picker
#include "pmedit/raycast.c" // cursor ray vs the prisms, world->screen
#include "pmedit/emit.c"    // build the preview's instances
#include "pmedit/render.c"  // draw the preview over the finished world
#include "pmedit/input.c"   // keyboard, mouse and the 60Hz update
#include "pmedit/ui.c"      // panels, palette, labels, hints

#endif // BLOCKO_PMEDIT_C_INCLUDED
