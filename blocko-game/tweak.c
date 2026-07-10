#include "blocko.c"
#ifndef BLOCKO_TWEAK_C_INCLUDED
#define BLOCKO_TWEAK_C_INCLUDED

// tweak.c - in-game terrain tweaker
//
// One big table of world-gen knobs (terrain_knobs.h + gen_knobs.h) drives
// everything here: the K-key panel, the `tweak` socket/console command, and
// reset-to-default tracking. Changing a knob invalidates the noise/terrain/
// formation memos and schedules a debounced regen_world(), so the world
// rebuilds around you a moment after you stop tapping - tweak, look, repeat.
//
// Knob values are all floats; TW_INT rows just round and step by whole
// numbers, TW_LOG rows step multiplicatively (for feature sizes).

struct tweak {
        const char *name;
        float *val;             // NULL marks a section header (name = title)
        float def, lo, hi;
        int flags;
        const char *desc;
};

static struct tweak tweaks[] = {
#define TWEAK_SECTION(title) { title, NULL, 0, 0, 0, 0, NULL },
#define TWEAK(n, d, lo, hi, fl, desc) { #n, &n, d, lo, hi, fl, desc },
#define TWEAK_VAR(n, d, lo, hi, fl, desc) { #n, &n, d, lo, hi, fl, desc },
#include "../common/tinyc.games/terrain_knobs.h"
#include "gen_knobs.h"
#undef TWEAK
#undef TWEAK_VAR
#undef TWEAK_SECTION
};
#define NR_TWEAKS ((int)(sizeof tweaks / sizeof *tweaks))

int tweak_open;
static int tweak_sel;
static int tweak_regen_at; // frame to fire the debounced regen (0 = none)

// a knob changed: stale memos refill, and the world regenerates half a
// second after the last change (so holding an arrow doesn't regen every frame)
static void tweak_touch()
{
        noise_config_gen++;   // taylor-noise feature memos
        terrain_config_gen++; // get_height/warp/ledge memos
        form_config_gen++;    // formations sit on the old surface
        tweak_regen_at = frame + 30;
}

// call once per frame from the main loop
void tweak_poll()
{
        if (tweak_regen_at && frame >= tweak_regen_at)
        {
                tweak_regen_at = 0;
                regen_world();
        }
}

static struct tweak *tweak_find(const char *name)
{
        for (int i = 0; i < NR_TWEAKS; i++)
                if (tweaks[i].val && !SDL_strcasecmp(tweaks[i].name, name))
                        return &tweaks[i];
        return NULL;
}

static void tweak_set(struct tweak *t, float v)
{
        v = CLAMP(v, t->lo, t->hi);
        if (t->flags & TW_INT) v = roundf(v);
        if (v == *t->val) return;
        *t->val = v;
        tweak_touch();
}

static void tweak_adjust(struct tweak *t, int dir, int coarse)
{
        float v;
        if (t->flags & TW_LOG)
        {
                float f = coarse ? 1.25f : 1.05f;
                v = dir > 0 ? *t->val * f : *t->val / f;
                if ((t->flags & TW_INT) && roundf(v) == roundf(*t->val))
                        v = *t->val + dir; // log step too small to move an int
        }
        else if (t->flags & TW_INT)
                v = *t->val + dir * (coarse ? 10 : 1);
        else
                v = *t->val + dir * (t->hi - t->lo) * (coarse ? .025f : .0025f);
        tweak_set(t, v);
}

static void tweak_move(int dir) // select the next knob, skipping headers
{
        int i = tweak_sel;
        do i = (i + dir + NR_TWEAKS) % NR_TWEAKS; while (!tweaks[i].val);
        tweak_sel = i;
}

static void tweak_jump(int dir) // hop to the first knob of the next section
{
        int i = tweak_sel;
        for (int n = 0; n < NR_TWEAKS; n++)
        {
                i = (i + dir + NR_TWEAKS) % NR_TWEAKS;
                if (!tweaks[i].val) break;
        }
        do i = (i + 1) % NR_TWEAKS; while (!tweaks[i].val);
        tweak_sel = i;
}

// key events come here after the console; returns 1 if consumed. arrows
// repeat (key_move's repeat gate comes after us), K toggles the panel
int tweak_key(int down)
{
        if (event.key.key == SDLK_K)
        {
                if (down && !event.key.repeat)
                {
                        tweak_open = !tweak_open;
                        if (!tweaks[tweak_sel].val) tweak_move(1);
                }
                return 1;
        }
        if (!tweak_open || !down)
                return 0;
        int coarse = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        switch (event.key.key)
        {
                case SDLK_UP:        tweak_move(-1); return 1;
                case SDLK_DOWN:      tweak_move( 1); return 1;
                case SDLK_LEFT:      tweak_adjust(&tweaks[tweak_sel], -1, coarse); return 1;
                case SDLK_RIGHT:     tweak_adjust(&tweaks[tweak_sel],  1, coarse); return 1;
                case SDLK_PAGEUP:    tweak_jump(-1); return 1;
                case SDLK_PAGEDOWN:  tweak_jump( 1); return 1;
                case SDLK_BACKSPACE: tweak_set(&tweaks[tweak_sel], tweaks[tweak_sel].def); return 1;
        }
        return 0; // everything else (WASD etc.) still drives the player
}

static void tweak_fmt(char *buf, size_t sz, struct tweak *t, float v)
{
        if (t->flags & TW_INT) snprintf(buf, sz, "%d", (int)v);
        else                   snprintf(buf, sz, "%.4g", v);
}

