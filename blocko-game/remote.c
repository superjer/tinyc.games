#include "blocko.c"
#ifndef BLOCKO_REMOTE_C_INCLUDED
#define BLOCKO_REMOTE_C_INCLUDED

// remote.c - debug/automation socket + in-game console
//
// A unix socket for reading timings and poking at the running game, so tools
// (or an AI agent) can benchmark and drive the game without the keyboard.
// One command per connection: connect, send a line, read reply, disconnected.
//
//   echo fps     | nc -U /tmp/blocko-<tag>.sock
//   echo "tp 200 -1500" | nc -U /tmp/blocko-<tag>.sock
//
// The socket path is per-worktree: /tmp/blocko-<tag>.sock, where <tag> is
// derived from the git worktree root (see remote_tag), so several game copies
// can run at once without colliding. The startup line prints the exact path,
// and the bk helper connects to the same one. BLOCKO_SOCK overrides it.
//
// The tilde-key console runs the same commands through remote_dispatch().
//
// The socket transport is unix-only; on Windows only the console works.

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#define REMOTE_RING 8192

int remote_listen_fd = -1;
int remote_client_fd = -1;
char remote_cmd[256];
size_t remote_cmd_len;

// frame times (ms) since last "fps reset"
float frame_ring[REMOTE_RING];
int frame_ring_len;
unsigned long long frame_prev_count;
unsigned long long fps_reset_count;

// timer_times snapshot taken at last "timings reset"
unsigned long long timer_base[timer_ + 1];

// counter snapshots taken at last "fps reset"
int fps_base_meshes;
int fps_base_chunks;
int fps_base_gen_ticks;

int remote_walk_frames; // hold forward key for this many frames
int remote_tp_snap;     // tp landed: plant feet once the ground is known
int remote_fly_frames;  // noclip along yaw for this many frames...
float remote_fly_speed; // ...at this many blocks/sec (deterministic traversal)
float remote_fly_alt = SEA_LEVEL - 6; // ...holding this y (blocks; y=0 is the sky)
int remote_break_frames; // hold "left click" (mine) for this many frames
int remote_build_frames; // hold "right click" (place) for this many frames

// Per-worktree instance tag, so multiple game copies (git worktrees) don't
// collide on the socket / dump paths. Derived from the git worktree root (or
// the cwd if git isn't available); the bk tool derives the identical tag the
// same way, so both ends land on the same socket. Override the socket path
// entirely with the BLOCKO_SOCK env var.
const char *remote_tag()
{
        static char tag[64];
        if (tag[0]) return tag;

        char root[4096] = "";
#ifndef _WIN32
        FILE *g = popen("git rev-parse --show-toplevel 2>/dev/null", "r");
        if (g)
        {
                if (fgets(root, sizeof root, g))
                        root[strcspn(root, "\n")] = '\0';
                pclose(g);
        }
        if (!root[0] && !getcwd(root, sizeof root))
                root[0] = '\0';
#endif
        if (!root[0])
        {
                snprintf(tag, sizeof tag, "default");
                return tag;
        }

        // FNV-1a of the full path keeps distinct worktrees apart even if their
        // leaf directory names happen to match; the readable prefix is just the
        // sanitized leaf name. The bk tool mirrors this byte-for-byte.
        unsigned h = 2166136261u;
        for (char *s = root; *s; s++)
                h = (h ^ (unsigned char)*s) * 16777619u;

        const char *leaf = strrchr(root, '/');
        leaf = leaf ? leaf + 1 : root;
        char clean[24];
        int n = 0;
        for (const char *s = leaf; *s && n < (int)sizeof clean - 1; s++)
        {
                char c = *s;
                int ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z') || c == '-' || c == '_';
                clean[n++] = ok ? c : '_';
        }
        clean[n] = '\0';

        snprintf(tag, sizeof tag, "%s-%08x", clean, h);
        return tag;
}

#ifdef _WIN32
void remote_init() { fps_reset_count = SDL_GetPerformanceCounter(); }
#else
void remote_init()
{
        const char *path = getenv("BLOCKO_SOCK");
        char pathbuf[256];
        if (!path)
        {
                snprintf(pathbuf, sizeof pathbuf, "/tmp/blocko-%s.sock", remote_tag());
                path = pathbuf;
        }

        unlink(path);
        remote_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (remote_listen_fd < 0) return;

        struct sockaddr_un addr = {0};
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path);

        if (bind(remote_listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
            listen(remote_listen_fd, 1) < 0)
        {
                close(remote_listen_fd);
                remote_listen_fd = -1;
                fprintf(stderr, "remote: could not listen on %s\n", path);
                return;
        }

        fcntl(remote_listen_fd, F_SETFL, O_NONBLOCK);
        fps_reset_count = SDL_GetPerformanceCounter();
        fprintf(stderr, "remote: listening on %s\n", path);
}
#endif // _WIN32

int floatcmp(const void *a, const void *b)
{
        float d = *(const float *)a - *(const float *)b;
        return d < 0 ? -1 : d > 0 ? 1 : 0;
}

