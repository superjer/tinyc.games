#include "blocko.c"
#ifndef BLOCKO_NET_C_INCLUDED
#define BLOCKO_NET_C_INCLUDED

// net.c - TCP multiplayer: a listen server and its clients
//
// One player hosts (--serve); others join (--connect <host>). The world never
// travels: a joining client gets the seed and the edit overlay (edit.c) in
// WELCOME, generates terrain locally, and replays the edits as chunks appear.
// After that the wire carries only events: block edits now, player/mob state
// in later phases. All coords on the wire are ABSOLUTE tile coords - window
// coords differ per instance (scootx/scootz).
//
// Messages are [type u8][len u16][payload], everything little-endian, packed
// by hand so struct padding never leaks into the protocol. Sockets are
// nonblocking and polled once per frame on the main thread (net_poll, next to
// remote_poll), with growable per-connection buffers absorbing partial reads
// and writes. Like the debug socket, unix-only for now.

int net_mode; // NET_OFF/NET_SERVER/NET_CLIENT (defs.c)

#ifdef _WIN32

void net_poll() {}
void net_send_edit(int x, int y, int z, int tile) {}
int net_serve(int port) { fprintf(stderr, "net: not on Windows yet\n"); return -1; }
int net_connect(const char *host, int port) { return net_serve(0); }
int net_describe(char *out, int outsz) { return snprintf(out, outsz, "net off\n"); }

#else // the rest of the file

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define NET_PROTO 1
#define NET_MAX_CLIENTS (NR_PLAYERS - 1)
#define NET_BUF_MAX (32 << 20) // a peer this far behind is gone: drop it

enum {
        MSG_HELLO = 1, // c->s: u32 protocol version
        MSG_WELCOME,   // s->c: u8 your player id, i32 seed, f32 sun_pitch, u32 nr edits
        MSG_EDIT,      // both: i32 ax, i32 ay, i32 az, u8 tile
};

struct conn {
        int fd;                  // -1 = free
        int player;              // player slot this connection drives
        int helloed;             // server: HELLO checked out, WELCOME sent
        unsigned char *in, *out; // partial-read and unsent-write buffers
        int in_len, in_cap, out_len, out_cap;
};

static struct conn conns[NET_MAX_CLIENTS] = {{.fd = -1}};
static int net_listen_fd = -1;
static struct conn server_conn = {.fd = -1};
static int net_welcomed; // client: WELCOME has arrived

// --- little-endian packing (avoids struct padding + host byte order) -------

