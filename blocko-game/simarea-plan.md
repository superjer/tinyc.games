# Sim areas: simulate around every player

## Goal

The server (headless or listen) simulates mobs around every connected player,
not just near its own window center. All simulation reads go through one
accessor, so no gameplay code assumes the render ring holds the world under
it — the sim/render divorce happens at the interface, not in storage.

Non-goals: moving rendering off the ring, item-drop sync, lighting inside sim
areas, interest-managed mob snapshots, world saving.

## Shape: a sim area is a tiny ring

The main world window is a direct-mapped ring: slot = absolute coord masked
(`T_` does `(x - scootx) & (TILESW-1)`), with absolute stamps (`chunk_stamp`)
saying which chunk each slot currently holds. A **sim area is the same
structure at 4×4 chunks** (256×256 blocks, power-of-2 masks intact), following
one remote player:

```c
struct warea {
        unsigned char *tiles, *sun;   // sun is gen scratch only (light = render)
        int *gndh;
        struct stamp stamp[4][4];     // absolute, like chunk_stamp
        char claim1[4][4], claim2[4][4];
        int scootx, scootz;           // blocks, like the main ring's
        int maskw, maskd, pitchz, pitchx; // so one set of macros fits all sizes
};
```

The main ring becomes `struct warea main_area` whose pointers alias the
existing globals (masks `TILESW-1`). One description of "a windowed piece of
world"; the main one is just big.

Memory per area: tiles 10.5MB + sun 10.5MB + gndh 0.25MB ≈ **21MB/player**,
~150MB worst case at 7 clients. `SIM_AREA_CHUNKS 4` is the tunable.

**Rim trust (answered by stage 1):** chunk content is a pure function of
(seed, absolute coords) — rim-generated and interior-generated chunks hash
identically (see the determinism gate below). So ALL of an area's chunks match
the terrain clients render, and the mob leash only needs to keep mobs inside
the area's guaranteed coverage (±64 blocks of the anchor), not a smaller
trusted interior.

## The accessor

```c
int sim_tile(int wx, int y, int wz);  // window coords, like T_
int sim_gndh(int wx, int wz);
```

Resolution order: main ring if the absolute chunk is stamp-resident (`AGEN_`
check — two compares, so the local-player fast path stays cheap) → linear scan
of the ≤7 areas → `BEDR` (solid; a mob outside all coverage stands still
rather than falling through the world).

Converted call sites (all of them):
- collision.c `block_collide` — the two `T_` reads (covers player, mob, and
  item physics via `world_collide`)
- mob.c `mob_spawn` — `GNDH_` + `T_` reads

`rayshot` and everything else render-side keeps raw `T_`: targeting is a
client concern and always happens at the window center.

Overlapping areas (two players near each other) hold duplicate copies of the
same chunks. That's fine: generation is deterministic and edits write through
to every container, so the copies are byte-identical; wasting ~10MB beats
refcounting.

## Generation

All gen writes are chunker.c's 23 `TT_/TSUN_/TGNDH_` macro sites. Those macros
gain a destination: a thread-local `struct warea *gen_area` set when a builder
claims a job, with masks/pitches read from the area (a runtime multiply in gen
inner loops is noise next to the terrain math). `form_spans()` and the noise
functions already take absolute coords — untouched.

`chunk_builder` scans the main ring first (as now), then each active area for
stamp-mismatched slots, nearest-to-anchor first, with the same claim +
pass1-neighbor-wait discipline scoped to that area. Area scoot bumps when the
anchor crosses a chunk boundary — absolute stamps make this free, same as the
main ring: scrolled-in slots simply mismatch and regenerate.

## Edits

`set_tile` / `edit_apply_remote` gain one loop: after the main-ring attempt,
write the tile into every area containing the coord and recalc that column of
the area's `gndh`. The overlay records once, exactly as now. When an area
chunk finishes generating, replay the overlay into it (`edit_apply_chunk`
parameterized by area, skipping light). `tile_light_update` stays
main-ring-only — light is render.

This is what makes a remote player's dig real to the mobs chasing them.

## Mobs

- **Spawn:** the ambient spawn loop (mob.c `spawn_timer`) iterates anchors —
  the local player plus every `net_player_active(i)` (server only) — instead
  of just `player[my_player]`, each anchor getting its share of
  `MOB_POP_TARGET`. `mob_spawn` reads through `sim_gndh`/`sim_tile`.
- **Leash:** a mob farther than 64 blocks from every anchor despawns (it's
  leaving trusted sim coverage). Clients already interpret absence from the
  snapshot as death — no protocol change.
- Targeting is already nearest-player. `MSG_MOB` is already absolute coords.

## Lifecycle

