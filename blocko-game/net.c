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
// and writes.

int net_mode; // NET_OFF/NET_SERVER/NET_CLIENT (defs.c)

// the same BSD socket calls run everywhere; Windows just spells a few things
// differently (winsock wants waking up, sockets close with closesocket, and
// errors come from WSAGetLastError instead of errno)
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define MSG_NOSIGNAL 0 // no SIGPIPE on Windows
#define net_close closesocket
static int net_again() { return WSAGetLastError() == WSAEWOULDBLOCK; }
static int net_intr() { return WSAGetLastError() == WSAEINTR; }
static void net_startup()
{
        static int done;
        WSADATA wsa;
        if (!done) done = !WSAStartup(MAKEWORD(2, 2), &wsa);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0 // macOS has no MSG_NOSIGNAL
#endif
#define net_close close
static int net_again() { return errno == EAGAIN || errno == EWOULDBLOCK; }
static int net_intr() { return errno == EINTR; }
static void net_startup() {}
#endif

#define NET_PROTO 3
#define NET_MAX_CLIENTS (NR_PLAYERS - 1)
#define NET_BUF_MAX (32 << 20) // a peer this far behind is gone: drop it

enum {
        MSG_HELLO = 1, // c->s: u32 protocol version
        MSG_WELCOME,   // s->c: u8 your player id, i32 seed, f32 sun_pitch, u32 nr edits
        MSG_EDIT,      // both: i32 ax, i32 ay, i32 az, u8 tile
        MSG_PLAYER,    // both: u8 id, f32 abs pos xyz (units), f32 vel xyz, f32 yaw,
                       //       pitch, u8 flags (NET_PF_*), u8 held tile
        MSG_MOB,       // s->c: u32 total kills, then per mob u8 slot, size, hurt, dying, f32 abs xyz, yaw
        MSG_PUNCH,     // c->s: u8 mob slot, f32 aim x, f32 aim z
        MSG_BONK,      // s->c: f32 knock x, f32 knock z - a slime hit YOU
        MSG_TIME,      // s->c: f32 sun_pitch, so sunsets stay shared
        MSG_CHAT,      // both: u8 sender id, then the text (not NUL-terminated)
        MSG_PMODEL,    // both: u8 owner id, then a raw struct pmodel (~15.8KB)
};

#define MOB_ENTRY 20 // bytes per mob in a MSG_MOB snapshot

// last state received for each remote player, in ABSOLUTE units. seen is the
// pframe it arrived; a player quiet for 2 seconds fades out of the world
static struct net_state { float x, y, z, vx, vy, vz, yaw, pitch; int seen; }
        net_state[NR_PLAYERS];

// client: where the server last put each mob (ABSOLUTE units); positions ease
// toward these each frame so 15Hz snapshots read as continuous motion
static struct { float x, y, z, yaw; } mob_target[NR_MOBS];

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
        net_close(c->fd);
        c->fd = -1;
        free(c->in);  c->in  = NULL; c->in_len  = c->in_cap  = 0;
        free(c->out); c->out = NULL; c->out_len = c->out_cap = 0;
        if (net_mode == NET_SERVER && c->helloed)
        {
                fprintf(stderr, "net: player %d left\n", c->player);
                net_state[c->player].seen = 0; // their ghost leaves at once here;
                                               // other clients age it out in 2s
        }
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
                int n = send(c->fd, (char *)c->out + off, c->out_len - off, MSG_NOSIGNAL);
                if (n > 0) { off += n; continue; }
                if (n < 0 && net_again()) break;
                if (n < 0 && net_intr()) continue;
                conn_close(c);
                return;
        }
        if (off && c->fd >= 0)
        {
                memmove(c->out, c->out + off, c->out_len - off);
                c->out_len -= off;
        }
}

// announce my model: a client sends it to the server (which stores + relays),
// the server broadcasts it to every client directly. Used at WELCOME and
// whenever the model editor closes with changes.
void pmodel_send_mine()
{
        static unsigned char pm[1 + sizeof(struct pmodel)];
        pm[0] = my_player;
        memcpy(pm + 1, &pm_models[my_player], sizeof(struct pmodel));
        if (net_mode == NET_CLIENT)
                conn_send(&server_conn, MSG_PMODEL, pm, sizeof pm);
        else if (net_mode == NET_SERVER)
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed)
                                conn_send(&conns[i], MSG_PMODEL, pm, sizeof pm);
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
        // every model this server knows (its own included), so the joiner
        // sees everyone dressed right away; theirs arrives from them shortly
        static unsigned char pm[1 + sizeof(struct pmodel)];
        for (int i = 0; i < NR_PLAYERS; i++)
        {
                if (!pmodel_have[i] || i == c->player) continue;
                pm[0] = i;
                memcpy(pm + 1, &pm_models[i], sizeof(struct pmodel));
                conn_send(c, MSG_PMODEL, pm, sizeof pm);
        }

        fprintf(stderr, "net: player %d joined (%d edits sent)\n", c->player, edit_len);
}

