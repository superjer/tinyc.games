# Plan: player model version history in save-data

STATUS: IMPLEMENTED, with these user amendments: writes debounced to one
per second (plus the final at close); names zero-padded `00001.model`;
legacy model.dat is NEVER read - a fresh install loads
`blocko-game/assets/models/player-default.model` (a copy of the user's
model), and with nothing anywhere it randomizes as before.

Every editor session that changes the model leaves one numbered snapshot in
`save-data/blocko/player-models/`; the game loads the newest on startup.
Files are written only during their own session - once the editor closes,
a file is never touched again.

## 1. Files and naming

- Directory: `save-data/blocko/player-models/` under the launch cwd. Create
  the whole path at startup if missing (SDL3 has `SDL_CreateDirectory`; it
  makes parents? verify - if not, create the three levels in order).
- Names: `1.model`, `2.model`, ... - newest = highest number, parsed with
  strtol (NUMERIC, not lexicographic: 10.model > 9.model). Ignore anything
  that doesn't parse as a positive int ending in `.model`.
- Format: byte-identical to model.dat - `[u8 owner id][raw struct pmodel]`,
  one shared save/load routine. The loader keeps its size checks (and the
  v1 converter, though .model files will always be current-format).

## 2. Detecting "an edit" (checksum, not per-site hooks)

Instrumenting every mutation path (paint, RESIZE, joint/attach nudges, TYPE,
STYLE, rest angles, NEW PART, copy, delete...) is a dozen call sites and a
bug every time a new one is added. Instead:

- At editor open, remember `pm_checksum(&pm_models[my_player])` as
  `saved_sum`.
- Each `pmedit_update` frame: if the current checksum != saved_sum, write
  the session file and update saved_sum. FNV over ~15KB per frame is
  nothing, and a paint drag just rewrites a 15KB file a few dozen times.
- This catches every mutation for free, now and forever. Remote model
  arrivals can't trip it - it only ever checksums MY slot, which only the
  editor mutates.

## 3. Session lifecycle

- `pmedit_session_file` (static int, 0 = none). Allocated LAZILY on the
  session's first detected change: scan the dir for the current max N,
  take N+1, remember it. All later writes this session overwrite that file.
- On U-close: one final write if a session file exists (cheap to write
  unconditionally-if-allocated; guarantees the closing state is on disk
  even though the per-frame check almost certainly already wrote it), then
  forget the number. Next session allocates fresh.
- Editor opened and closed with no changes: no file, no gap in numbering.
- Free bonus: a crash mid-edit leaves the latest state on disk.

## 4. Startup load

- `pmodel_init`: scan the dir, load the highest-numbered file.
- Empty/missing dir: fall back to legacy `model.dat` (read-only from now
  on - pmodel_save's model.dat write is retired). A loaded legacy model
  becomes `1.model` the first time an editor session changes something.
- Nothing anywhere: randomize, as today.
- Directory scan: SDL3 `SDL_GlobDirectory`/`SDL_EnumerateDirectory`, or
  plain opendir/readdir with a _WIN32 branch if the SDL calls disappoint.

## 5. Non-goals / accepted quirks

- No pruning: ~15KB per session, let it grow.
- Two instances editing in the same cwd could race for the same number -
  not a real workflow, ignored.
- History is per-cwd (per-worktree), same as model.dat today.

## 6. Verify

1. Fresh dir + old model.dat: launch -> loads legacy (converter message),
   open editor, first paint stroke -> `1.model` appears; more edits only
   touch `1.model`; U-close -> final write, net announce as today.
2. Relaunch -> "loaded .../1.model" (newest), no model.dat write.
3. Second session with edits -> `2.model`; a no-edit open/close in between
   leaves no file.
4. Delete the dir -> recreated on launch.