void remote_dispatch(const char *cmd, char *out, size_t outsz)
{
        char *p = out;
        char *end = out + outsz;
        *p = 0; // a command may write no reply (e.g. find with no matches)

        if (!strncmp(cmd, "fps reset", 9))
        {
                frame_ring_len = 0;
                fps_reset_count = SDL_GetPerformanceCounter();
                fps_base_meshes = nr_meshes_built;
                fps_base_chunks = nr_chunks_generated;
                fps_base_gen_ticks = chunk_gen_ticks;
                memset(gen_pass_ms, 0, sizeof gen_pass_ms);
                memset(gpu_ms_accum, 0, sizeof gpu_ms_accum);
                gpu_ms_frames = 0;
                memset(shadow_polys_accum, 0, sizeof shadow_polys_accum);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "shadowlod", 9))
        {
                // "shadowlod <blocks>" sets the LOD distance for far/extreme
                // shadow casters (-1 disables, 0 = all chunks); bare = query
                if (cmd[9] == ' ')
                        shadow_lod_dist = atof(cmd + 10);
                p += snprintf(p, end-p, "shadow_lod_dist %g\n", shadow_lod_dist);
        }
        else if (!strncmp(cmd, "gpu", 3))
        {
                if (!gpu_ms_frames)
                        p += snprintf(p, end-p, "no gpu samples yet (fps reset, wait a bit)\n");
                else
                {
                        unsigned n = gpu_ms_frames;
                        p += snprintf(p, end-p,
                                "frames %u\n"
                                "shadow_near_ms %.3f\nshadow_mid_ms %.3f\n"
                                "shadow_far_ms %.3f\nshadow_ext_ms %.3f\n"
                                "terrain_ms %.3f\npost_ms %.3f\n",
                                n,
                                gpu_ms_accum[GPU_TS_SHADOW_N_END] / n,
                                gpu_ms_accum[GPU_TS_SHADOW_M_END] / n,
                                gpu_ms_accum[GPU_TS_SHADOW_F_END] / n,
                                gpu_ms_accum[GPU_TS_SHADOW_X_END] / n,
                                gpu_ms_accum[GPU_TS_TERRAIN_END] / n,
                                gpu_ms_accum[GPU_TS_FRAME_END] / n);
                        p += snprintf(p, end-p,
                                "shadow_faces_per_frame n %llu m %llu f %llu x %llu\n",
                                shadow_polys_accum[SHADOW_NEAR] / n,
                                shadow_polys_accum[SHADOW_MID] / n,
                                (shadow_polys_accum[SHADOW_FAR_A] + shadow_polys_accum[SHADOW_FAR_B]) / n,
                                (shadow_polys_accum[SHADOW_EXT_A] + shadow_polys_accum[SHADOW_EXT_B]) / n);
                }
        }
        else if (!strncmp(cmd, "fps", 3))
        {
                float elapsed = (float)(SDL_GetPerformanceCounter() - fps_reset_count)
                        / SDL_GetPerformanceFrequency();
                int n = frame_ring_len;
                if (n < 1)
                {
                        p += snprintf(p, end-p, "no frames yet\n");
                }
                else
                {
                        float sorted[REMOTE_RING];
                        memcpy(sorted, frame_ring, n * sizeof *sorted);
                        qsort(sorted, n, sizeof *sorted, floatcmp);
                        float sum = 0.f;
                        for (int i = 0; i < n; i++) sum += sorted[i];
                        p += snprintf(p, end-p,
                                "frames %d\nelapsed_s %.3f\nfps %.1f\n"
                                "avg_ms %.3f\np50_ms %.3f\np99_ms %.3f\nworst_ms %.3f\n"
                                "meshes_built %d\nchunks_generated %d\ngen_ms %d\n"
                                "gen_pass hmap %d soil %d caves %d water %d trees %d light %d corners %d\n",
                                n, elapsed, n / elapsed, sum / n,
                                sorted[n/2], sorted[n*99/100], sorted[n-1],
                                nr_meshes_built - fps_base_meshes,
                                nr_chunks_generated - fps_base_chunks,
                                chunk_gen_ticks - fps_base_gen_ticks,
                                gen_pass_ms[GEN_HMAP],
                                gen_pass_ms[GEN_SOIL], gen_pass_ms[GEN_CAVES],
                                gen_pass_ms[GEN_WATER], gen_pass_ms[GEN_TREES],
                                gen_pass_ms[GEN_LIGHT], gen_pass_ms[GEN_CORNERS]);
                }
        }
        else if (!strncmp(cmd, "timings reset", 13))
        {
                memcpy(timer_base, timer_times, sizeof timer_base);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "timings", 7))
        {
                unsigned long long freq = SDL_GetPerformanceFrequency();
                unsigned long long sum = 0;
                for (int i = 0; i <= timer_; i++)
                        sum += timer_times[i] - timer_base[i];
                for (int i = 0; i <= timer_; i++)
                {
                        unsigned long long t = timer_times[i] - timer_base[i];
                        if (!t) continue;
                        p += snprintf(p, end-p, "%8.4f  %4.1f%%  %s\n",
                                (float)t / freq, 100.f * t / sum, timernamesprint[i]);
                }
        }
        else if (!strncmp(cmd, "pos", 3))
        {
                p += snprintf(p, end-p, "window_blocks %.2f %.2f %.2f\n"
                        "absolute_blocks %.2f %.2f\nscoot_chunks %d %d\n",
                        player[my_player].pos.x / BS, player[my_player].pos.y / BS, player[my_player].pos.z / BS,
                        player[my_player].pos.x / BS - scootx, player[my_player].pos.z / BS - scootz,
                        chunk_scootx, chunk_scootz);
        }
        else if (!strncmp(cmd, "save ", 5))
        {
                // append "<seed> <ax> <ay> <az> <name>" to blocko.saves so a
                // spot can be revisited later (set the seed, then tp there).
                // Coords are absolute blocks, matching pos/tp.
                const char *name = cmd + 5;
                while (*name == ' ') name++;
                if (!*name)
                        p += snprintf(p, end-p, "usage: save <name>\n");
                else
                {
                        int ax = player[my_player].pos.x / BS - scootx;
                        int ay = player[my_player].pos.y / BS;
                        int az = player[my_player].pos.z / BS - scootz;
                        FILE *f = fopen("blocko.saves", "a");
                        if (f)
                        {
                                fprintf(f, "%d %d %d %d %s\n", world_seed, ax, ay, az, name);
                                fclose(f);
                                p += snprintf(p, end-p, "saved %d %d %d %d %s\n",
                                        world_seed, ax, ay, az, name);
                        }
                        else
                                p += snprintf(p, end-p, "can't write blocko.saves\n");
                }
        }
        else if (!strncmp(cmd, "load", 4))
        {
                // "load <name>" restores a spot saved with `save`: set that
                // line's seed, regen the world, and tp there. "load" with no
                // name loads the most recent line. Names may contain spaces;
                // if several lines share a name, the latest one wins.
                const char *want = cmd + 4;
                while (*want == ' ') want++;
                FILE *f = fopen("blocko.saves", "r");
                if (!f)
                {
                        p += snprintf(p, end-p, "no blocko.saves\n");
                }
                else
                {
                        char line[512], best[512] = "";
                        while (fgets(line, sizeof line, f))
                        {
                                line[strcspn(line, "\n")] = '\0';
                                int s, ax, ay, az, off = 0;
                                if (sscanf(line, "%d %d %d %d %n", &s, &ax, &ay, &az, &off) < 4)
                                        continue;
                                if (!*want || !strcmp(line + off, want))
                                        snprintf(best, sizeof best, "%s", line);
                        }
                        fclose(f);

                        if (!best[0])
                        {
                                if (*want) p += snprintf(p, end-p, "no save named '%s'\n", want);
                                else       p += snprintf(p, end-p, "blocko.saves is empty\n");
                        }
                        else
                        {
                                int s, ax, ay, az, off = 0;
                                sscanf(best, "%d %d %d %d %n", &s, &ax, &ay, &az, &off);
                                world_seed = s;
                                // regen: invalidate every chunk's gen stamp
                                for (int i = 0; i < VAOD; i++) for (int j = 0; j < VAOW; j++)
                                {
                                        chunk_stamp[i][j].ax = INT_MIN;
                                        chunk_stamp[i][j].az = INT_MIN;
                                        chunk_estamp[i][j].ax = INT_MIN;
                                        chunk_estamp[i][j].az = INT_MIN;
                                }
                                // tp to the saved absolute location
                                player[my_player].pos.x = (ax + scootx) * BS;
                                player[my_player].pos.y = ay * BS;
                                player[my_player].pos.z = (az + scootz) * BS;
                                player[my_player].grav = GRAV_ZERO;
                                p += snprintf(p, end-p, "loaded %s\n", best);
                        }
                }
        }
        else if (!strncmp(cmd, "tp ", 3))
        {
                float a, b, c;
                int n = sscanf(cmd + 3, "%f %f %f", &a, &b, &c);
                float ax = a;
                float ay = (n == 3) ? b : 0;      // "tp x z" or "tp x y z"
                float az = (n == 3) ? c : b;
                if (n >= 2 && isfinite(ax) && isfinite(ay) && isfinite(az))
                {
                        // past ~2M blocks the scoot rebase (dx * BS) overflows int and
                        // the window can never catch the player; stay well inside that
                        ax = CLAMP(ax, -1000000.f, 1000000.f);
                        az = CLAMP(az, -1000000.f, 1000000.f);
                        player[my_player].pos.x = (ax + scootx) * BS;
                        player[my_player].pos.z = (az + scootz) * BS;
                        player[my_player].grav = GRAV_ZERO;
                        if (n == 3)
                        {
                                // explicit height: start exactly there instead of
                                // snapping to the ground. gravity still applies -
                                // pair with noclip to hold a camera in the air
                                player[my_player].pos.y = CLAMP(ay, 0.f, TILESH-1.f) * BS;
                                remote_tp_snap = 0;
                                p += snprintf(p, end-p, "ok %g %g %g\n", ax, ay, az);
                        }
                        else
                        {
                                player[my_player].pos.y = 0;
                                remote_tp_snap = 1; // land on the ground (remote_poll)
                                p += snprintf(p, end-p, "ok %g %g\n", ax, az);
                        }
                }
                else
                        p += snprintf(p, end-p, "usage: tp <abs_x> <abs_z>  or  tp <abs_x> <y> <abs_z>  (blocks)\n");
        }
        else if (!strncmp(cmd, "walk ", 5))
        {
                int frames = atoi(cmd + 5);
                remote_walk_frames = frames;
                player[my_player].goingf = frames > 0;
                player[my_player].running = frames > 0;
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "fly ", 4))
        {
                int frames; float speed, alt;
                int n = sscanf(cmd + 4, "%d %f %f", &frames, &speed, &alt);
                if (n >= 2)
                {
                        remote_fly_frames = frames;
                        remote_fly_speed = speed;
                        // default skims just above the sea so the shadow
                        // cascades stay full of terrain (flying near the sky
                        // makes shadow work look artificially cheap)
                        remote_fly_alt = (n == 3) ? alt : SEA_LEVEL - 6;
                        p += snprintf(p, end-p, "ok\n");
                }
                else
                        p += snprintf(p, end-p, "usage: fly <frames> <blocks_per_sec> [<y_blocks>]\n");
        }
        else if (!strncmp(cmd, "turn ", 5))
        {
                player[my_player].yaw = atof(cmd + 5) * PI / 180.f;
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "look", 4))
        {
                // "look" reports the aim; "look <yaw_deg> <pitch_deg>" points the
                // camera there (like moving the mouse). +pitch looks down, 0 is
                // level; yaw 0 faces +z, matching the on-screen compass.
                float yaw_deg, pitch_deg;
                if (sscanf(cmd + 4, "%f %f", &yaw_deg, &pitch_deg) == 2)
                {
                        float limit = PI * 0.5f - 0.001f;
                        player[my_player].yaw = yaw_deg * PI / 180.f;
                        player[my_player].pitch = CLAMP(pitch_deg * PI / 180.f, -limit, limit);
                }
                p += snprintf(p, end-p, "yaw_deg %.1f pitch_deg %.1f\n",
                        player[my_player].yaw * 180.f / PI, player[my_player].pitch * 180.f / PI);
        }
        else if (!strncmp(cmd, "click", 5))
        {
                // "click <left|right> [frames]" holds a mouse button for that many
                // rendered frames (default 1), then releases - the socket's stand
                // -in for clicking. left = break/mine (needs ~MINE_TIME held while
                // aimed at one block), right = place. frames 0 releases now.
                char btn[16] = ""; int frames = 1;
                int got = sscanf(cmd + 5, "%15s %d", btn, &frames);
                if (got >= 1 && (!strcmp(btn, "left") || !strcmp(btn, "right")))
                {
                        int down = frames > 0;
                        if (!strcmp(btn, "left"))
                        {
                                remote_break_frames = frames;
                                player[my_player].breaking = down;
                        }
                        else
                        {
                                remote_build_frames = frames;
                                player[my_player].building = down;
                        }
                        p += snprintf(p, end-p, "ok\n");
                }
                else
                        p += snprintf(p, end-p, "usage: click <left|right> [frames]\n");
        }
        else if (!strncmp(cmd, "patch", 5))
        {
                // report the reject+patch state (instant block edits + mining)
                int mining = (mine_frac > 0.f && mine_x >= 0);
                p += snprintf(p, end-p, "edit_pending %d mining %d\n", patch_active, mining);
                if (patch_box_on)
                        p += snprintf(p, end-p,
                                "box_abs %d %d %d .. %d %d %d\nverts %d\n",
                                patch_box_lo[0], patch_box_lo[1], patch_box_lo[2],
                                patch_box_hi[0], patch_box_hi[1], patch_box_hi[2], patch_vert_count);
                else
                        p += snprintf(p, end-p, "box off\n");
        }
        else if (!strncmp(cmd, "target", 6))
        {
                // report the block the player is aiming at (what a left click
                // mines) and where a right click would place, in absolute coords
                if (target_x >= 0)
                        p += snprintf(p, end-p, "target %d %d %d tile %d\n",
                                target_x - scootx, target_y, target_z - scootz,
                                T_(target_x, target_y, target_z));
                else
                        p += snprintf(p, end-p, "target none\n");
                if (place_x >= 0)
                        p += snprintf(p, end-p, "place %d %d %d\n",
                                place_x - scootx, place_y, place_z - scootz);
                else
                        p += snprintf(p, end-p, "place none\n");
        }
        else if (!strncmp(cmd, "dist ", 5))
        {
                draw_dist = atof(cmd + 5);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "tile ", 5))
        {
                // tile <ax> <ay> <az> - read one tile by absolute coords,
                // through the sim accessor: window first, then sim areas
                int ax, ay, az;
                if (sscanf(cmd + 5, "%d %d %d", &ax, &ay, &az) == 3)
                {
                        int t = sim_tile_abs(ax, ay, az);
                        if (t < 0)
                                p += snprintf(p, end-p, "out of window\n");
                        else
                                p += snprintf(p, end-p, "tile %d\n", t);
                }
                else
                        p += snprintf(p, end-p, "usage: tile <ax> <ay> <az>\n");
        }
        else if (!strncmp(cmd, "edits", 5))
        {
                // the edit overlay (edit.c): "edits" reports how many block
                // edits are recorded; "edits clear" forgets them (they'd
                // otherwise replay whenever their chunk regenerates)
                if (!strcmp(cmd + 5, " clear"))
                        edit_clear();
                p += snprintf(p, end-p, "edits %d\n", edit_len);
        }
        else if (!strncmp(cmd, "find ", 5))
        {
                // find <tile> <ax0> <az0> <ax1> <az1> - list matching blocks
                // in an absolute-coord rect (inclusive), as "x y z" lines
                int tile, ax0, az0, ax1, az1;
                if (sscanf(cmd + 5, "%d %d %d %d %d", &tile, &ax0, &az0, &ax1, &az1) == 5)
                {
                        int lines = 0;
                        for (int ax = ax0; ax <= ax1 && lines >= 0; ax++)
                        for (int az = az0; az <= az1 && lines >= 0; az++)
                        {
                                int x = ax + scootx;
                                int z = az + scootz;
                                if (x < 0 || x >= TILESW || z < 0 || z >= TILESD)
                                        continue;
                                for (int y = 0; y < TILESH; y++)
                                {
                                        if (T_(x, y, z) != tile) continue;
                                        if (end-p < 40) { p += snprintf(p, end-p, "truncated\n"); lines = -1; break; }
                                        p += snprintf(p, end-p, "%d %d %d\n", ax, y, az);
                                        lines++;
                                }
                        }
                }
                else
                        p += snprintf(p, end-p, "usage: find <tile> <ax0> <az0> <ax1> <az1>\n");
        }
        else if (!strncmp(cmd, "debounce ", 9))
        {
                remesh_debounce = atoi(cmd + 9);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "tint", 4))
        {
                // debug viz: tint the reject+patch mesh red so you can see it.
                // "tint" toggles; "tint 0|1" sets explicitly.
                int v;
                if (sscanf(cmd + 4, "%d", &v) == 1)
                        patch_tint = v;
                else
                        patch_tint = !patch_tint;
                p += snprintf(p, end-p, "patch_tint %d\n", patch_tint);
        }
        else if (!strncmp(cmd, "noise", 5))
        {
                // noise                      - show current knobs
                // noise <knob> <value>       - set a knob (kernel2/contrast/aniso/nvary)
                char which[32]; float val;
                if (sscanf(cmd + 5, "%31s %f", which, &val) == 2)
                {
                        if      (!strcmp(which, "kernel2"))  noise_kernel_sq = val;
                        else if (!strcmp(which, "contrast")) noise_base_weight = val;
                        else if (!strcmp(which, "aniso"))    noise_aniso = val;
                        else if (!strcmp(which, "nvary"))    noise_nvary = val;
                        else if (!strcmp(which, "interp"))   noise_interp = val;
                        noise_config_gen++; // stale memos refill
                        form_config_gen++;  // formations sit on the old surface
                }
                p += snprintf(p, end-p, "kernel2 %d\ncontrast %g\naniso %g\nnvary %d\ninterp %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        (int)noise_kernel_sq, noise_base_weight, noise_aniso,
                        (int)noise_nvary, (int)noise_interp);
        }
        else if (!strncmp(cmd, "formdump", 8))
        {
                // formdump [<path>] - reconstruct the voxel model of the nearest
                // formation from its column spans and write it (int W,H,D +
                // bytes, j=up) for offline rendering (scratchpad render.py)
                char path[256];
                snprintf(path, sizeof path, "/tmp/blocko-%s_form.bin", remote_tag());
                sscanf(cmd + 8, "%255s", path);
                int pax = player[my_player].pos.x / BS - scootx;
                int paz = player[my_player].pos.z / BS - scootz;
                int ci = pax >> FORM_CELL_BITS, cj = paz >> FORM_CELL_BITS;
                struct formation *f = NULL;
                for (int i = ci-1; i <= ci+1 && !f; i++)
                {
                        for (int j = cj-1; j <= cj+1 && !f; j++)
                        {
                                struct formation *g = get_formation(i, j);
                                if (g->nspan) f = g;
                        }
                }
                if (!f) { p += snprintf(p, end-p, "no formation near\n"); }
                else
                {
                        int ymin = TILESH, ymax = 0;
                        for (int k = 0; k < f->nspan; k++)
                        {
                                if (f->slo[k] < ymin) ymin = f->slo[k];
                                if (f->shi[k] > ymax) ymax = f->shi[k];
                        }
                        int Hh = ymax - ymin + 1;
                        static unsigned char buf[FBW * 260 * FBD];
                        if (Hh > 260) Hh = 260;
                        memset(buf, 0, (size_t)FBW * Hh * FBD);
                        for (int li = 0; li < FBW; li++) for (int lk = 0; lk < FBD; lk++)
                        {
                                int c = li * FBD + lk;
                                int cnt = f->colcnt[c], off = f->coloff[c];
                                for (int q = 0; q < cnt; q++)
                                {
                                        for (int y = f->slo[off+q]; y <= f->shi[off+q]; y++)
                                        {
                                                int jj = ymax - y; // world up (small y) -> big j
                                                if (jj < 0 || jj >= Hh) continue;
                                                buf[(li*Hh + jj)*FBD + lk] = 1;
                                        }
                                }
                        }
                        FILE *fp = fopen(path, "wb");
                        if (fp)
                        {
                                int d[3] = { FBW, Hh, FBD };
                                fwrite(d, sizeof d, 1, fp);
                                fwrite(buf, 1, (size_t)FBW * Hh * FBD, fp);
                                fclose(fp);
                                p += snprintf(p, end-p, "ok %s  %dx%dx%d spans %d\n",
                                        path, FBW, Hh, FBD, f->nspan);
                        }
                        else p += snprintf(p, end-p, "can't write %s\n", path);
                }
        }
        else if (!strncmp(cmd, "form near", 9))
        {
                // form near [<radius>] - list formations near the player as
                // "x z spheres above_sea" lines (absolute block coords)
                int radius = atoi(cmd + 9);
                if (radius <= 0) radius = 512;
                int pax = player[my_player].pos.x / BS - scootx;
                int paz = player[my_player].pos.z / BS - scootz;
                int c0 = (pax - radius) >> FORM_CELL_BITS, c1 = (pax + radius) >> FORM_CELL_BITS;
                int d0 = (paz - radius) >> FORM_CELL_BITS, d1 = (paz + radius) >> FORM_CELL_BITS;
                int found = 0;
                for (int i = c0; i <= c1; i++) for (int j = d0; j <= d1; j++)
                {
                        struct formation *f = get_formation(i, j);
                        if (!f->nspan) continue;
                        int top = TILESH; // smallest world Y (= highest point)
                        for (int k = 0; k < f->nspan; k++)
                                if (f->slo[k] < top) top = f->slo[k];
                        if (end-p < 60) { p += snprintf(p, end-p, "truncated\n"); goto form_done; }
                        p += snprintf(p, end-p, "%d %d spheres %d above_sea %d\n",
                                f->bx + FBW/2, f->bz + FBD/2, f->n, SEA_LEVEL - top);
                        found++;
                }
                if (!found)
                        p += snprintf(p, end-p, "none within %d blocks\n", radius);
                form_done: ;
        }
        else if (!strncmp(cmd, "form", 4))
        {
                // form                 - show current knobs
                // form <knob> <value>  - set a knob
                char which[32]; float val;
                if (sscanf(cmd + 4, "%31s %f", which, &val) == 2)
                {
                        if      (!strcmp(which, "enable")) form_enable = val;
                        else if (!strcmp(which, "region")) form_region = val;
                        else if (!strcmp(which, "chance")) form_chance = val;
                        else if (!strcmp(which, "steps"))  form_steps = val;
                        else if (!strcmp(which, "rmin"))   form_rmin = val;
                        else if (!strcmp(which, "rmax"))   form_rmax = val;
                        else if (!strcmp(which, "detail")) form_detail = val;
                        form_config_gen++; // stale memos refill
                }
                p += snprintf(p, end-p, "enable %d\nregion %g\nchance %g\n"
                        "steps %d\nrmin %g\nrmax %g\ndetail %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        (int)form_enable, form_region, form_chance,
                        (int)form_steps, form_rmin, form_rmax, (int)form_detail);
        }
        else if (!strncmp(cmd, "caves", 5))
        {
                int v;
                if (sscanf(cmd + 5, "%d", &v) == 1)
                        cave_enable = v;
                p += snprintf(p, end-p, "caves %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        cave_enable);
        }
        else if (!strncmp(cmd, "trees", 5))
        {
                int v;
                if (sscanf(cmd + 5, "%d", &v) == 1)
                        tree_enable = v;
                p += snprintf(p, end-p, "trees %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        tree_enable);
        }
        else if (!strncmp(cmd, "seed", 4))
        {
                int v;
                if (sscanf(cmd + 4, "%d", &v) == 1)
                {
                        world_seed = v;
                        edit_clear(); // new seed = new world; old edits are meaningless
                }
                p += snprintf(p, end-p, "seed %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        world_seed);
        }
        else if (!strncmp(cmd, "terrace", 7))
        {
                float f; int v;
                if (sscanf(cmd + 7, " jitter %f", &f) == 1)
                        TERRACE_JITTER_AMP = f;
                else if (sscanf(cmd + 7, "%d", &v) == 1)
                        TERRACE_ENABLE = v;
                p += snprintf(p, end-p, "terrace %d  jitter %.3f\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        (int)TERRACE_ENABLE, TERRACE_JITTER_AMP);
        }
        else if (!strncmp(cmd, "grassshadow", 11))
        {
                // toggle tall grass shadows (same as the T key)
                int v;
                if (sscanf(cmd + 11, "%d", &v) == 1)
                        grass_shadows = v;
                else
                        grass_shadows = !grass_shadows;
                p += snprintf(p, end-p, "grassshadow %d\n", grass_shadows);
        }
        else if (!strncmp(cmd, "screenshot", 10))
        {
                // grab the last presented frame to a PNG. Runs between frames
                // (remote_poll is called before draw_stuff), so it captures the
                // previous fully-rendered frame. Default path is per-worktree.
                char path[256];
                snprintf(path, sizeof path, "/tmp/blocko-%s_shot.png", remote_tag());
                sscanf(cmd + 10, "%255s", path);
                if (headless)
                        p += snprintf(p, end-p, "headless: nothing rendered\n");
                else
                {
                        vulkan_request_screenshot(path);
                        p += snprintf(p, end-p, "ok %s\n", path);
                }
        }
        else if (!strncmp(cmd, "pmedit", 6))
        {
                // toggle the model editor, exactly like the U key
                if (headless)
                        p += snprintf(p, end-p, "headless: no editor\n");
                else
                {
                        pmedit_toggle();
                        p += snprintf(p, end-p, "pmedit %d\n", pmedit_on);
                }
        }
        else if (!strncmp(cmd, "pmpick", 6))
        {
                // drive the model picker (needs the editor open): "pmpick"
                // opens it, "pmpick N" chooses cell N, "pmpick next|prev"
                // pages, "pmpick close" closes it
                if (headless)
                        p += snprintf(p, end-p, "headless: no editor\n");
                else
                {
                        const char *a = cmd + 6;
                        while (*a == ' ') a++;
                        int on = pmedit_remote_pick(a);
                        p += snprintf(p, end-p, "pmpick picker=%d\n", on);
                }
        }
        else if (!strncmp(cmd, "noclip", 6))
        {
                // fly through solids with no gravity; jump rises, sneak sinks
                int v;
                if (sscanf(cmd + 6, "%d", &v) == 1)
                        player[my_player].noclip = v;
                else
                        player[my_player].noclip = !player[my_player].noclip;
                p += snprintf(p, end-p, "noclip %d\n", player[my_player].noclip);
        }
        else if (!strncmp(cmd, "cull", 4))
        {
                // freeze/unfreeze chunk culling (same as the F2 key)
                int v;
                if (sscanf(cmd + 4, "%d", &v) == 1)
                        lock_culling = v;
                int cam = 0, x0 = VAOW, x1 = -1, z0 = VAOD, z1 = -1;
                for (int k = 0; k < visible_chunk_count; k++)
                {
                        if (!visible_chunks[k].camera_visible) continue;
                        cam++;
                        if (visible_chunks[k].x < x0) x0 = visible_chunks[k].x;
                        if (visible_chunks[k].x > x1) x1 = visible_chunks[k].x;
                        if (visible_chunks[k].z < z0) z0 = visible_chunks[k].z;
                        if (visible_chunks[k].z > z1) z1 = visible_chunks[k].z;
                }
                p += snprintf(p, end-p, "lock_culling %d camera_chunks %d total_chunks %d "
                        "bounds x %d..%d z %d..%d\n",
                        lock_culling, cam, visible_chunk_count, x0, x1, z0, z1);
        }
        else if (!strncmp(cmd, "freeze", 6))
        {
                // pin the shadow maps + sun where they are (same as the F6 key)
                // so the cascades stay anchored and you can inspect their edges
                int v;
                if (sscanf(cmd + 6, "%d", &v) == 1)
                        freeze_shadows = v;
                else
                        freeze_shadows = !freeze_shadows;
                p += snprintf(p, end-p, "freeze_shadows %d\n", freeze_shadows);
        }
        else if (!strncmp(cmd, "dump", 4))
        {
                // raw world arrays to a file, for offline diffing; the default
                // path is per-worktree (matches the socket) so parallel game
                // copies don't clobber each other's dumps
                char path[256];
                snprintf(path, sizeof path, "/tmp/blocko-%s_dump.bin", remote_tag());
                sscanf(cmd + 4, "%255s", path);
                FILE *f = fopen(path, "wb");
                if (f)
                {
                        fwrite(tiles, 1, TILESD * TILESH * TILESW, f);
                        fwrite(gndheight, 1, TILESW * TILESD, f);
                        fclose(f);
                        p += snprintf(p, end-p, "ok %s\n", path);
                }
                else
                        p += snprintf(p, end-p, "can't write %s\n", path);
        }
        else if (!strncmp(cmd, "flat", 4))
        {
                // dead-flat test world (send regen after) - isolates the chunk
                // seams that terrain relief otherwise hides
                int v;
                if (sscanf(cmd + 4, "%d", &v) == 1) flat_world = v;
                else flat_world = !flat_world;
                p += snprintf(p, end-p, "flat_world %d (send regen)\n", flat_world);
        }
        else if (!strncmp(cmd, "sum", 3))
        {
                // FNV-1a over the raw world arrays, for A/B-ing gen changes
                unsigned th = 2166136261u, sh = 2166136261u, gh = 2166136261u;
                for (int i = 0; i < TILESD * TILESH * TILESW; i++)
                        th = (th ^ tiles[i]) * 16777619u;
                for (int i = 0; i < TILESD * TILESH * TILESW; i++)
                        sh = (sh ^ sunlight[i]) * 16777619u;
                for (int i = 0; i < TILESW * TILESD; i++)
                        gh = (gh ^ gndheight[i]) * 16777619u;
                p += snprintf(p, end-p, "tiles %08x sun %08x gndh %08x\n", th, sh, gh);
        }
        else if (!strncmp(cmd, "csum ", 5))
        {
                // csum <acx> <acz> - FNV-1a over ONE chunk's tiles + gndheight,
                // by absolute chunk coords: compares the same piece of world
                // across instances whose windows sit at different scoots.
                // Chunks outside the window answer from a sim area copy when
                // one holds them (same absolute iteration order = same hash)
                int acx, acz;
                if (sscanf(cmd + 5, "%d %d", &acx, &acz) == 2)
                {
                        int wcx = acx + chunk_scootx, wcz = acz + chunk_scootz;
                        int in_win = wcx >= 0 && wcx < VAOW && wcz >= 0 && wcz < VAOD;
                        struct warea *a = (in_win && AGEN_(wcx, wcz)) ? NULL
                                : sim_area_with_chunk(acx, acz);
                        if (in_win && AGEN_(wcx, wcz))
                        {
                                unsigned th = 2166136261u, gh = 2166136261u;
                                int x0 = wcx * CHUNKW, z0 = wcz * CHUNKD;
                                for (int z = z0; z < z0 + CHUNKD; z++)
                                for (int x = x0; x < x0 + CHUNKW; x++)
                                {
                                        for (int y = 0; y < TILESH; y++)
                                                th = (th ^ T_(x, y, z)) * 16777619u;
                                        gh = (gh ^ (unsigned)GNDH_(x, z)) * 16777619u;
                                }
                                p += snprintf(p, end-p, "csum %d %d tiles %08x gndh %08x\n",
                                        acx, acz, th, gh);
                        }
                        else if (a)
                        {
                                unsigned th = 2166136261u, gh = 2166136261u;
                                int x0 = acx * CHUNKW, z0 = acz * CHUNKD;
                                for (int z = z0; z < z0 + CHUNKD; z++)
                                for (int x = x0; x < x0 + CHUNKW; x++)
                                {
                                        for (int y = 0; y < TILESH; y++)
                                                th = (th ^ AREA_T(a, x, y, z)) * 16777619u;
                                        gh = (gh ^ (unsigned)AREA_GNDH(a, x, z)) * 16777619u;
                                }
                                p += snprintf(p, end-p, "csum %d %d tiles %08x gndh %08x\n",
                                        acx, acz, th, gh);
                        }
                        else if (in_win)
                                p += snprintf(p, end-p, "not generated\n");
                        else
                                p += snprintf(p, end-p, "out of window\n");
                }
                else
                        p += snprintf(p, end-p, "usage: csum <acx> <acz>\n");
        }
        else if (!strncmp(cmd, "tweak", 5))
        {
                // the big world-gen knob table (tweak.c) - same knobs as the
                // in-game K panel. sets regenerate the world on their own.
                const char *a = cmd + 5;
                while (*a == ' ') a++;
                tweak_dispatch(a, p, end-p);
                p += strlen(p);
        }
        else if (!strncmp(cmd, "regen", 5))
        {
                regen_world();
                p += snprintf(p, end-p, "ok - world regenerating\n");
        }
        else if (!strncmp(cmd, "serve", 5))
        {
                // host a multiplayer game (net.c); default port if omitted
                int port = NET_PORT;
                sscanf(cmd + 5, "%d", &port);
                if (net_serve(port) == 0)
                        p += snprintf(p, end-p, "serving on port %d\n", port);
                else
                        p += snprintf(p, end-p, "can't serve (already on? port taken?)\n");
        }
        else if (!strncmp(cmd, "connect ", 8))
        {
                // join a multiplayer game: connect <host> [port]
                char host[128];
                int port = NET_PORT;
                if (sscanf(cmd + 8, "%127s %d", host, &port) >= 1)
                {
                        if (net_connect(host, port) == 0)
                                p += snprintf(p, end-p, "connected to %s:%d as player %d\n",
                                        host, port, my_player);
                        else
                                p += snprintf(p, end-p, "can't connect to %s:%d\n", host, port);
                }
                else
                        p += snprintf(p, end-p, "usage: connect <host> [port]\n");
        }
        else if (!strncmp(cmd, "net", 3))
        {
                p += net_describe(p, end-p);
        }
        else if (!strncmp(cmd, "say ", 4))
        {
                // speak in chat, as if typed with T
                char line[300];
                snprintf(line, sizeof line, "<player %d> %s", my_player, cmd + 4);
                chat_add(line);
                net_send_chat(cmd + 4);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "sun ", 4))
        {
                // "sun <pitch>" freezes the sun there; "sun run" resumes motion
                if (!strcmp(cmd + 4, "run")) {
                        sun_frozen = 0;
                } else {
                        sun_pitch = atof(cmd + 4);
                        sun_frozen = 1;
                }
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "mob", 3))
        {
                // "mob"          - report living slimes and kill count
                // "mob spawn"    - force a slime to spawn near the player
                // "mob <0|1>"    - toggle auto-spawning; 0 also despawns the
                //                  living ones (an off-switch, not a valve)
                char arg[32] = "";
                sscanf(cmd + 3, "%31s", arg);
                if (!strcmp(arg, "spawn"))
                {
                        int pax = player[my_player].pos.x / BS;
                        int paz = player[my_player].pos.z / BS;
                        int ok = 0;
                        for (int r = 3; r < 20 && !ok; r++)
                                ok = mob_spawn(pax + r, paz) || mob_spawn(pax - r, paz)
                                   || mob_spawn(pax, paz + r) || mob_spawn(pax, paz - r);
                        p += snprintf(p, end-p, ok ? "spawned\n" : "no room to spawn\n");
                }
                else if (arg[0] == '0' || arg[0] == '1')
                {
                        mob_enable = arg[0] - '0';
                        if (!mob_enable)
                                for (int i = 0; i < NR_MOBS; i++)
                                        mob[i].alive = 0;
                        p += snprintf(p, end-p, "mob_enable %d\n", mob_enable);
                }
                int live = 0;
                for (int i = 0; i < NR_MOBS; i++) if (mob[i].alive) live++;
                p += snprintf(p, end-p, "living %d kills %d enable %d\n",
                        live, mob_kills, mob_enable);
                for (int i = 0; i < NR_MOBS; i++)
                {
                        if (!mob[i].alive) continue;
                        if (end-p < 60) break;
                        p += snprintf(p, end-p, "  mob %d at %.1f %.1f %.1f size %d\n", i,
                                mob[i].pos.x / BS - scootx, mob[i].pos.y / BS,
                                mob[i].pos.z / BS - scootz, mob[i].size);
                }
        }
        else if (!strncmp(cmd, "redirty", 7))
        {
                // mark every chunk mesh dirty, to force a full remesh pass
                // (for measuring rebuild cost under controlled, equal work)
                for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
                        chunk_dirty[i][j] = 1;
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "meshthr", 7))
        {
                // set the mesh-build OpenMP thread cap (persists)
                int n;
                if (sscanf(cmd + 7, "%d", &n) == 1)
                        mesh_threads = ICLAMP(n, 1, MAX_MESH_THREADS);
                p += snprintf(p, end-p, "mesh_threads %d\n", mesh_threads);
        }
        else if (!strncmp(cmd, "spike", 5))
        {
                // spike [w] [d] [h] [reps] [threads] - time meshing a w*d*h
                // region of cells at the player, to compare rebuild cost vs
                // region size (and vs OpenMP thread count)
                int w_ = CHUNKW, d_ = 0, h_ = TILESH, reps = 50, threads = 0;
                int got = sscanf(cmd + 5, "%d %d %d %d %d", &w_, &d_, &h_, &reps, &threads);
                if (got < 2) d_ = w_;
                if (got < 3) h_ = TILESH;
                if (got < 4) reps = 50;
                w_ = ICLAMP(w_, 1, CHUNKW);
                d_ = ICLAMP(d_, 1, CHUNKD);
                h_ = ICLAMP(h_, 1, TILESH);
                int orig_threads = mesh_threads;
                if (threads > 0) mesh_threads = threads;

                // center the region on the player, clamped inside the window
                int px = player[my_player].pos.x / BS;
                int pz = player[my_player].pos.z / BS;
                int xlo = ICLAMP(px - w_/2, 0, TILESW - w_);
                int zlo = ICLAMP(pz - d_/2, 0, TILESD - d_);
                int ylo = ICLAMP(TILESH/2 - h_/2, 0, TILESH - h_);

                unsigned long long freq = SDL_GetPerformanceFrequency();
                unsigned long long t0 = SDL_GetPerformanceCounter();
                for (int r = 0; r < reps; r++)
                        mesh_region(xlo, xlo + w_, ylo, ylo + h_, zlo, zlo + d_, FACE_ALL, xlo, zlo);
                unsigned long long t1 = SDL_GetPerformanceCounter();

                int used_threads = mesh_threads;
                if (threads > 0) mesh_threads = orig_threads;

                float ms = 1000.f * (t1 - t0) / freq / reps;
                p += snprintf(p, end-p,
                        "region %dx%dx%d = %d cells, threads %d\n%.3f ms/build (%d reps)\n"
                        "verts %ld opaque + %ld water\n",
                        w_, d_, h_, w_ * d_ * h_, used_threads,
                        ms, reps, (long)(v - vbuf), (long)(w - wbuf));
        }
        else if (!strncmp(cmd, "unlock", 6))
        {
                test_lock = 0;
                p += snprintf(p, end-p, "unlocked\n");
        }
        else if (!strncmp(cmd, "lock", 4))
        {
                // "lock 0" unlocks; "lock <any message>" locks and shows it
                // in the banner, so test scripts can report their progress
                const char *m = cmd + 4;
                while (*m == ' ') m++;
                if (!strcmp(m, "0"))
                        test_lock = 0;
                else
                {
                        test_lock = 1;
                        snprintf(test_lock_msg, sizeof test_lock_msg, "%s", m);
                        // free the mouse if it's captured, like hitting escape,
                        // so the user gets their cursor back while locked out
                        if (mouselook)
                        {
                                SDL_SetWindowRelativeMouseMode(vk.window, false);
                                mouselook = false;
                        }
                }
                p += snprintf(p, end-p, "lock %d\n", test_lock);
        }
        else if (!strncmp(cmd, "quit", 4))
        {
                // shut down after the reply is written - directly, not via
                // SDL_EVENT_QUIT, which the test lock swallows
                remote_want_quit = 1;
                p += snprintf(p, end-p, "ok\n");
        }
        else
        {
                // one line per group so it fits the on-screen console
                p += snprintf(p, end-p, "commands:\n"
                        "fps [reset] | timings [reset] | gpu | pos | tp <ax> <az>\n"
                        "save <name> | load [<name>]\n"
                        "walk <frames> | fly <frames> <bl/s> | turn <deg>\n"
                        "look [<yaw> <pitch>] | click <left|right> [frames] | target | patch\n"
                        "dist <blocks> | debounce <frames> | tint [<0|1>] | shadowlod [<blocks>]\n"
                        "screenshot [<path>] | noclip [<0|1>]\n"
                        "find <tile> <ax0> <az0> <ax1> <az1> | tile <ax> <ay> <az> | edits [clear]\n"
                        "tweak [<name> [<val>]|reset|dump] | noise [<knob> <val>] | form [near <r>|<knob> <val>]\n"
                        "caves [<0|1>] | trees [<0|1>] | flat [<0|1>] | seed [<n>] | sum | csum <acx> <acz> | dump [<path>]\n"
                        "cull [<0|1>] | freeze [<0|1>] | lock [<msg>|0] | regen | sun <pitch> | quit\n"
                        "serve [<port>] | connect <host> [<port>] | net | say <text>\n");
        }
}