Server allocates an area when a remote player becomes active, frees it when
they go inactive (the existing 120-frame `seen` timeout) or disconnect.
Always-allocate, even when the player stands inside the main window — uniform
beats clever, and the redundancy is 21MB. (Skipping window-resident players is
a possible later optimization, noted and not done.)

Single player and pure clients allocate nothing; the accessor's fast path is
the ring, so behavior and perf are unchanged.

## Determinism gate — ANSWERED (stage 1, 2026-07-09)

Question: does a chunk generated at a window rim (missing neighbor pass1
context) have different tiles than the same chunk generated mid-window?

**No.** Experiment: two headless instances, same seed, windows offset by
(+8,+8) chunks (tp then regen on one), `csum` compared over the whole 8×8
shared region — 64 chunks including every rim/interior asymmetry on both axes
and corners, repeated on two seeds (777, 31337): all 128 comparisons
identical, tiles and gndheight. Chunk content is a pure function of
(seed, absolute coords); window position and neighbor pass1 availability
don't leak into it. Today's multiplayer does NOT diverge at differing window
rims, and sim areas can trust all their chunks.

**Bug found along the way:** `regen_world()` while builders are mid-job can
hang terrain generation permanently — a builder in chunk_builder's
wait-for-neighbor-TEDGE_ loop waits for a pass1 stamp that regen invalidated
and that no other builder will re-claim (repro: tp 8 chunks, regen ~1s later
while the scoot-in chunks still generate; generation stalls with holes).
The `connect` socket command hits the same window: client_welcome calls
regen_world with builders running (the --connect launch path is safe — it
joins before the workers start). Worth fixing separately: e.g. builders
re-check their claim's stamp validity in the wait loop and abandon the job,
or regen_world waits for builders to go idle.

## Stages (each builds, hash-verified, committed)

1. **`csum` command + rim experiment.** DONE (commit ef5f89c) — see the
   determinism gate above for the result and the regen-race bug it exposed.
2. **Fix the regen race.** DONE — regen epoch. `regen_epoch` (defs.c) is
   bumped by regen_world() inside the (chunks) critical (the stamp wipe now
   happens under the lock too); a builder snapshots the epoch when claiming
   and re-checks it at every stamping point — after each pass1 unit, in the
   wait-for-neighbor-edges loop, and before the final pass2 stamp. On a
   mismatch it abandons: skips the stamp (never marking stale-seed data
   fresh), releases its claims, and loops back to claim a fresh target.
   Second race found while verifying: an abandoning builder releases claim1
   on edges it never stamped, and a builder that claimed a neighboring
   target after the regen skipped those edges (claim1 was held), so it
   waited forever for a stamp nobody owned (~23-chunk holes in the repro at
   1.5s delay). Fix: the wait loop re-scans the 3×3 each iteration and
   adopts any missing edge that is unclaimed, generating it itself.
   Verified: tp + regen races at 0.2/0.7/1.5/2.5s delays each settle with
   all 256 window chunks generated (csum-probed every slot, zero
   "not generated"); csums of 5 chunks identical before/after a raced
   regen (no stale stamps); `connect` sent mid-initial-generation (the
   client_welcome regen path) completes with zero holes and client csums
   matching the server. Note for probing: in-window absolute chunk range is
   `-scootx .. 15-scootx` (csum maps wcx = acx + chunk_scootx).
3. **`struct warea` + gen through `gen_area`.** DONE — struct warea (defs.c)
   holds tiles/sun/gndh + maskw/maskd/pitchx/pitchz; main_area aliases the
   globals (filled in startup); TT_/TSUN_/TGNDH_ index through the
   thread-local gen_area (defaults to &main_area). Light macros (TGLO_ etc)
   stay global — light is render. tscootx/tscootz keep their role as the
   claimed destination's scoot snapshot. Verified no behavior change: all
   256 in-window csums identical old-vs-new binary on seed 777 (fresh
   spawn) and seed 31337 (after tp 520 520, exercising scooted gen).
4. **Accessor.** DONE — simarea.c: `sim_tile`/`sim_gndh`, AGEN_-gated main
   ring else solid BARR / gndh 0 (no BEDR tile exists; BARR is the invisible
   solid). Converted: collision.c block_collide (the two T_ reads), mob.c
   mob_spawn (GNDH_ + T_; its explicit AGEN_ guard subsumed — ungenerated
   ⇒ sim_gndh 0 ⇒ gnd<2 refuses), PLUS two sim reads added after this plan's
   survey: mob_los_clear (IS_SOLID) and mob_water_surface (T_). mob_punch's
   IS_SOLID stays raw (player aim = client concern). Verified vs the stage-3
   binary on seed 777: identical spawn pos, identical walk stall point
   (terrain step at x=563), identical tp 1200 900 landing (y and scoot),
   mobs auto-spawn (6) and respawn near the player after the tp scoot.