static void client_welcome(const unsigned char *p)
{
        int old = my_player;
        my_player = p[0];
        if (my_player >= NR_PLAYERS) { conn_close(&server_conn); return; }
        if (my_player != old)
        {
                player[my_player] = player[old]; // carry the local body to its new slot
                pmodel_local_moved(old);         // and its model + face tiles
        }

        // introduce my model; the server stores it and relays it to everyone
        pmodel_send_mine();

        world_seed = get_u32(p + 1);
        sun_pitch = get_f32(p + 5);

        // the server's edit overlay replaces anything local, and the world
        // regenerates from the server's seed as if by the regen command
        edit_clear();
        regen_world();

        // drop any local slimes: the server owns the mobs now, and its
        // snapshots (MSG_MOB) will repopulate the array
        for (int i = 0; i < NR_MOBS; i++)
                mob[i].alive = 0;

        net_welcomed = 1;
        fprintf(stderr, "net: joined as player %d, seed %d, %u edits incoming\n",
                my_player, world_seed, get_u32(p + 9));
}

// a remote player's state landed: store the target, and wake the slot up if
// this is its first sighting (snap into place, give it the player box size)
static void net_store_player(int id, const unsigned char *p)
{
        if (id < 0 || id >= NR_PLAYERS || id == my_player) return; // the server itself is id 0
        struct net_state *st = &net_state[id];
        int fresh = !st->seen;
        st->x = get_f32(p + 1);  st->y  = get_f32(p + 5);  st->z  = get_f32(p + 9);
        st->vx = get_f32(p + 13); st->vy = get_f32(p + 17); st->vz = get_f32(p + 21);
        st->yaw = get_f32(p + 25);
        st->pitch = get_f32(p + 29);
        // the animation flags apply straight to the player slot: remote
        // players don't run physics, so nothing here fights the sim
        struct player *pl = &player[id];
        pl->ground   = !!(p[33] & NET_PF_GROUND);
        pl->sneaking = !!(p[33] & NET_PF_SNEAK);
        pl->breaking = !!(p[33] & NET_PF_BREAK);
        pl->building = !!(p[33] & NET_PF_BUILD);
        pl->wet      = !!(p[33] & NET_PF_WET);
        pl->noclip   = !!(p[33] & NET_PF_NOCLIP);
        pm_held[id] = p[34];
        st->seen = pframe ? pframe : 1;
        if (fresh)
        {
                player[id].pos = (struct box){ st->x + scootx * (float)BS, st->y,
                        st->z + scootz * (float)BS, PLYR_W, PLYR_H, PLYR_W };
                player[id].yaw = st->yaw;
                player[id].pitch = st->pitch;
        }
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
        case MSG_PLAYER:
                if (len < 35 || !c->helloed) return;
                net_store_player(c->player, p); // the slot is the identity, not the byte
                unsigned char relay[35];
                memcpy(relay, p, 35);
                relay[0] = c->player;
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed && &conns[i] != c)
                                conn_send(&conns[i], MSG_PLAYER, relay, 35);
                break;
        case MSG_PUNCH:
                if (len < 9 || !c->helloed) return;
                if (p[0] < NR_MOBS)
                        mob_shatter(p[0], get_f32(p + 1), get_f32(p + 5));
                break;
        case MSG_PMODEL:
        {
                if (len < 1 + (int)sizeof(struct pmodel) || !c->helloed) return;
                pmodel_net_recv(c->player, p + 1, len - 1); // the slot is the identity
                static unsigned char relay[1 + sizeof(struct pmodel)];
                relay[0] = c->player;
                memcpy(relay + 1, p + 1, sizeof(struct pmodel));
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed && &conns[i] != c)
                                conn_send(&conns[i], MSG_PMODEL, relay, sizeof relay);
                break;
        }
        case MSG_CHAT:
        {
                if (len < 2 || len > 256 || !c->helloed) return;
                unsigned char relay[256];
                memcpy(relay, p, len);
                relay[0] = c->player; // the slot is the identity, not the byte
                char line[300];
                snprintf(line, sizeof line, "<player %d> %.*s",
                        c->player, len - 1, (const char *)p + 1);
                chat_add(line);
                for (int i = 0; i < NET_MAX_CLIENTS; i++)
                        if (conns[i].helloed && &conns[i] != c)
                                conn_send(&conns[i], MSG_CHAT, relay, len);
                break;
        }
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
        case MSG_PLAYER:
                if (len < 35) return;
                net_store_player(p[0], p);
                break;
        case MSG_MOB:
        {
                // full snapshot of every living mob: apply each entry, then
                // kill any local mob the server no longer knows
                if (len < 4) return;
                mob_kills = get_u32(p); // the shared kill tally
                p += 4;
                len -= 4;
                char in_snap[NR_MOBS] = {0};
                for (; len >= MOB_ENTRY; p += MOB_ENTRY, len -= MOB_ENTRY)
                {
                        int slot = p[0];
                        if (slot >= NR_MOBS) continue;
                        in_snap[slot] = 1;
                        struct mob *m = &mob[slot];
                        mob_target[slot].x = get_f32(p + 4);
                        mob_target[slot].y = get_f32(p + 8);
                        mob_target[slot].z = get_f32(p + 12);
                        mob_target[slot].yaw = get_f32(p + 16);
                        if (!m->alive) // fresh: land it in place, no glide-in
                        {
                                m->pos.x = m->prev.x = mob_target[slot].x + scootx * (float)BS;
                                m->pos.y = m->prev.y = mob_target[slot].y;
                                m->pos.z = m->prev.z = mob_target[slot].z + scootz * (float)BS;
                                m->yaw = m->prev_yaw = mob_target[slot].yaw;
                                m->pos.w = m->pos.d = m->pos.h = 1; // sized just below
                                m->size = 0;
                                m->alive = 1;
                        }
                        if (m->size != p[1])
                                mob_set_size(m, p[1]);
                        m->hurt = p[2];
                        m->dying = p[3];
                }
                for (int i = 0; i < NR_MOBS; i++)
                        if (mob[i].alive && !in_snap[i])
                                mob[i].alive = 0;
                break;
        }
        case MSG_TIME:
                if (len < 4) return;
                sun_pitch = get_f32(p); // the sun drifts apart slowly; snap to match
                break;
        case MSG_PMODEL:
                if (len < 1 + (int)sizeof(struct pmodel)) return;
                pmodel_net_recv(p[0], p + 1, len - 1);
                break;
        case MSG_CHAT:
        {
                if (len < 2 || len > 256) return;
                char line[300];
                snprintf(line, sizeof line, "<player %d> %.*s",
                        p[0], len - 1, (const char *)p + 1);
                chat_add(line);
                break;
        }
        case MSG_BONK:
        {
                // a slime on the server bonked ME: same knockback the host
                // player gets in update_mobs, in forward/right velocity terms
                if (len < 8) return;
                struct player *pl = &player[my_player];
                float nx = get_f32(p), nz = get_f32(p + 4);
                float fwdx = sinf(pl->yaw), fwdz = cosf(pl->yaw);
                pl->fvel = (nx * fwdx + nz * fwdz) * PLYR_SPD_R * 4;
                pl->rvel = (nx * fwdz - nz * fwdx) * PLYR_SPD_R * 4;
                pl->grav = GRAV_JUMP + 5;
                break;
        }
        }
}

