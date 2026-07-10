# Blocko debug socket

Blocko listens on a Unix domain socket so tools (or an AI agent) can benchmark
and drive the running game without touching the keyboard. It's how the perf
work in this project gets measured and reproduced.

- **Socket path:** `/tmp/blocko-<tag>.sock`, where `<tag>` is derived from the
  git worktree root, so several game copies (git worktrees) can run at once
  without colliding. The game prints the exact path on startup (`remote:
  listening on ...`), and the `bk` helper derives the same one. Override the
  whole path with the `BLOCKO_SOCK` env var.
- **Protocol:** one command per connection — connect, send one line, read the
  reply, disconnect.
- **Platform:** Unix only. On Windows the socket is absent; use the in-game
  console instead (see below).

Source of truth: `remote.c` (`remote_dispatch`).

## Sending commands

The `bk` helper sends one command and prints the reply:

```bash
blocko-game/tools/bk fps
blocko-game/tools/bk tp 200 -1500
blocko-game/tools/bk "spike 64 64 256 100"
```

Or use netcat directly, against the socket path the game printed at startup
(`remote: listening on /tmp/blocko-<tag>.sock`):

```bash
echo fps | nc -U /tmp/blocko-<tag>.sock
```

Or open the **in-game console** with the tilde (`` ` ``) key and type the same
commands — they run through the same dispatcher. Minecraft-style keys work
too: `/` opens the box with a slash typed (a `/command` runs and its reply
lands in the chat log), and `T` opens plain chat.

## Typical workflow

Build and run in the background, then drive it over the socket:

```bash
cmake --build build --target blocko
./build/blocko --seed 1234 --lock "measuring" &   # start locked, comparable seed
# ... drive and measure ...
blocko-game/tools/bk unlock
blocko-game/tools/bk quit
```

The `--lock "<msg>"` launch option starts the game already locked, so there's no
window where input reaches the game before a driver gets around to `bk lock`.
It's equivalent to sending `bk lock "<msg>"` immediately after launch.

The `--title "<msg>"` launch option sets the window title to `Blocko - <msg>`,
so when a run is launched for someone else to inspect, the title says what
they're supposed to be testing.

While `lock` is held, all keyboard/mouse input to the game is ignored (except
the tilde console) and the banner message is shown on screen — good for keeping
a test deterministic. `bk unlock` (or `bk lock 0`) releases it; `bk quit` works
even while locked.

When running **several instances at once** (multiplayer tests), launch each
lightweight so they don't fight over the GPU: `--dist <blocks>` sets the draw
distance (try 100) and `--noshadow` skips shadow maps, both from the very first
frame. Give each instance its own debug socket with the `BLOCKO_SOCK` env var —
set it on the game *and* on each `bk` call driving that instance:

```bash
./build/blocko --serve --dist 100 --noshadow --lock server &
BLOCKO_SOCK=/tmp/blocko-b.sock ./build/blocko --connect 127.0.0.1 --dist 100 --noshadow --lock client &
blocko-game/tools/bk net                          # talks to the server
BLOCKO_SOCK=/tmp/blocko-b.sock blocko-game/tools/bk net   # talks to the client
```

---

## Performance & profiling

| Command | What it does |
|---|---|
| `fps` | Frame stats since the last `fps reset`: frame count, elapsed seconds, FPS, avg/p50/p99/worst frame ms, `meshes_built`, `chunks_generated`, terrain `gen_ms`, and a per-pass generation breakdown (hmap/soil/caves/water/trees/light/corners). |
| `fps reset` | Zero the frame ring, the mesh/chunk/gen counters, and the `gpu` accumulators, and restart the elapsed timer. |
| `gpu` | Per-pass GPU times (ms/frame, averaged since `fps reset`): the four shadow cascades, terrain, and post, plus average faces drawn per frame into each cascade. Averaging matters — far/extreme cascades render on alternating frames. |
| `timings` | Per-section CPU time breakdown (seconds and % of total) for the named `TIMER()` buckets — `build_meshes`, `gpu_sync`, `shadow_render`, `draw_terrain`, etc. |
| `timings reset` | Snapshot the current timer totals as the new baseline, so `timings` reports deltas from here. |
| `spike [w] [d] [h] [reps] [threads]` | Time meshing a `w×d×h` cell region centered on the player, `reps` times, and report ms/build plus vertex counts. Optional `threads` overrides the mesh OpenMP thread count for the measurement. Defaults: `w=64`, `d=w`, `h=256`, `reps=50`, current thread cap. Used to measure rebuild cost vs. region size and thread count. |
| `meshthr [<n>]` | Get, or set, the OpenMP thread cap for mesh rebuilds (clamped to `MAX_MESH_THREADS`). Persists. Default is chosen from the core count at startup. |
| `redirty` | Mark every chunk mesh dirty to force a full remesh pass — for measuring rebuild cost under equal, controlled work (pair with `fps reset` / `timings reset`). |

Example — measure a full-chunk rebuild's real cost:

```bash
bk="blocko-game/tools/bk"
$bk redirty; $bk "fps reset"; $bk "timings reset"
sleep 5
$bk fps       # meshes_built and frame times
$bk timings   # build_meshes total; divide by meshes_built for per-mesh ms
```

## Player & camera

| Command | What it does |
|---|---|
| `pos` | Current position: `window_blocks` (x y z in the sliding window), `absolute_blocks` (world x z), and `scoot_chunks` (window offset in chunks). |
| `tp <ax> <az>` / `tp <ax> <ay> <az>` | Teleport to absolute block coords `ax, az`, landing on the ground (held at the sky until the destination terrain has generated). The 3-arg form is x y z: start at exactly height `ay` instead of snapping to the ground; gravity still applies, so pair with `noclip 1` to hold a camera above water or mid-air (`fly` pins altitude to the sky and ground-snap lands on the seabed). Coords clamp to ±1,000,000 blocks (farther overflows the scoot math); the reply echoes where you actually landed. |
| `save <name>` | Append `<seed> <ax> <ay> <az> <name>` (absolute blocks) to `blocko.saves` in the game's working directory, so a spot can be revisited later — set the seed, then `tp` to `ax az`. `<name>` is an arbitrary label (may contain spaces). |
| `load [<name>]` | Restore a spot saved with `save`: set that line's seed, regen the world, and teleport to the saved location. `load <name>` picks the matching line (latest if a name repeats); bare `load` picks the most recent line in the file. |
| `walk <frames>` | Hold forward + run for `frames` frames, then stop. |
| `fly <frames> <blocks_per_sec> [<y_blocks>]` | Noclip forward along the current yaw at a fixed altitude and constant speed for `frames` frames — deterministic traversal for streaming/benchmarks. Altitude defaults to just above sea level (`SEA_LEVEL - 6`) so the shadow cascades stay full of terrain; pass `y_blocks` (y=0 is the sky) to override. |
| `turn <deg>` | Set the player yaw to `deg` degrees. |
| `look [<yaw> <pitch>]` | Point the camera (like moving the mouse): set yaw and pitch in degrees. `+pitch` looks down, `0` is level. With no args, just reports the current `yaw_deg` / `pitch_deg`. |
| `target` | Report the block the player is aiming at (`target <x> <y> <z> tile <t>`, absolute coords — what a left click mines) and where a right click would place (`place <x> <y> <z>`). `none` if nothing is in range. |
| `click <left\|right> [frames]` | Hold a "mouse button" for `frames` rendered frames (default 1), then release — the socket's stand-in for clicking. `left` = break/mine (hold well past `MINE_TIME` ≈ 45 while aimed at one block), `right` = place. `frames 0` releases immediately. Placement obeys the usual collision guard, so it's skipped if the player's body occupies the target cell. |
| `patch` | Report the reject+patch state for instant block edits: `edit_pending` (a break/place waiting on its debounced rebuild), `mining` (a dig in progress), and the effective reject box (`box_abs lo .. hi`) with its patch vertex count, or `box off`. |

## Rendering & view

| Command | What it does |
|---|---|
| `dist <blocks>` | Set the draw distance in blocks. |
| `debounce <frames>` | Set how many quiet frames a light-only change waits before the chunk is remeshed. |
| `tint [<0\|1>]` | Debug viz for the reject+patch instant edits: blend the patch mesh (opaque + water) 50% red so you can see the tiny corrected geometry drawn over the rejected faces. Toggles with no arg. |
| `cull [<0|1>]` | Freeze (`1`) or unfreeze (`0`) chunk-culling (same as the F2 key), and report camera-visible chunk count and bounds. With no arg, just reports. |
| `freeze [<0|1>]` | Pin the shadow maps and sun where they are (same as the F6 key) so the cascades stay anchored in the world and you can walk out to inspect their edges. Toggles with no arg. |
| `sun <pitch>` | Freeze the sun at `pitch` radians (0 = east, π/2 = up, π = west). `sun run` resumes the normal day/night motion. Useful for deterministic lighting in screenshots. |
| `grassshadow [<0\|1>]` | Toggle whether tall grass casts shadows in the near cascade (same as the T key). Toggles with no arg. |
| `shadowlod [<blocks>]` | Get/set the distance beyond which chunks cast 2×2×2 LOD shadows in the far/extreme cascades (default 160). `0` = LOD everywhere, `-1` = disabled (full detail always). |
| `screenshot [<path>]` | Save the current frame as a PNG. Default path is per-worktree (`/tmp/blocko-<tag>_shot.png`), like the socket/dump. Captured inside the frame while its swapchain image is still acquired, so it's Vulkan-clean. |
| `noclip [<0\|1>]` | Fly through solids with no gravity. While on: jump (Space) rises, sneak (LShift) sinks, WASD moves horizontally through anything. Toggles with no arg. |

## World inspection

| Command | What it does |
|---|---|
| `find <tile> <ax0> <az0> <ax1> <az1>` | List every block of type `<tile>` in the absolute-coord rectangle (inclusive) as `x y z` lines. |
| `tile <ax> <ay> <az>` | Read one tile by absolute coords, through the sim accessor: the main window first, then any sim area holding that chunk (a server keeps one following each remote player — simarea.c). `out of window` if no copy exists anywhere. |
| `edits [clear]` | Report how many block edits are in the edit overlay (edit.c) — the recorded player edits that replay whenever their chunk regenerates, so digs/builds survive scooting away and back. `edits clear` forgets them (do this before `sum`-based A/B gen comparisons on an edited world). |
| `form near [<radius>]` | List formations within `<radius>` blocks of the player (default 512) as `x z spheres <n> above_sea <n>` lines. |
| `formdump [<path>]` | Reconstruct the nearest formation's carved voxel model from its column spans and write it (`int W,H,D` + bytes, `j`=up) for offline rendering. Pipe it through `tools/form_render.py` (needs numpy + PIL) to eyeball the "carve the scaffold" shapes: `python3 blocko-game/tools/form_render.py /tmp/blocko-<tag>_form.bin` writes `formations.png`. |
| `sum` | FNV-1a hashes of the raw `tiles`, `sunlight`, and `gndheight` arrays — for A/B-ing generation changes. |
| `csum <acx> <acz>` | FNV-1a hash of one chunk's tiles + gndheight, addressed by absolute chunk coords — compares the same piece of world across instances whose windows sit at different scoots (where `sum` would hash different regions). The window holds absolute chunks `-scootx..15-scootx` × `-scootz..15-scootz` (scoot from `pos`) — note the negation. A chunk outside the window still answers if a sim area holds it (identical hash for identical content, so it A/Bs area copies against a client's window); otherwise `out of window`. |
| `dump [<path>]` | Write the raw `tiles` + `gndheight` arrays to a file (default `/tmp/blocko-<tag>_dump.bin`, per-worktree like the socket) for offline diffing. |

## World generation knobs

These change generation parameters; send **`regen`** afterward to rebuild the
world with the new values (exception: `tweak` regenerates on its own). With no
arguments, each prints its current knobs.

| Command | What it does |
|---|---|
| `tweak` | List every knob in the big world-gen table (tweak.c) — the base height octaves, terraces, oceans, peaks, ranges, warps, ledges, soil bands, vegetation, ore, caves, trees, tall grass, formations, and the noise-algorithm knobs. `*` marks off-default values. Knob values are local to this instance — they don't sync in multiplayer, so tweaking while connected desyncs terrain. |
| `tweak <name> [<val>]` | Show or set one knob by name (case-insensitive). Values clamp to the knob's range. Unlike the other commands here, a set **regenerates the world by itself**, debounced ~0.5s after the last change. |
| `tweak reset` | Every knob back to its default (regenerates if anything changed). |
| `tweak dump` | Print the off-default knobs as replayable `tweak <name> <val>` lines — save them to keep a look you like across runs, or transcribe into the table defaults (terrain_knobs.h / gen_knobs.h). |
| `tweak panel [<0\|1>]` | Show/hide the on-screen tweaker panel (same as the **K** key: arrows navigate/adjust, Shift = coarse steps, PgUp/PgDn jump sections, Backspace resets the knob). |
| `noise [<knob> <val>]` | Terrain noise knobs: `kernel2`, `contrast`, `aniso`, `nvary`, `interp`. (Also rows of the `tweak` table.) |
| `form [<knob> <val>]` | Formation knobs: `enable`, `region`/`chance` (how densely carved-rock formations cluster — lower `chance` or higher `region` = fewer, cheaper to generate), `steps`/`rmin`/`rmax` (scaffold length & sphere size), `detail` (0/1: add the fine grit shell — off by default, ~2x cheaper for a barely-visible change). (Note: `form near ...` is the separate inspection command above.) |
| `caves [<0\|1>]` | Enable/disable cave carving. |
| `trees [<0\|1>]` | Enable/disable trees. |
| `flat [<0\|1>]` | Force a dead-flat world (uniform grass just above the waterline, no caves/formations). A debug aid for spotting chunk-seam artifacts that terrain relief hides. Toggles with no arg; send `regen` after. |
| `terrace [<0\|1>]` / `terrace jitter <amp>` | Enable/disable the terrace/shelf pass (mesa country), or set the shelf-boundary jitter amplitude in steps (0 = straight even bands, ~0.4 default breaks them up). Shorthand for the `TERRACE_ENABLE` / `TERRACE_JITTER_AMP` tweak knobs. |
| `seed [<n>]` | Set the world seed. Setting a seed also clears the edit overlay (new world, old edits meaningless). |
| `regen` | Invalidate every chunk's generation stamp; the whole window regenerates in place, nearest chunks first. Recorded edits replay onto the regenerated chunks (send `edits clear` first for pristine terrain). |

## Multiplayer (net.c)

One player hosts, others join; the world itself never travels — a joining
client gets the seed and the edit overlay, generates terrain locally, and
replays the edits. After that the wire carries events: block edits (both
ways), player states at 20Hz (drawn as cube-folk), mob snapshots at 15Hz
(the server owns the slimes; clients send punches and receive bonks), and
the sun's position every few seconds. Also available at launch as
`--serve [port]` and `--connect <host[:port]>` (default port 26262).
Works on Windows too (winsock) — only this debug socket is Unix-only.

`--headless` runs a dedicated server as a terminal app: no window, no
Vulkan, no input. It implies `--serve` (add `--serve <port>` for a
different port), prints chat to stdout, and stops on Ctrl+C or `bk quit`.
The world simulates around the invisible player 0 at spawn, so mobs and
live edits happen within the 1024-block window centered there.

| Command | What it does |
|---|---|
| `serve [<port>]` | Start hosting on the given TCP port. |
| `connect <host> [<port>]` | Join a hosted game. Blocks up to 5 s for the WELCOME (seed + edit log), then regenerates the world with the server's seed. |
| `net` | Report the current role and connected players. |
| `say <text>` | Speak in chat, as if typed with T. In-game, `T` opens chat, `/` opens command entry, and chat lines show for ~10 s bottom-left. |

## Mobs

| Command | What it does |
|---|---|
| `mob` | Report living slime count, total kills, auto-spawn state, and each live mob's absolute position and HP. |
| `mob spawn` | Force-spawn a slime near the player. |
| `mob <0\|1>` | Disable / enable automatic mob spawning. `mob 0` also despawns any slimes already alive, so the world is actually mob-free. |

## Test-harness control

| Command | What it does |
|---|---|
| `lock [<msg>]` | Block all game input except the console and show `<msg>` in an on-screen banner (update it as a test progresses). |
| `lock 0` / `unlock` | Release the input lock. |
| `quit` | Shut the game down cleanly (works even while locked). |