#ifndef _WIN32
void remote_reply(int fd, const char *cmd)
{
        char out[8000];
        remote_dispatch(cmd, out, sizeof out);
        write(fd, out, strlen(out));
        if (remote_want_quit)
                game_shutdown(0);
}
#endif

// call once per rendered frame from the main loop
void remote_poll()
{
        // frame time bookkeeping
        unsigned long long now = SDL_GetPerformanceCounter();
        if (frame_prev_count && frame_ring_len < REMOTE_RING)
                frame_ring[frame_ring_len++] = (float)(now - frame_prev_count)
                        * 1000.f / SDL_GetPerformanceFrequency();
        frame_prev_count = now;

        // tp lands directly on the ground: the destination may not be
        // generated yet, so hold the player at the sky (where they can't
        // fall or get stuck) until the column has terrain, then plant
        // their feet on it
        if (remote_tp_snap)
        {
                struct player *pl = &player[my_player];
                int gnd = sim_gndh((pl->pos.x + PLYR_W/2) / BS,
                                   (pl->pos.z + PLYR_W/2) / BS);
                pl->pos.y = gnd ? gnd * BS - PLYR_H - 1 : 0;
                pl->grav = GRAV_ZERO;
                if (gnd) remote_tp_snap = 0;
        }

        // auto-walk countdown
        if (remote_walk_frames > 0 && --remote_walk_frames == 0)
        {
                player[my_player].goingf = 0;
                player[my_player].running = 0;
        }

        // release "held" mouse buttons once their frame budget runs out
        if (remote_break_frames > 0 && --remote_break_frames == 0)
                player[my_player].breaking = 0;
        if (remote_build_frames > 0 && --remote_build_frames == 0)
                player[my_player].building = 0;

        // noclip flight: constant speed along yaw at fixed altitude
        if (remote_fly_frames > 0)
        {
                remote_fly_frames--;
                player[my_player].pos.x += sinf(player[my_player].yaw) * remote_fly_speed * BS / 60.f;
                player[my_player].pos.z += cosf(player[my_player].yaw) * remote_fly_speed * BS / 60.f;
                player[my_player].pos.y = remote_fly_alt * BS;
                player[my_player].grav = GRAV_ZERO;
        }

#ifndef _WIN32
        if (remote_listen_fd < 0) return;

        if (remote_client_fd < 0)
        {
                remote_client_fd = accept(remote_listen_fd, NULL, NULL);
                if (remote_client_fd < 0) return;
                fcntl(remote_client_fd, F_SETFL, O_NONBLOCK);
                remote_cmd_len = 0;
        }

        for (;;)
        {
                ssize_t got = read(remote_client_fd, remote_cmd + remote_cmd_len,
                        sizeof remote_cmd - 1 - remote_cmd_len);

                if (got > 0)
                {
                        remote_cmd_len += got;
                        remote_cmd[remote_cmd_len] = '\0';
                        char *nl = strchr(remote_cmd, '\n');
                        if (nl || remote_cmd_len >= sizeof remote_cmd - 1)
                        {
                                if (nl) *nl = '\0';
                                remote_reply(remote_client_fd, remote_cmd);
                                close(remote_client_fd);
                                remote_client_fd = -1;
                                return;
                        }
                }
                else if (got == 0) // client hung up without a full line
                {
                        close(remote_client_fd);
                        remote_client_fd = -1;
                        return;
                }
                else
                {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                                close(remote_client_fd);
                                remote_client_fd = -1;
                        }
                        return; // no complete line yet - try again next frame
                }
        }
