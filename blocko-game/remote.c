#include "blocko.c"
#ifndef BLOCKO_REMOTE_C_INCLUDED
#define BLOCKO_REMOTE_C_INCLUDED

// remote.c - debug/automation socket
//
// A unix socket for reading timings and poking at the running game, so tools
// (or an AI agent) can benchmark and drive the game without the keyboard.
// One command per connection: connect, send a line, read reply, disconnected.
//
//   echo fps     | nc -U /tmp/blocko.sock
//   echo "tp 200 -1500" | nc -U /tmp/blocko.sock

#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

int remote_walk_frames; // hold forward key for this many frames
int remote_fly_frames;  // noclip along yaw for this many frames...
float remote_fly_speed; // ...at this many blocks/sec (deterministic traversal)

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

int floatcmp(const void *a, const void *b)
{
        float d = *(const float *)a - *(const float *)b;
        return d < 0 ? -1 : d > 0 ? 1 : 0;
}

void remote_reply(int fd, const char *cmd)
{
        char out[8000];
        char *p = out;
        char *end = out + sizeof out;

        if (!strncmp(cmd, "fps reset", 9))
        {
                frame_ring_len = 0;
                fps_reset_count = SDL_GetPerformanceCounter();
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
                                "avg_ms %.3f\np50_ms %.3f\np99_ms %.3f\nworst_ms %.3f\n",
                                n, elapsed, n / elapsed, sum / n,
                                sorted[n/2], sorted[n*99/100], sorted[n-1]);
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
        else if (!strncmp(cmd, "sun ", 4))
        {
                sun_pitch = atof(cmd + 4);
                p += snprintf(p, end-p, "ok\n");
        }
        else if (!strncmp(cmd, "quit", 4))
        {
                SDL_Event ev = { .type = SDL_EVENT_QUIT };
                SDL_PushEvent(&ev);
                p += snprintf(p, end-p, "ok\n");
        }
        else
        {
                p += snprintf(p, end-p, "commands: fps [reset] | timings [reset] | "
                        "pos | tp <ax> <az> | walk <frames> | turn <deg> | "
                        "dist <blocks> | sun <pitch> | quit\n");
        }

        write(fd, out, p - out);
}

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
                player[0].pos.y = 10 * BS; // above any terrain
                player[0].grav = GRAV_ZERO;
        }

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
}

#endif // BLOCKO_REMOTE_C_INCLUDED