// the panel: a scrolling window of the table on the left, the selected
// knob's description and the key help pinned underneath
void tweak_draw()
{
        if (!tweak_open) return;

        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1.f) scale = 1.f;
        int lh = FONT_CH_H * scale;
        int x = 20;
        int y0 = screenh / 8;
        int rows = (screenh * 3 / 4) / lh - 5; // leave room for the footer
        if (rows < 5) rows = 5;

        // scroll the window so the selection stays comfortably inside
        static int top;
        if (tweak_sel < top + 2) top = MAX(tweak_sel - 2, 0);
        if (tweak_sel > top + rows - 3) top = MIN(tweak_sel - rows + 3, NR_TWEAKS - rows);
        if (top < 0) top = 0;

        char val[32], line[128];

        // three passes so each color is one font batch
        for (int pass = 0; pass < 3; pass++)
        {
                font_begin(screenw, screenh);
                for (int i = top; i < MIN(top + rows, NR_TWEAKS); i++)
                {
                        struct tweak *t = &tweaks[i];
                        int want = !t->val ? 0 : (i == tweak_sel ? 2 : 1);
                        if (want != pass) continue;
                        int y = y0 + (i - top) * lh;
                        if (!t->val)
                        {
                                snprintf(line, sizeof line, "--- %s ---", t->name);
                        }
                        else
                        {
                                tweak_fmt(val, sizeof val, t, *t->val);
                                snprintf(line, sizeof line, "%c%-20s %s",
                                        *t->val != t->def ? '*' : ' ', t->name, val);
                        }
                        font_add_text(line, x, y, 0);
                }
                if      (pass == 0) font_end(0.5f, 1.f, 1.f);   // section headers
                else if (pass == 1) font_end(1.f, 1.f, 1.f);    // knobs
                else                font_end(1.f, 1.f, 0.3f);   // selection
        }

        // footer: selected knob detail + key help
        struct tweak *t = &tweaks[tweak_sel];
        char lo[32], hi[32], def[32], footer[512];
        tweak_fmt(lo, sizeof lo, t, t->lo);
        tweak_fmt(hi, sizeof hi, t, t->hi);
        tweak_fmt(def, sizeof def, t, t->def);
        snprintf(footer, sizeof footer,
                "%s  [%s .. %s]  default %s\n%s\n%s\n"
                "Arrows adjust (Shift = coarse)  PgUp/Dn section  Bksp reset  K close",
                t->name, lo, hi, def, t->desc ? t->desc : "",
                tweak_regen_at ? "REGENERATING..." : "");
        font_begin(screenw, screenh);
        font_add_text(footer, x, y0 + (rows + 1) * lh, 0);
        font_end(1.f, 0.7f, 0.3f);
}

// the `tweak` socket/console command (see remote_dispatch):
//   tweak                  - list every knob (* = off-default)
//   tweak <name> [<value>] - show or set one knob (regen follows on its own)
//   tweak reset            - every knob back to its default
//   tweak dump             - off-default knobs as replayable `tweak` commands
void tweak_dispatch(const char *args, char *out, size_t outsz)
{
        char *p = out, *end = out + outsz;
        char name[64] = "";
        float v;
        int got = sscanf(args, "%63s %f", name, &v);
        char val[32], def[32];

        if (got <= 0)
        {
                for (int i = 0; i < NR_TWEAKS; i++)
                {
                        struct tweak *t = &tweaks[i];
                        if (end-p < 48) { p += snprintf(p, end-p, "truncated\n"); return; }
                        if (!t->val)
                                p += snprintf(p, end-p, "-- %s\n", t->name);
                        else
                        {
                                tweak_fmt(val, sizeof val, t, *t->val);
                                p += snprintf(p, end-p, "%c%s %s\n",
                                        *t->val != t->def ? '*' : ' ', t->name, val);
                        }
                }
        }
        else if (got == 1 && !strcmp(name, "reset"))
        {
                int n = 0;
                for (int i = 0; i < NR_TWEAKS; i++)
                        if (tweaks[i].val && *tweaks[i].val != tweaks[i].def)
                        {
                                *tweaks[i].val = tweaks[i].def;
                                n++;
                        }
                if (n) tweak_touch();
                p += snprintf(p, end-p, "reset %d knobs%s\n", n,
                        n ? " - world regenerating shortly" : "");
        }
        else if (!strcmp(name, "panel"))
        {
                // show/hide the on-screen panel remotely (same as the K key)
                tweak_open = got == 2 ? (v != 0) : !tweak_open;
                if (!tweaks[tweak_sel].val) tweak_move(1);
                p += snprintf(p, end-p, "panel %d\n", tweak_open);
        }
        else if (got == 1 && !strcmp(name, "dump"))
        {
                int n = 0;
                for (int i = 0; i < NR_TWEAKS; i++)
                {
                        struct tweak *t = &tweaks[i];
                        if (!t->val || *t->val == t->def) continue;
                        if (end-p < 48) { p += snprintf(p, end-p, "truncated\n"); return; }
                        tweak_fmt(val, sizeof val, t, *t->val);
                        p += snprintf(p, end-p, "tweak %s %s\n", t->name, val);
                        n++;
                }
                if (!n) p += snprintf(p, end-p, "everything is at its default\n");
        }
        else
        {
                struct tweak *t = tweak_find(name);
                if (!t)
                {
                        p += snprintf(p, end-p, "no knob named '%s' - "
                                "send 'tweak' for the list\n", name);
                        return;
                }
                if (got == 2)
                        tweak_set(t, v);
                tweak_fmt(val, sizeof val, t, *t->val);
                tweak_fmt(def, sizeof def, t, t->def);
                p += snprintf(p, end-p, "%s %s (default %s)%s\n",
                        t->name, val, def,
                        got == 2 ? " - world regenerating shortly" : "");
        }
}

#endif // BLOCKO_TWEAK_C_INCLUDED