#endif // _WIN32
}

// ---- in-game console & chat ----
//
// Minecraft-style keys: T opens chat, / opens chat with the slash typed, and
// a line starting with / runs as a command (its reply lands in the chat log).
// The tilde console still works as before - everything typed there is a
// command, no slash needed, and it stays open showing the full reply.

int console_open;
int console_chat;        // opened for chat: plain lines are chat, /lines are commands
int console_opened_frame; // swallow the opening keystroke's own text event
char console_input[256];
size_t console_input_len;
char console_reply[8000];

// recent chat, shown even with the console closed, newest at the bottom
#define CHAT_LINES 8
#define CHAT_SHOW_FRAMES 600 // ~10 seconds
char chat_log[CHAT_LINES][300];
int chat_expire[CHAT_LINES];
int chat_head;

// append one line to the chat log (printable ascii only - this also takes
// lines straight off the network, so strip anything that could fake a line)
void chat_add(const char *s)
{
        char *d = chat_log[chat_head], *end = d + sizeof chat_log[0] - 1;
        for (; *s && d < end; s++)
                if (*s >= ' ' && *s <= '~')
                        *d++ = *s;
        *d = '\0';
        if (headless) // the terminal is the chat display
        {
                printf("%s\n", chat_log[chat_head]);
                fflush(stdout); // don't lag when piped to a log
        }
        chat_expire[chat_head] = frame + CHAT_SHOW_FRAMES;
        chat_head = (chat_head + 1) % CHAT_LINES;
}

