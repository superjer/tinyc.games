# Plan: resting angles into the struct, world, and net

Status: the test rig works and is user-approved. Angles currently live in
`pm_rest_deg[PM_MAX_PIECES][3]` (pmodel.c, editor-only, not saved), applied in
pm_resolve when `pm_rest_apply` is set, which pmedit flips on only for RESTING
ANGLE mode and the ANIMATE preview. This plan makes them a real, saved,
synced part of the model.

## 1. Struct (defs.c)

Add to `struct pm_piece` (defs.c:382), after `type`:

```c
    signed char rest[3];     // resting pitch/yaw/roll, degrees, +-25; the
                             // standing pose - animations swing on top of it
```

- All-char struct stays padding-free per piece (14 -> 17 bytes); `struct
  pmodel` grows 19628 -> ~19664 bytes. It still travels the net and disk as
  raw bytes.
- Zeroed memory stays inert (rest 0 = today's pose), so `pm_default`,
  NEW PART's initializer, and `pmodel_randomize` need no changes.
- In-struct (not a parallel array) so `pm_piece_delete` compaction and
  MAKE COPY's struct assignment carry the angles automatically — that's why
  the test rig needed the memset/memcpy hacks; both get deleted.

## 2. Sanitize + checksum (pmodel.c)

- `pm_sanitize` (pmodel.c:287): clamp each `rest[a]` to [-25, 25] (covers
  net input and hand-edited/corrupt model.dat).
- `pm_checksum` is FNV over the whole struct: nothing to do.

## 3. Resolve / render in world (pmodel.c)

In `pm_resolve`, replace the test-rig read with the struct field:

```c
    pitch += p->rest[0] * (PI / 180);
    pyaw  += p->rest[1] * (PI / 180);
    roll  += p->rest[2] * (PI / 180);
```

KEEP the `pm_rest_apply` gate (rename to taste, e.g. `pm_rest_pose`):

- The WORLD emit path (pm_resolve call at pmodel.c:~809) sets it 1 around its
  calls: everyone's model renders posed, all the time. That alone is "rendered
  in world" — my own model's shadow and remote players both go through it.
- pmedit keeps setting it exactly as now (`pmedit_restang || pmedit_animate`)
  around ITS calls. Do NOT apply the pose in the other editor modes: the
  geometry-editing math assumes the standing pose is pure translation —
  DETACH's offset folding, MOVE PART/PARENT flush placement
  (pmedit_flush_att), RESIZE face picking, and the JOINT gizmo all want
  axis-aligned pieces. Posing them would subtly corrupt clicks and folds.

Delete `pm_rest_deg`; the editor reads/writes `mo->piece[i].rest` instead.

## 4. Editor wiring (pmedit.c)

- `pmedit_rest_adjust`: write `pm_models[my_player].piece[pmedit_sel].rest`
  and clamp there (drop the pm_rest_deg reference).
- `pmedit_delete`: remove the `memset(pm_rest_deg, ...)` line (compaction now
  moves the angles with the pieces — strictly better behavior).
- `pmedit_copy`: remove the `memcpy(pm_rest_deg[i], ...)` line (the struct
  assignment already copies rest).
- Hint-line display reads `piece[pmedit_sel].rest`.
- Header doc: drop "test rig / doesn't save" wording.
- Saving is free: U-close already calls pmodel_save + announces over the net.

## 5. Disk format migration (pmodel.c)

model.dat is `[u8 id][raw struct pmodel]`. The struct grew, so a raw fread of
the new size against an old file reports "truncated" and throws the user's
saved model away. Add a one-time converter in `pmodel_load`:

- Read the file size. New size -> load as-is.
- Old size (1 + 2 + 12*14 + pad2 + 1024 + 18432; compute with sizeof-based
  constants, don't hardcode): load into a byte buffer, then unpack — copy
  header, then each 14-byte piece into its 17-byte slot with rest zeroed,
  then palette + texels from their old offsets.
- Anything else -> today's "truncated, ignoring" path.
- pmodel_save always writes the new format, so the conversion runs once.
- TEST with a backup copy of a real model.dat first (it lives in the launch
  cwd; the user has one they care about).

## 6. Net (net.c)

- The MSG_PMODEL send/recv/relay paths all use `sizeof(struct pmodel)` — they
  pick up the new size with no code changes (len guards included).
- Bump `NET_PROTO 2 -> 3` (net.c:56). Old and new builds disagree on the
  packet layout, and the proto check at HELLO is the honest gate. Both sides
  must rebuild — re-verify on real Windows eventually (standing backlog item).
- MSG_PLAYER / flags byte: untouched.

## 7. Verify

1. Build; open editor, pose a couple of pieces, U-close (saves + announces),
   relaunch: pose survives disk roundtrip and shows on the standing model in
   the world (F-key third person / V camera views to see your own).
2. Old-model.dat migration: back up, run once, confirm "loaded model.dat"
   (not "truncated") and unchanged look with rest all zero.
3. Two instances (`--dist 100 --noshadow`, per-instance BLOCKO_SOCK): join,
   pose in one, U-close, watch the other player's model repose live.
4. ANIMATE + in-world walk: animations swing on top of the pose (already
   proven by the rig, but re-check WALK arms vs a rolled arm piece).
5. Editor edit modes (DETACH, MOVE PART, RESIZE, JOINT) on a posed piece:
   still behave as if standing — pose must NOT leak into those views.