static void conn_read(struct conn *c)
{
        unsigned char tmp[65536];
        while (c->fd >= 0)
        {
                int n = recv(c->fd, (char *)tmp, sizeof tmp, 0);
                if (n > 0)
                {
                        if (c->in_len + n > NET_BUF_MAX) { conn_close(c); return; }
                        buf_put(&c->in, &c->in_len, &c->in_cap, tmp, n);
                        if (n < (int)sizeof tmp) break;
                }
                else if (n == 0) { conn_close(c); return; }
                else if (net_again()) break;
                else if (net_intr()) continue;
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
#ifdef _WIN32
        u_long nb = 1;
        ioctlsocket(fd, FIONBIO, &nb);
#else
        fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
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

// is player slot i a live remote player worth drawing/tracking?
int net_player_active(int i)
{
        return net_mode != NET_OFF && i != my_player
                && net_state[i].seen && pframe - net_state[i].seen < 120;
}

// remote player i's last reported position in ABSOLUTE block coords -
// the anchor their sim area follows
void net_player_anchor(int i, int *abx, int *abz)
{
        *abx = (int)(net_state[i].x / BS);
        *abz = (int)(net_state[i].z / BS);
}

// send my own player state at ~20Hz (every 3rd physics tick)
static void net_send_my_state()
{
        // a dedicated server has no body to show; a headless CLIENT is a
        // real player (bk-driven) and must report, or the server won't
        // follow it with a sim area
        if (headless && net_mode == NET_SERVER) return;
        static int last_sent = -3;
        if (pframe - last_sent < 3) return;
        last_sent = pframe;

        struct player *p = &player[my_player];
        unsigned char m[35];
        m[0] = my_player;
        put_f32(m + 1, p->pos.x - scootx * (float)BS); // window -> absolute units
        put_f32(m + 5, p->pos.y);
        put_f32(m + 9, p->pos.z - scootz * (float)BS);
        put_f32(m + 13, p->vel.x);
        put_f32(m + 17, p->vel.y);
        put_f32(m + 21, p->vel.z);
        put_f32(m + 25, p->yaw);
        put_f32(m + 29, p->pitch);
        m[33] = (p->ground   ? NET_PF_GROUND : 0)
              | (p->sneaking ? NET_PF_SNEAK  : 0)
              | (p->breaking ? NET_PF_BREAK  : 0)
              | (p->building ? NET_PF_BUILD  : 0)
              | (p->wet      ? NET_PF_WET    : 0)
              | (p->noclip   ? NET_PF_NOCLIP : 0);
        m[34] = held_tile;
        if (net_mode == NET_CLIENT)
                conn_send(&server_conn, MSG_PLAYER, m, sizeof m);
        else for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed)
                        conn_send(&conns[i], MSG_PLAYER, m, sizeof m);
}

// server: keep everyone's sun where mine is, every ~5 seconds
static void net_send_time()
{
        static int last_sent = -300;
        if (pframe - last_sent < 300) return;
        last_sent = pframe;
        unsigned char m[4];
        put_f32(m, sun_pitch);
        for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed)
                        conn_send(&conns[i], MSG_TIME, m, sizeof m);
}