static void console_show(int chat, const char *prefill)
{
        console_open = 1;
        console_chat = chat;
        console_opened_frame = frame;
        snprintf(console_input, sizeof console_input, "%s", prefill);
        console_input_len = strlen(console_input);
        SDL_StartTextInput(vk.window);
        // drop any held movement so the player doesn't run off
        player[my_player].goingf = player[my_player].goingb = 0;
        player[my_player].goingl = player[my_player].goingr = 0;
        player[my_player].running = player[my_player].sneaking = 0;
}

static void console_hide()
{
        console_open = 0;
        SDL_StopTextInput(vk.window);
}

void console_toggle()
{
        if (console_open)
                console_hide();
        else
                console_show(0, "");
}

// submit the typed line: chat goes out as chat, /commands (and everything in
// the tilde console) run through the same dispatcher as the socket
static void console_submit()
{
        if (console_chat && console_input[0] != '/')
        {
                char line[300];
                snprintf(line, sizeof line, "<player %d> %s", my_player, console_input);
                chat_add(line);
                net_send_chat(console_input);
        }
        else
        {
                remote_dispatch(console_input + (console_input[0] == '/'),
                        console_reply, sizeof console_reply);
                if (remote_want_quit)
                        game_shutdown(0);
                if (console_chat)
                {
                        // a /command from chat: its reply reads out as chat lines
                        for (char *r = console_reply; r && *r; )
                        {
                                char *nl = strchr(r, '\n');
                                if (nl) *nl = '\0';
                                if (*r) chat_add(r);
                                r = nl ? nl + 1 : NULL;
                        }
                        console_reply[0] = '\0';
                }
        }
        console_input_len = 0;
        console_input[0] = '\0';
}

