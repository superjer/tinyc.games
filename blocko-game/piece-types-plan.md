# Piece types & typed animation — plan

Give every model piece a semantic TYPE; animation dispatches on type instead
of waving everything. The current index-seeded flail survives as a selectable
animation STYLE, because it is funny.

Format compatibility is a non-goal (dev): model.dat and the wire struct may
break freely. `pm_sanitize` just has to make garbage safe, not meaningful.

## 1. Data

- `struct pm_piece`: replace `unsigned char axes` with `unsigned char type`.
  Same struct size; the old PM_PITCH/PM_YAW flags die with the placeholder
  that read them.
- Types (FIXED = 0, so zeroed memory is inert):

      FIXED  TORSO  HEAD  LEFT ARM  RIGHT ARM
      LEG 1  LEG 2  TAIL  JIGGLE    EYES

- `struct pmodel`: add `unsigned char style` next to `nr_pieces`
  (0 = WALK, 1 = FLAIL). Travels in the same raw packet, saved in model.dat,
  every client renders you the way you chose.
- Default humanoid + random generator assign the obvious types
  (BODY→TORSO, HEAD→HEAD, arms→LEFT/RIGHT ARM, legs→LEG 1/LEG 2).
- `pm_sanitize`: clamp type and style to valid ranges (else FIXED/WALK).
  Old model.dat loads as garbage types; paint and geometry survive; user
  re-types pieces in the editor. Good enough.

## 2. Animation context (replaces the lone `t`)

`pm_resolve`/`pm_emit` take a `struct pm_anim *` (NULL = standing pose, the
editor's static preview):

    walk_phase   accumulated from horizontal DISTANCE, not time (no foot skate)
    speed        0..1, smoothed horizontal speed
    look_pitch   the player's real pitch
    look_yaw     head yaw relative to body facing
    crouch       0..1, eased from the sneaking flag
    bounce       jiggle excitation: vertical velocity delta (landings), + walk bob
    t            wall clock, for idle motion (tail sway, blink windows)
    style        WALK or FLAIL (from the model)

Per-slot `pm_anim_state[NR_PLAYERS]` in pmodel.c holds the accumulators
(prev pos, walk_phase, smoothed speed, eased crouch, jiggle spring pos/vel,
blink seed). `pmodel_build` updates it from `player[i]` each frame — remote
players' pos/yaw/pitch already sync, so this works for everyone. Whether the
sneaking flag reaches remote players: check MSG_POS during implementation;
if it doesn't, remote crouch just stays 0 until it does.

## 3. Per-type motion (WALK style)

| type      | motion |
|-----------|--------|
| FIXED     | none |
| TORSO     | lowers by crouch (few px down in root space), subtle walk bob; the whole rig hangs off it, so everything follows |
| HEAD      | pitch = look_pitch, yaw = look_yaw clamped ±60° so it can't owl-spin |
| LEFT ARM  | walk swing (pitch axis), anti-phase to RIGHT ARM; lateral raises mirror RIGHT ARM's sign |
| RIGHT ARM | walk swing; the side that holds the item later — the swing/hold hook lives here |
| LEG 1     | pitch swing sin(walk_phase) · amp · speed |
| LEG 2     | anti-phase to LEG 1. Side-agnostic on purpose: alternate the types on a quadruped and it trots for free |
| TAIL      | slow idle yaw sway; lifts slightly with speed |
| JIGGLE    | damped spring on pitch, kicked by `bounce` (landings, walk bob) |
| EYES      | rendered normally, except a ~0.15 s blink window every 3–5 s (seeded per slot so crowds don't sync-blink) — implemented as "skip emitting these faces" in pm_emit |

Left/right arm distinction stays dumb: they rotate opposite ways (sign flip
on the lateral axis). If T-pose comes out backwards, flip the sign. Done.

FLAIL style: the current sines, applied to every piece except TORSO (the old
gate was the axes flags, which are gone). Flailing eyes and tails are a
feature.

Amplitudes/frequencies as constants first; promote to tweak-panel knobs if
tuning gets tedious.

## 4. Editor

- **TYPE button** in the piece view, below SELECT PARENT: label is the
  current type name ("RIGHT ARM", longest, fits the 320px column at scale 3).
  Left-click cycles forward, right-click back. Immediate visual feedback via
  ANIMATE.
- **STYLE button** below ANIMATE (label WALK / FLAIL), visible whenever
  ANIMATE is. Exempt from "any click stops ANIMATE" so you can A/B styles
  while it plays.
- **ANIMATE preview** feeds a synthetic context: walking in place
  (speed 1, phase advancing), gentle head sweep to show HEAD tracking,
  periodic blinks. FLAIL style previews the flail.

## 5. Order of work

1. `axes`→`type` swap, type tables, sanitize, TYPE button. FLAIL keys off
   type ≠ TORSO so the game still moves like today.
2. Anim context plumbing: pm_resolve signature, per-slot state, walk-phase
   from distance. Behavior unchanged (flail reads `t` from the context).
3. `style` byte + STYLE button + WALK dispatch skeleton (everything FIXED-
   still except legs). Then per type: legs → torso/crouch → arms → head →
   tail → jiggle → eyes blink.
4. ANIMATE synthetic context.
5. Save/send already cover the new bytes (raw struct); update tools/README
   only if any new socket command appears (none planned).

Not in scope, hooks left behind: item held in RIGHT ARM (needs item
rendering on players), real gait blending, 2nd/3rd person cameras.
