#include "blocko.c"
#ifndef BLOCKO_REMOTE_C_INCLUDED
#define BLOCKO_REMOTE_C_INCLUDED

// remote.c - debug/automation socket + in-game console
//
// A unix socket for reading timings and poking at the running game, so tools
// (or an AI agent) can benchmark and drive the game without the keyboard.
// One command per connection: connect, send a line, read reply, disconnected.
//
//   echo fps     | nc -U /tmp/blocko.sock
//   echo "tp 200 -1500" | nc -U /tmp/blocko.sock
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
int remote_fly_frames;  // noclip along yaw for this many frames...
float remote_fly_speed; // ...at this many blocks/sec (deterministic traversal)

#ifdef _WIN32
void remote_init() { fps_reset_count = SDL_GetPerformanceCounter(); }
#else
void remote_init()
{
        const char *path = getenv("BLOCKO_SOCK");
        if (!path) path = "/tmp/blocko.sock";

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

        if (!strncmp(cmd, "fps reset", 9))
        {
                frame_ring_len = 0;
                fps_reset_count = SDL_GetPerformanceCounter();
                fps_base_meshes = nr_meshes_built;
                fps_base_chunks = nr_chunks_generated;
                fps_base_gen_ticks = chunk_gen_ticks;
                memset(gen_pass_ms, 0, sizeof gen_pass_ms);
                p += snprintf(p, end-p, "ok\n");
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
                        player[0].pos.x / BS, player[0].pos.y / BS, player[0].pos.z / BS,
                        player[0].pos.x / BS - scootx, player[0].pos.z / BS - scootz,
                        chunk_scootx, chunk_scootz);
        }
        else if (!strncmp(cmd, "tp ", 3))
        {
                float ax, az;
                if (sscanf(cmd + 3, "%f %f", &ax, &az) == 2)
                {
                        player[0].pos.x = (ax + scootx) * BS;
                        player[0].pos.z = (az + scootz) * BS;
                        player[0].pos.y = 0; // fall from the sky
                        player[0].grav = GRAV_ZERO;
                        p += snprintf(p, end-p, "ok\n");
                }
                else
                        p += snprintf(p, end-p, "usage: tp <abs_x_blocks> <abs_z_blocks>\n");
        }
        else if (!strncmp(cmd, "walk ", 5))
        {
                int frames = atoi(cmd + 5);
                remote_walk_frames = frames;
                player[0].goingf = frames > 0;
                player[0].running = frames > 0;
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "fly ", 4))
        {
                int frames; float speed;
                if (sscanf(cmd + 4, "%d %f", &frames, &speed) == 2)
                {
                        remote_fly_frames = frames;
                        remote_fly_speed = speed;
                        p += snprintf(p, end-p, "ok\n");
                }
                else
                        p += snprintf(p, end-p, "usage: fly <frames> <blocks_per_sec>\n");
        }
        else if (!strncmp(cmd, "turn ", 5))
        {
                player[0].yaw = atof(cmd + 5) * PI / 180.f;
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "dist ", 5))
        {
                draw_dist = atof(cmd + 5);
                p += snprintf(p, end-p, "ok\n");
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
                        noise_kernel_sq, noise_base_weight, noise_aniso, noise_nvary,
                        noise_interp);
        }
        else if (!strncmp(cmd, "form near", 9))
        {
                // form near [<radius>] - list formations near the player as
                // "x z spheres above_sea" lines (absolute block coords)
                int radius = atoi(cmd + 9);
                if (radius <= 0) radius = 512;
                int pax = player[0].pos.x / BS - scootx;
                int paz = player[0].pos.z / BS - scootz;
                int c0 = (pax - radius) >> FORM_CELL_BITS, c1 = (pax + radius) >> FORM_CELL_BITS;
                int d0 = (paz - radius) >> FORM_CELL_BITS, d1 = (paz + radius) >> FORM_CELL_BITS;
                int found = 0;
                for (int i = c0; i <= c1; i++) for (int j = d0; j <= d1; j++)
                {
                        struct formation *f = get_formation(i, j);
                        if (!f->n) continue;
                        float top = TILESH;
                        for (int k = 0; k < f->n; k++)
                                if (f->y[k] - f->r[k] * f->sq[k] < top)
                                        top = f->y[k] - f->r[k] * f->sq[k];
                        if (end-p < 60) { p += snprintf(p, end-p, "truncated\n"); goto form_done; }
                        p += snprintf(p, end-p, "%d %d spheres %d above_sea %d\n",
                                (int)f->x[0], (int)f->z[0], f->n, SEA_LEVEL - (int)top);
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
                        form_config_gen++; // stale memos refill
                }
                p += snprintf(p, end-p, "enable %d\nregion %g\nchance %g\n"
                        "steps %d\nrmin %g\nrmax %g\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        form_enable, form_region, form_chance,
                        form_steps, form_rmin, form_rmax);
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
                        world_seed = v;
                p += snprintf(p, end-p, "seed %d\n"
                        "(send 'regen' to rebuild the world with these)\n",
                        world_seed);
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
        else if (!strncmp(cmd, "dump", 4))
        {
                // raw world arrays to a file, for offline diffing
                char path[256] = "/tmp/blocko_dump.bin";
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
        else if (!strncmp(cmd, "regen", 5))
        {
                // invalidate all generation stamps: the whole ring regenerates
                // in place, nearest chunks first
                for (int i = 0; i < VAOD; i++) for (int j = 0; j < VAOW; j++)
                {
                        chunk_stamp[i][j].ax = INT_MIN;
                        chunk_stamp[i][j].az = INT_MIN;
                        chunk_estamp[i][j].ax = INT_MIN;
                        chunk_estamp[i][j].az = INT_MIN;
                }
                p += snprintf(p, end-p, "ok - world regenerating\n");
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
                // "mob <0|1>"    - toggle auto-spawning
                char arg[32] = "";
                sscanf(cmd + 3, "%31s", arg);
                if (!strcmp(arg, "spawn"))
                {
                        int pax = player[0].pos.x / BS;
                        int paz = player[0].pos.z / BS;
                        int ok = 0;
                        for (int r = 3; r < 20 && !ok; r++)
                                ok = mob_spawn(pax + r, paz) || mob_spawn(pax - r, paz)
                                   || mob_spawn(pax, paz + r) || mob_spawn(pax, paz - r);
                        p += snprintf(p, end-p, ok ? "spawned\n" : "no room to spawn\n");
                }
                else if (arg[0] == '0' || arg[0] == '1')
                {
                        mob_enable = arg[0] - '0';
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
                        p += snprintf(p, end-p, "  mob %d at %.1f %.1f %.1f hp %d\n", i,
                                mob[i].pos.x / BS - scootx, mob[i].pos.y / BS,
                                mob[i].pos.z / BS - scootz, mob[i].hp);
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
                int px = player[0].pos.x / BS;
                int pz = player[0].pos.z / BS;
                int xlo = ICLAMP(px - w_/2, 0, TILESW - w_);
                int zlo = ICLAMP(pz - d_/2, 0, TILESD - d_);
                int ylo = ICLAMP(TILESH/2 - h_/2, 0, TILESH - h_);

                unsigned long long freq = SDL_GetPerformanceFrequency();
                unsigned long long t0 = SDL_GetPerformanceCounter();
                for (int r = 0; r < reps; r++)
                        mesh_region(xlo, xlo + w_, ylo, ylo + h_, zlo, zlo + d_, FACE_ALL);
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
                        "fps [reset] | timings [reset] | pos | tp <ax> <az>\n"
                        "walk <frames> | fly <frames> <bl/s> | turn <deg>\n"
                        "dist <blocks> | debounce <frames>\n"
                        "find <tile> <ax0> <az0> <ax1> <az1>\n"
                        "noise [<knob> <val>] | form [near <r>|<knob> <val>]\n"
                        "caves [<0|1>] | trees [<0|1>] | seed [<n>] | sum | dump [<path>]\n"
                        "cull [<0|1>] | lock [<msg>|0] | regen | sun <pitch> | quit\n");
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

        // auto-walk countdown
        if (remote_walk_frames > 0 && --remote_walk_frames == 0)
        {
                player[0].goingf = 0;
                player[0].running = 0;
        }

        // noclip flight: constant speed along yaw at fixed altitude
        if (remote_fly_frames > 0)
        {
                remote_fly_frames--;
                player[0].pos.x += sinf(player[0].yaw) * remote_fly_speed * BS / 60.f;
                player[0].pos.z += cosf(player[0].yaw) * remote_fly_speed * BS / 60.f;
                player[0].pos.y = 4 * BS; // above any terrain
                player[0].grav = GRAV_ZERO;
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

// ---- in-game console: tilde to open, runs the same commands ----

int console_open;
char console_input[256];
size_t console_input_len;
char console_reply[8000];

void console_toggle()
{
        console_open = !console_open;
        if (console_open)
        {
                console_input_len = 0;
                console_input[0] = '\0';
                SDL_StartTextInput(vk.window);
                // drop any held movement so the player doesn't run off
                player[0].goingf = player[0].goingb = 0;
                player[0].goingl = player[0].goingr = 0;
                player[0].running = player[0].sneaking = 0;
        }
        else
                SDL_StopTextInput(vk.window);
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
                return 0;
        if (down) switch (event.key.key)
        {
                case SDLK_ESCAPE:
                        console_toggle();
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
                        {
                                remote_dispatch(console_input, console_reply, sizeof console_reply);
                                if (remote_want_quit)
                                        game_shutdown(0);
                                console_input_len = 0;
                                console_input[0] = '\0';
                        }
                        break;
        }
        return 1;
}

void console_text(const char *text)
{
        if (!console_open) return;
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
        if (!console_open) return;

        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1.f) scale = 1.f;
        int lh = FONT_CH_H * scale;
        int inputy = screenh - 2 * lh;

        char line[300];
        snprintf(line, sizeof line, "> %s_", console_input);
        font_begin(screenw, screenh);
        font_add_text(line, 20, inputy, 0);
        font_end(1.f, 1.f, .5f);

        if (!console_reply[0]) return;

        // reply sits above the input line, bottom-anchored, tail if it's long
        char *r = console_reply;
        int lines = 1;
        for (char *s = r; *s; s++)
                if (*s == '\n' && s[1]) lines++;
        while (lines > 32) { r = strchr(r, '\n') + 1; lines--; }
        font_begin(screenw, screenh);
        font_add_text(r, 20, inputy - (lines + 1) * lh, 0);
        font_end(1.f, 1.f, 1.f);
}

#endif // BLOCKO_REMOTE_C_INCLUDED