// key events go here first; returns 1 if the console consumed the key
int console_key(int down)
{
        if (event.key.key == SDLK_GRAVE)
        {
                if (down && !event.key.repeat)
                        console_toggle();
                return 1;
        }
        if (!console_open)
        {
                if (down && !event.key.repeat && event.key.key == SDLK_T)
                        { console_show(1, ""); return 1; }
                if (down && !event.key.repeat && event.key.key == SDLK_SLASH)
                        { console_show(1, "/"); return 1; }
                return 0;
        }
        if (down) switch (event.key.key)
        {
                case SDLK_ESCAPE:
                        console_hide();
                        break;
                case SDLK_BACKSPACE:
                        if (console_input_len)
                                console_input[--console_input_len] = '\0';
                        break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                        if (event.key.repeat)
                                break;
                        if (console_input_len)
                                console_submit();
                        if (console_chat)
                                console_hide(); // chat closes on send, like Minecraft
                        break;
        }
        return 1;
}

void console_text(const char *text)
{
        if (!console_open) return;
        if (frame == console_opened_frame)
                return; // the keystroke that opened the box isn't input
        for (; *text; text++)
        {
                if (*text < ' ' || *text > '~' || *text == '`' || *text == '~')
                        continue; // printable ascii only; tilde toggles, never types
                if (console_input_len < sizeof console_input - 1)
                {
                        console_input[console_input_len++] = *text;
                        console_input[console_input_len] = '\0';
                }
        }
}