static void put_u32(unsigned char *p, unsigned v)
{
        p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static unsigned get_u32(const unsigned char *p)
{
        return (unsigned)p[0] | (unsigned)p[1] << 8
             | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static void put_f32(unsigned char *p, float f)
{
        unsigned u;
        memcpy(&u, &f, 4);
        put_u32(p, u);
}

static float get_f32(const unsigned char *p)
{
        unsigned u = get_u32(p);
        float f;
        memcpy(&f, &u, 4);
        return f;
}

// --- connections ------------------------------------------------------------

static void conn_close(struct conn *c)
{
        if (c->fd < 0) return;
        close(c->fd);
        c->fd = -1;
        free(c->in);  c->in  = NULL; c->in_len  = c->in_cap  = 0;
        free(c->out); c->out = NULL; c->out_len = c->out_cap = 0;
        if (net_mode == NET_SERVER && c->helloed)
                fprintf(stderr, "net: player %d left\n", c->player);
        c->helloed = 0;
}

static void buf_put(unsigned char **buf, int *len, int *cap, const void *data, int n)
{
        if (*len + n > *cap)
        {
                int ncap = *cap ? *cap : 4096;
                while (*len + n > ncap) ncap *= 2;
                *buf = realloc(*buf, ncap);
                *cap = ncap;
        }
        memcpy(*buf + *len, data, n);
        *len += n;
}

static void conn_send(struct conn *c, int type, const void *payload, int len)
{
        if (c->fd < 0) return;
        if (c->out_len + 3 + len > NET_BUF_MAX) { conn_close(c); return; }
        unsigned char hdr[3] = { type, len & 255, len >> 8 };
        buf_put(&c->out, &c->out_len, &c->out_cap, hdr, 3);
        buf_put(&c->out, &c->out_len, &c->out_cap, payload, len);
}

static void conn_flush(struct conn *c)
{
        int off = 0;
        while (c->fd >= 0 && off < c->out_len)
        {
                int n = send(c->fd, c->out + off, c->out_len - off, MSG_NOSIGNAL);
                if (n > 0) { off += n; continue; }
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (n < 0 && errno == EINTR) continue;
                conn_close(c);
                return;
        }
        if (off && c->fd >= 0)
        {
                memmove(c->out, c->out + off, c->out_len - off);
                c->out_len -= off;
        }
}

// --- message handling -------------------------------------------------------

static void server_welcome(struct conn *c)
{
        unsigned char m[13];
        m[0] = c->player;
        put_u32(m + 1, world_seed);
        put_f32(m + 5, sun_pitch);
        put_u32(m + 9, edit_len);
        conn_send(c, MSG_WELCOME, m, sizeof m);

        // stream the whole edit overlay; the client records the entries and
        // replays them as its chunks generate, same as a local regen
        int it = 0, x, y, z, tile;
        unsigned char e[13];
        while (edit_next(&it, &x, &y, &z, &tile))
        {
                put_u32(e, x); put_u32(e + 4, y); put_u32(e + 8, z);
                e[12] = tile;
                conn_send(c, MSG_EDIT, e, sizeof e);
        }
        fprintf(stderr, "net: player %d joined (%d edits sent)\n", c->player, edit_len);
}

static void client_welcome(const unsigned char *p)
{
        int old = my_player;
        my_player = p[0];
        if (my_player >= NR_PLAYERS) { conn_close(&server_conn); return; }
        if (my_player != old)
                player[my_player] = player[old]; // carry the local body to its new slot
        world_seed = get_u32(p + 1);
        sun_pitch = get_f32(p + 5);

        // the server's edit overlay replaces anything local, and the world
        // regenerates from the server's seed as if by the regen command
        edit_clear();
        regen_world();

        // mobs aren't synced yet (phase 4): keep the client's world quiet
        // rather than showing slimes the server can't see
        mob_enable = 0;
        for (int i = 0; i < NR_MOBS; i++)
                mob[i].alive = 0;

        net_welcomed = 1;
        fprintf(stderr, "net: joined as player %d, seed %d, %u edits incoming\n",
                my_player, world_seed, get_u32(p + 9));
}

static void net_handle(struct conn *c, int type, const unsigned char *p, int len)
{
        if (net_mode == NET_SERVER) switch (type)
        {
        case MSG_HELLO:
                if (len < 4 || get_u32(p) != NET_PROTO) { conn_close(c); return; }
                if (c->helloed) return;
                c->helloed = 1;
                server_welcome(c);
                break;
        case MSG_EDIT:
                if (len < 13 || !c->helloed) return;
                int y = get_u32(p + 4);
                if (y < 0 || y >= TILESH) return;
                edit_apply_remote(get_u32(p), y, get_u32(p + 8), p[12]);
                for (int i = 0; i < NET_MAX_CLIENTS; i++) // relay to the others
                        if (conns[i].helloed && &conns[i] != c)
                                conn_send(&conns[i], MSG_EDIT, p, 13);
                break;
        }
        else switch (type)
        {
        case MSG_WELCOME:
                if (len < 13) { conn_close(c); return; }
                client_welcome(p);
                break;
        case MSG_EDIT:
                if (len < 13) return;
                int y = get_u32(p + 4);
                if (y < 0 || y >= TILESH) return;
                edit_apply_remote(get_u32(p), y, get_u32(p + 8), p[12]);
                break;
        }
}

static void conn_read(struct conn *c)
{
        unsigned char tmp[65536];
        while (c->fd >= 0)
        {
                int n = recv(c->fd, tmp, sizeof tmp, 0);
                if (n > 0)
                {
                        if (c->in_len + n > NET_BUF_MAX) { conn_close(c); return; }
                        buf_put(&c->in, &c->in_len, &c->in_cap, tmp, n);
                        if (n < (int)sizeof tmp) break;
                }
                else if (n == 0) { conn_close(c); return; }
                else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else if (errno == EINTR) continue;
                else { conn_close(c); return; }
        }

        int off = 0;
        while (c->fd >= 0 && c->in_len - off >= 3)
        {
                int type = c->in[off];
                int len = c->in[off + 1] | (c->in[off + 2] << 8);
                if (c->in_len - off - 3 < len) break;
                net_handle(c, type, c->in + off + 3, len);
                off += 3 + len;
        }
        if (off && c->fd >= 0)
        {
                memmove(c->in, c->in + off, c->in_len - off);
                c->in_len -= off;
        }
}

static void net_nonblock(int fd)
{
        fcntl(fd, F_SETFL, O_NONBLOCK);
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
}

// --- public entry points ----------------------------------------------------

// called from set_tile with the edit in ABSOLUTE coords: clients tell the
// server, the server tells everyone (the server relays client edits itself)
void net_send_edit(int x, int y, int z, int tile)
{
        if (net_mode == NET_OFF) return;
        unsigned char m[13];
        put_u32(m, x); put_u32(m + 4, y); put_u32(m + 8, z);
        m[12] = tile;
        if (net_mode == NET_CLIENT)
                conn_send(&server_conn, MSG_EDIT, m, sizeof m);
        else for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed)
                        conn_send(&conns[i], MSG_EDIT, m, sizeof m);
}

// once per rendered frame from the main loop, like remote_poll
void net_poll()
{
        if (net_mode == NET_SERVER)
        {
                for (;;)
                {
                        int fd = accept(net_listen_fd, NULL, NULL);
                        if (fd < 0) break;
                        int i;
                        for (i = 0; i < NET_MAX_CLIENTS; i++)
                                if (conns[i].fd < 0) break;
                        if (i == NET_MAX_CLIENTS) { close(fd); continue; } // full
                        net_nonblock(fd);
                        conns[i] = (struct conn){ .fd = fd, .player = i + 1 };
                }
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].fd >= 0)
                        {
                                conn_read(&conns[i]);
                                conn_flush(&conns[i]);
                        }
        }
        else if (net_mode == NET_CLIENT)
        {
                conn_read(&server_conn);
                conn_flush(&server_conn);
                if (server_conn.fd < 0)
                {
                        net_mode = NET_OFF;
                        fprintf(stderr, "net: disconnected\n");
                }
        }
}