// server: snapshot every living mob to every client at ~15Hz (every 4th tick)
static void net_send_mobs()
{
        static int last_sent = -4;
        if (pframe - last_sent < 4) return;
        last_sent = pframe;

        unsigned char snap[4 + NR_MOBS * MOB_ENTRY];
        put_u32(snap, mob_kills); // the shared kill tally rides along
        int n = 0;
        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;
                unsigned char *e = snap + 4 + n * MOB_ENTRY;
                e[0] = i;
                e[1] = m->size;
                e[2] = m->hurt;
                e[3] = m->dying;
                put_f32(e + 4, m->pos.x - scootx * (float)BS); // window -> absolute
                put_f32(e + 8, m->pos.y);
                put_f32(e + 12, m->pos.z - scootz * (float)BS);
                put_f32(e + 16, m->yaw);
                n++;
        }
        for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed)
                        conn_send(&conns[i], MSG_MOB, snap, 4 + n * MOB_ENTRY);
}

// client: ease each mob toward its snapshot target; prev tracks the old spot
// so mob_build's prev->pos sub-tick lerp keeps working
static void net_smooth_mobs()
{
        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;
                m->prev = m->pos;
                m->prev_yaw = m->yaw;
                m->pos.x += (mob_target[i].x + scootx * (float)BS - m->pos.x) * 0.35f;
                m->pos.y += (mob_target[i].y - m->pos.y) * 0.35f;
                m->pos.z += (mob_target[i].z + scootz * (float)BS - m->pos.z) * 0.35f;
                float dyaw = mob_target[i].yaw - m->yaw;
                while (dyaw >  PI) dyaw -= TAU;
                while (dyaw < -PI) dyaw += TAU;
                m->yaw += dyaw * 0.35f;
        }
}

// client: my punch flies to the server, which owns the mobs
void net_send_punch(int slot, float aimx, float aimz)
{
        if (net_mode != NET_CLIENT) return;
        unsigned char m[9];
        m[0] = slot;
        put_f32(m + 1, aimx);
        put_f32(m + 5, aimz);
        conn_send(&server_conn, MSG_PUNCH, m, sizeof m);
}