void console_draw()
{
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1.f) scale = 1.f;
        int lh = FONT_CH_H * scale;
        int inputy = screenh - 3.5f * lh; // above the F3 coords readout (0.955 * screenh)
        int base = inputy; // chat stacks upward from here

        if (console_open)
        {
                char line[300];
                snprintf(line, sizeof line, "> %s_", console_input);
                font_begin(screenw, screenh);
                font_add_text(line, 20, inputy, 0);
                font_end(1.f, 1.f, .5f);

                if (!console_chat && console_reply[0])
                {
                        // reply sits above the input line, bottom-anchored,
                        // tail if it's long; chat goes above the reply
                        char *r = console_reply;
                        int lines = 1;
                        for (char *s = r; *s; s++)
                                if (*s == '\n' && s[1]) lines++;
                        while (lines > 32) { r = strchr(r, '\n') + 1; lines--; }
                        base = inputy - (lines + 1) * lh;
                        font_begin(screenw, screenh);
                        font_add_text(r, 20, base, 0);
                        font_end(1.f, 1.f, 1.f);
                }
        }

        // recent chat: always while the console is up, else until it expires
        int rows[CHAT_LINES], shown = 0;
        for (int k = 0; k < CHAT_LINES; k++)
        {
                int i = (chat_head + k) % CHAT_LINES; // oldest first
                if (!chat_log[i][0]) continue;
                if (!console_open && frame > chat_expire[i]) continue;
                rows[shown++] = i;
        }
        if (!shown) return;
        font_begin(screenw, screenh);
        for (int k = 0; k < shown; k++)
                font_add_text(chat_log[rows[k]], 20, base - (shown + 1 - k) * lh, 0);
        font_end(1.f, 1.f, 1.f);
}

#endif // BLOCKO_REMOTE_C_INCLUDED