int net_serve(int port)
{
        if (net_mode != NET_OFF) return -1;

        net_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (net_listen_fd < 0) return -1;
        int one = 1;
        setsockopt(net_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(net_listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
            listen(net_listen_fd, NET_MAX_CLIENTS) < 0)
        {
                fprintf(stderr, "net: can't listen on port %d\n", port);
                close(net_listen_fd);
                net_listen_fd = -1;
                return -1;
        }
        fcntl(net_listen_fd, F_SETFL, O_NONBLOCK);

        for (int i = 0; i < NET_MAX_CLIENTS; i++)
                conns[i] = (struct conn){ .fd = -1 };
        net_mode = NET_SERVER;
        my_player = 0;
        fprintf(stderr, "net: serving on port %d\n", port);
        return 0;
}

int net_connect(const char *host, int port)
{
        if (net_mode != NET_OFF) return -1;

        char portstr[16];
        snprintf(portstr, sizeof portstr, "%d", port);
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
        struct addrinfo *res;
        if (getaddrinfo(host, portstr, &hints, &res) != 0)
        {
                fprintf(stderr, "net: can't resolve %s\n", host);
                return -1;
        }
        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) < 0)
        {
                fprintf(stderr, "net: can't connect to %s:%d\n", host, port);
                if (fd >= 0) close(fd);
                freeaddrinfo(res);
                return -1;
        }
        freeaddrinfo(res);
        net_nonblock(fd);

        server_conn = (struct conn){ .fd = fd };
        net_mode = NET_CLIENT;

        unsigned char m[4];
        put_u32(m, NET_PROTO);
        conn_send(&server_conn, MSG_HELLO, m, sizeof m);

        // wait for WELCOME (a beat of server time plus the edit log), so the
        // seed is right before any terrain generates. Bounded: a server that
        // never answers gets dropped and the game stays single-player.
        net_welcomed = 0;
        for (int waited = 0; !net_welcomed && waited < 5000; waited += 10)
        {
                conn_flush(&server_conn);
                conn_read(&server_conn);
                if (server_conn.fd < 0) break;
                SDL_Delay(10);
        }
        if (!net_welcomed)
        {
                fprintf(stderr, "net: no WELCOME from %s:%d\n", host, port);
                conn_close(&server_conn);
                net_mode = NET_OFF;
                return -1;
        }
        return 0;
}

// one-line status for the net command / HUD
int net_describe(char *out, int outsz)
{
        char *p = out, *end = out + outsz;
        if (net_mode == NET_OFF)
                p += snprintf(p, end-p, "net off\n");
        else if (net_mode == NET_CLIENT)
                p += snprintf(p, end-p, "client, player %d, %s\n", my_player,
                        server_conn.fd >= 0 ? "connected" : "disconnected");
        else
        {
                int n = 0;
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed) n++;
                p += snprintf(p, end-p, "server, %d client%s connected\n",
                        n, n == 1 ? "" : "s");
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed)
                                p += snprintf(p, end-p, "  player %d\n", conns[i].player);
        }
        return p - out;
}

#endif // _WIN32
#endif // BLOCKO_NET_C_INCLUDED