// my chat line goes to the server, which shows it and passes it around; the
// sender has already put it in their own log
void net_send_chat(const char *text)
{
        if (net_mode == NET_OFF) return;
        unsigned char m[256];
        int len = strlen(text);
        if (len > (int)sizeof m - 1) len = sizeof m - 1;
        if (!len) return;
        m[0] = my_player;
        memcpy(m + 1, text, len);
        if (net_mode == NET_CLIENT)
                conn_send(&server_conn, MSG_CHAT, m, 1 + len);
        else for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed)
                        conn_send(&conns[i], MSG_CHAT, m, 1 + len);
}

// server: tell a client one of my slimes bonked them
void net_send_bonk(int pi, float nx, float nz)
{
        if (net_mode != NET_SERVER) return;
        unsigned char m[8];
        put_f32(m, nx);
        put_f32(m + 4, nz);
        for (int i = 0; i < NET_MAX_CLIENTS; i++)
                if (conns[i].helloed && conns[i].player == pi)
                        conn_send(&conns[i], MSG_BONK, m, sizeof m);
}

// ease each remote player toward its last received state - 20Hz updates drawn
// at 60+fps would stutter if we snapped. A jump too big to be movement (a tp)
// snaps instead of gliding across the world.
static void net_smooth_players()
{
        for (int i = 0; i < NR_PLAYERS; i++)
        {
                if (!net_player_active(i)) continue;
                struct player *pl = &player[i];
                struct net_state *st = &net_state[i];
                float wx = st->x + scootx * (float)BS; // absolute -> window units
                float wz = st->z + scootz * (float)BS;
                float dx = wx - pl->pos.x, dy = st->y - pl->pos.y, dz = wz - pl->pos.z;
                if (dx*dx + dy*dy + dz*dz > (10.f*BS) * (10.f*BS))
                {
                        pl->pos.x = wx;
                        pl->pos.y = st->y;
                        pl->pos.z = wz;
                }
                else
                {
                        pl->pos.x += dx * 0.35f;
                        pl->pos.y += dy * 0.35f;
                        pl->pos.z += dz * 0.35f;
                }
                float dyaw = st->yaw - pl->yaw;
                while (dyaw >  PI) dyaw -= TAU;
                while (dyaw < -PI) dyaw += TAU;
                pl->yaw += dyaw * 0.35f;
                pl->pitch += (st->pitch - pl->pitch) * 0.35f;
        }
}

// once per rendered frame from the main loop, like remote_poll
void net_poll()
{
        if (net_mode == NET_SERVER)
        {
                for (;;)
                {
                        int fd = (int)accept(net_listen_fd, NULL, NULL);
                        if (fd < 0) break;
                        int i;
                        for (i = 0; i < NET_MAX_CLIENTS; i++)
                                if (conns[i].fd < 0) break;
                        if (i == NET_MAX_CLIENTS) { net_close(fd); continue; } // full
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

        if (net_mode != NET_OFF)
        {
                net_send_my_state();
                net_smooth_players();
        }
        if (net_mode == NET_SERVER)
        {
                net_send_mobs();
                net_send_time();
        }
        else if (net_mode == NET_CLIENT)
                net_smooth_mobs();
}

int net_serve(int port)
{
        if (net_mode != NET_OFF) return -1;
        net_startup();

        net_listen_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
        if (net_listen_fd < 0) return -1;
        int one = 1;
        setsockopt(net_listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(net_listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
            listen(net_listen_fd, NET_MAX_CLIENTS) < 0)
        {
                fprintf(stderr, "net: can't listen on port %d\n", port);
                net_close(net_listen_fd);
                net_listen_fd = -1;
                return -1;
        }
        net_nonblock(net_listen_fd);

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
        net_startup();

        char portstr[16];
        snprintf(portstr, sizeof portstr, "%d", port);
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
        struct addrinfo *res;
        if (getaddrinfo(host, portstr, &hints, &res) != 0)
        {
                fprintf(stderr, "net: can't resolve %s\n", host);
                return -1;
        }
        int fd = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0 || connect(fd, res->ai_addr, (int)res->ai_addrlen) < 0)
        {
                fprintf(stderr, "net: can't connect to %s:%d\n", host, port);
                if (fd >= 0) net_close(fd);
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

#endif // BLOCKO_NET_C_INCLUDED