5. **Areas live.** DONE — deviations from the sketch above, all
   simplifications the absolute-keyed ring made possible:
   - Areas are direct-mapped by ABSOLUTE coords (slot = abs & mask, stamps
     hold abs chunk coords) — no per-area scoot vars; the "scoot" is just
     cx0/cz0 (coverage low corner, anchor chunk second from it). A builder
     maps the area window via tscootx = -cx0*CHUNKW, coords 0..SIM_AREA_W.
   - One builder per area at a time (busy flag) instead of the main ring's
     claim1/claim2 edge dance; estamps still skip redundant pass-1 work
     across scoots. Per-area epoch (bumped on activate/scoot/deactivate and
     by regen_world) makes mid-job builders abandon, like regen_epoch.
   - Areas share ONE write-only sun scratch buffer (gen never reads sun;
     light is render) — ~17MB/player tiles + 64KB gndh, buffers kept after
     deactivation for reuse. Activation wipes stamps (edits while inactive
     never wrote through, so old copies lie).
   - Replay is main-thread: builders queue finished area chunks
     (area_fresh, the area cousin of just_generated), sim_areas_update
     drains it into edit_apply_area_chunk (tiles + area gndh, no light).
   - `tile` + `csum` (not `sum`) learned the accessor: csum of an area
     chunk hashes identically to a window copy, so instances A/B at
     distance. Fixed along the way: net_send_my_state's `if (headless)`
     guard also silenced headless CLIENTS (now server-only), which would
     have starved the server of anchors.
   Verified (headless server + client, seed 777): client tp 1800,1800
   (~1250 blocks outside the server window) → all 16 area-chunk csums match
   the client's window copies; client digs → server `tile` shows the edit
   and whole-chunk csums match INCLUDING gndh (the area ground-height rules
   mirror tile_light_update's); slot-reuse + return → overlay replays the
   dig into the regenerated area chunk; disconnect deactivates the area;
   full-window main-ring csums identical to the pre-stage-3 baseline;
   server holds 60.0fps idle and with an active area.
6. **Mob anchors + leash + per-anchor caps.** DONE — anchors are the host's
   body plus every net_player_active ghost; a headless server's disembodied
   player is neither an anchor nor an aggro target, so an idle dedicated
   server spawns nothing. Each anchor gets its own MOB_POP_TARGET share
   (a slime counts against its nearest anchor), spawn ring 20-56 blocks
   around remote anchors (inside their guaranteed coverage), classic 30-80
   around the host. The leash is HORIZONTAL distance - the area covers the
   whole column, and the first cut used 3D distance and shed every slime
   under a freshly-teleported (still falling, dy > 64) player: survive
   within MOB_DESPAWN (100) of the host's body or MOB_LEASH (64) of any
   remote player. legit_tile's window clamp is lifted from the sim paths:
   block_collide reads sim_tile bare (the y bounds check moved into
   sim_tile, returning OPEN off the top/bottom of the world),
   world_collide floors its float-to-block division (mobs in sim areas can
   sit at negative window coords, where truncation rounds the wrong way),
   mob_los_clear / mob_water_surface / mob_spawn unclamped (out-of-coverage
   already refuses via gndh 0 and solid BARR).
   Verified (headless server + headless client, seed 777, client at abs
   1810,1795 - ~1250 blocks outside the server window): 6 slimes spawn
   20-56 blocks from the client, mirrored exactly in the client's `bk mob`;
   a 30-block tp keeps precisely the 4 within 64 horizontal and replaces
   the 2 beyond; a slime chases to contact and its bonks shove the client
   ~2 blocks; a client punch shatters it server-side (kill tally and
   shards mirror both ways); disconnect leash-despawns everything; all 256
   window csums still match the pre-stage-3 baseline; single-player spawns
   6 around the local player in the classic ring, unchanged.

## Sizing notes

- 4×4 chunks ⇒ mobs live within ±64 blocks of a player. Slime AI operates in
  tens of blocks; spawn ring is 30–60 blocks. If the leash ever feels tight,
  8×8 (±192 usable, 85MB/player) is the same code with a bigger constant.
- ANSWERED (stage 2): gen writes `TSUN_` in gen_columns only; the one gen-time
  *read* is recalc_corner_lighting (blocklight.c, end of pass2), which is
  corner-light output — render only. Trees test open sky by scanning `TT_`,
  not sun. Area gen skips lighting, so areas can share one write-only dummy
  sun buffer (racy garbage, never read) and drop to ~11MB/player.
