#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_PICKER_C_INCLUDED
#define BLOCKO_PMEDIT_PICKER_C_INCLUDED

// pmedit/picker.c - the LOAD model picker: catalog, thumbnails, paging, choosing

// ---- model picker ---------------------------------------------------------
// A full-screen grid of live thumbnails, opened by the LOAD button in
// the whole-model view. Sources, in order: the named asset models (Bloc Croc,
// Buster Bo, the default), then this install's saved snapshots newest-first.
// Every source is read-only here; choosing one loads it live AND writes a
// fresh newest-numbered snapshot (so the pick loads next launch and later
// edits ride that new file). PM_NR_PREVIEW cells to a page; extras paginate.

struct pm_pick_ent { char label[24]; char path[256]; };
#define PM_PICK_MAX 256
#define PM_PICK_COLS 3
#define PM_PICK_ROWS 2 // PM_PICK_COLS * PM_PICK_ROWS == PM_NR_PREVIEW
static struct pm_pick_ent pmedit_pick_ent[PM_PICK_MAX];
static int pmedit_pick_nr;                              // selectable models found
static int pmedit_pick_page;                           // page of PM_NR_PREVIEW up
static struct pmodel pmedit_pick_model[PM_NR_PREVIEW];  // the page's loaded models
static int pmedit_pick_loaded[PM_NR_PREVIEW];           // cell holds a valid model
static float pmedit_pick_spin;                          // shared slow turntable

static int pmedit_pick_pages(void)
{
        int p = (pmedit_pick_nr + PM_NR_PREVIEW - 1) / PM_NR_PREVIEW;
        return p < 1 ? 1 : p;
}

// catalog the selectable models: named assets first, then save-data snapshots
// newest number first. Only files that actually load make the list.
static void pmedit_pick_scan(void)
{
        pmedit_pick_nr = 0;
        static const struct { const char *label, *path; } named[] = {
                { "BLOC CROC", TINYC_DIR "/blocko-game/assets/models/player-bloc-croc.model" },
                { "BUSTER BO", TINYC_DIR "/blocko-game/assets/models/player-buster-bo.model" },
                { "DEFAULT",   PM_DEFAULT_FILE },
        };
        struct pmodel probe;
        for (int i = 0; i < 3; i++)
        {
                if (!pmodel_load(&probe, named[i].path)) continue;
                struct pm_pick_ent *e = &pmedit_pick_ent[pmedit_pick_nr++];
                snprintf(e->label, sizeof e->label, "%s", named[i].label);
                snprintf(e->path, sizeof e->path, "%s", named[i].path);
        }
        // snapshots: glob the numbers, sort descending, list newest first
        int nums[PM_PICK_MAX], nn = 0, count = 0;
        char **names = SDL_GlobDirectory(pm_hist_dir, "*.model", 0, &count);
        for (int i = 0; names && i < count && nn < PM_PICK_MAX; i++)
        {
                char *end;
                long n = strtol(names[i], &end, 10);
                if (n > 0 && !strcmp(end, ".model")) nums[nn++] = (int)n;
        }
        SDL_free(names);
        for (int a = 0; a < nn; a++)
        {
                int best = a;
                for (int b = a + 1; b < nn; b++)
                        if (nums[b] > nums[best]) best = b;
                int t = nums[a]; nums[a] = nums[best]; nums[best] = t;
        }
        for (int i = 0; i < nn && pmedit_pick_nr < PM_PICK_MAX; i++)
        {
                struct pm_pick_ent *e = &pmedit_pick_ent[pmedit_pick_nr++];
                snprintf(e->label, sizeof e->label, "SAVE %d", nums[i]);
                pm_hist_path(e->path, sizeof e->path, nums[i]);
        }
}

// load the current page's models off disk and upload them to the preview slots
static void pmedit_pick_load_page(void)
{
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                int e = pmedit_pick_page * PM_NR_PREVIEW + c;
                pmedit_pick_loaded[c] = e < pmedit_pick_nr
                        && pmodel_load(&pmedit_pick_model[c], pmedit_pick_ent[e].path);
        }
        pmodel_upload_previews(pmedit_pick_model, pmedit_pick_loaded);
}

static void pmedit_picker_open(void)
{
        pmedit_pick_scan();
        pmedit_pick_page = 0;
        pmedit_pick_spin = 0;
        pmedit_picker = 1;
        pmedit_pick_load_page();
}

static void pmedit_pick_flip(int d) // paginate, clamped
{
        int p = pmedit_pick_page + d;
        if (p < 0 || p >= pmedit_pick_pages()) return;
        pmedit_pick_page = p;
        pmedit_pick_load_page();
}

// choose cell c: adopt its model live, then write a fresh newest snapshot and
// make it this session's file, so the pick loads next launch and later edits
// update it. The undo tracker sees the swap and folds it into the timeline.
static void pmedit_pick_choose(int c)
{
        if (!pmedit_pick_loaded[c]) return;
        pm_models[my_player] = pmedit_pick_model[c];
        pmodel_upload(my_player);
        pmedit_hist_n = pm_hist_newest() + 1;
        pm_hist_write(pmedit_hist_n);
        pmedit_hist_sum = pm_checksum(&pm_models[my_player]);
        pmedit_hist_cool = 1;
        pmedit_picker = 0;
        pmedit_sel = -1;
        pmedit_joint = pmedit_socket = pmedit_parent = pmedit_newpart = 0;
        pmedit_resize = pmedit_face = pmedit_restang = 0;
        pmedit_hide = 0; pmedit_hidden = 0;
}

// the grid is a PM_PICK_COLS x PM_PICK_ROWS block inset from the screen edges,
// with title/hint bands above and below
static void pmedit_pick_cell_rect(int c, float *r)
{
        float mx = 90, top = 130, bot = 100;
        float cw = (screenw - 2 * mx) / PM_PICK_COLS;
        float ch = (screenh - top - bot) / PM_PICK_ROWS;
        int col = c % PM_PICK_COLS, row = c / PM_PICK_COLS;
        float pad = 14;
        r[0] = mx + col * cw + pad;         r[1] = top + row * ch + pad;
        r[2] = mx + (col + 1) * cw - pad;   r[3] = top + (row + 1) * ch - pad;
}

static void pmedit_pick_prev_rect(float *r)
{ r[0] = 90;              r[1] = screenh - 78; r[2] = 240;        r[3] = screenh - 34; }
static void pmedit_pick_next_rect(float *r)
{ r[0] = screenw - 240;  r[1] = screenh - 78; r[2] = screenw - 90; r[3] = screenh - 34; }

static int pmedit_in_rect(float x, float y, const float *r)
{ return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }

// which visible cell the cursor is over, or -1
static int pmedit_pick_hover(void)
{
        float r[4];
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                if (!pmedit_pick_loaded[c]) continue;
                pmedit_pick_cell_rect(c, r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) return c;
        }
        return -1;
}

static void pmedit_pick_click(int btn)
{
        if (btn != SDL_BUTTON_LEFT) return;
        float r[4];
        if (pmedit_pick_pages() > 1)
        {
                pmedit_pick_prev_rect(r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) { pmedit_pick_flip(-1); return; }
                pmedit_pick_next_rect(r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) { pmedit_pick_flip(1); return; }
        }
        int c = pmedit_pick_hover();
        if (c >= 0) pmedit_pick_choose(c);
}

// remote driving hook (remote.c "pmpick"): no arg opens the picker, "N" chooses
// cell N, "next"/"prev" paginate, "close" closes it. Returns the picker state.
int pmedit_remote_pick(const char *arg)
{
        if (!pmedit_on) return 0;
        if (!strncmp(arg, "next", 4)) { if (pmedit_picker) pmedit_pick_flip(1); }
        else if (!strncmp(arg, "prev", 4)) { if (pmedit_picker) pmedit_pick_flip(-1); }
        else if (!strncmp(arg, "close", 5)) pmedit_picker = 0;
        else
        {
                int n;
                if (pmedit_picker && sscanf(arg, "%d", &n) == 1) pmedit_pick_choose(n);
                else pmedit_picker_open();
        }
        return pmedit_picker;
}

// ---- picker geometry + render ---------------------------------------------
static struct allocation pmedit_pick_alloc[MAX_FRAMES_IN_FLIGHT];
static struct pmvert pmedit_pick_buf[PM_NR_PREVIEW * PM_MAX_PIECES * PM_FACES];
static int pmedit_pick_start[PM_NR_PREVIEW]; // per-cell instance range in the buf
static int pmedit_pick_ct[PM_NR_PREVIEW];
static int pmedit_pick_total;                // instances filled this frame
static float pmedit_pick_pv[PM_NR_PREVIEW][16]; // per-cell proj*view

// build the page's thumbnail instances and per-cell cameras. Each loaded model
// resolves to a plain standing pose centered at the origin (spun by the shared
// turntable); its own narrow-FOV camera frames it to its bounding box, and its
// faces sample its preview texture slot.
static void pmedit_pick_emit(void)
{
        struct pmvert *b = pmedit_pick_buf;
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                pmedit_pick_start[c] = b - pmedit_pick_buf;
                pmedit_pick_ct[c] = 0;
                if (!pmedit_pick_loaded[c]) continue;
                struct pmodel *mo = &pmedit_pick_model[c];
                float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16];
                pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                                pmedit_pick_spin, NULL, space, geom, NULL);

                // the standing pose's bounding box (model frame == world here)
                float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
                for (int i = 0; i < mo->nr_pieces; i++)
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, geom[i],
                                (k      & 1) * mo->piece[i].dims[0],
                                (k >> 1 & 1) * mo->piece[i].dims[1],
                                (k >> 2 & 1) * mo->piece[i].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo[a]) lo[a] = p[a];
                                if (p[a] > hi[a]) hi[a] = p[a];
                        }
                }
                float cen[3] = {(lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2};
                float radius = 0.5f * sqrtf((hi[0]-lo[0]) * (hi[0]-lo[0])
                                          + (hi[1]-lo[1]) * (hi[1]-lo[1])
                                          + (hi[2]-lo[2]) * (hi[2]-lo[2]));
                if (radius < 1.f) radius = 1.f;

                // the cell camera: a narrow FOV at the cell's aspect (near
                // orthographic), the eye lifted and pulled back for a 3/4 read
                float r[4]; pmedit_pick_cell_rect(c, r);
                float cw = r[2] - r[0], chh = r[3] - r[1];
                float fov = 24.f, ednear = 100.f, edfar = 1000.f * BS;
                float fw = ednear * tanf(fov * PI / 360.f), fh = fw * chh / cw;
                float pj[16] = {
                        ednear / fw, 0, 0, 0,
                        0, -ednear / fh, 0, 0,
                        0, 0, -edfar / (edfar - ednear), -1,
                        0, 0, -(edfar * ednear) / (edfar - ednear), 0,
                };
                float dist = radius / (0.72f * MIN(fw, fh) / ednear);
                if (dist < radius + 0.3f * BS) dist = radius + 0.3f * BS;

                // sit the model at the real camera eye, so main.frag's shared
                // fog and shadows (keyed on distance from ubo.view_pos) read
                // near zero - otherwise the thumbnails wear the flat sunset-fog
                // tint. Shift every piece's world translation by the same delta;
                // the cell's own camera frames it from there.
                float delta[3] = { peye0 - cen[0], peye1 - cen[1], peye2 - cen[2] };
                for (int i = 0; i < mo->nr_pieces; i++)
                {
                        geom[i][12] += delta[0];
                        geom[i][13] += delta[1];
                        geom[i][14] += delta[2];
                }
                // y is down: a smaller y is higher, so lift the eye by -y
                float eye[3] = { peye0, peye1 - 0.42f * dist, peye2 - 0.90f * dist };
                float f3[3] = { peye0, peye1, peye2 }, view[16];
                lookit(view, f3, eye[0], eye[1], eye[2], NO_PITCH, 0);
                translate(view, -eye[0], -eye[1], -eye[2]);
                mat4_multiply(pmedit_pick_pv[c], pj, view);

                for (int i = 0; i < mo->nr_pieces; i++)
                {
                        float *m = geom[i];
                        for (int f = 0; f < PM_FACES; f++)
                                *b++ = (struct pmvert){
                                        .r0 = { m[0], m[4], m[8],  m[12] },
                                        .r1 = { m[1], m[5], m[9],  m[13] },
                                        .r2 = { m[2], m[6], m[10], m[14] },
                                        .dims = { mo->piece[i].dims[0],
                                                  mo->piece[i].dims[1],
                                                  mo->piece[i].dims[2] },
                                        .orient = f + 1,
                                        .tex = PM_LAYER_PREVIEW + c * pm_slot_layers()
                                                + i * PM_FACES + f,
                                        .illum = 0.9f, .glow = 0.f,
                                };
                        pmedit_pick_ct[c] += PM_FACES;
                }
        }
        pmedit_pick_total = b - pmedit_pick_buf;
}

static void pmedit_pick_render(VkCommandBuffer cmdbuf)
{
        pmedit_pick_emit();
        int fr = vk.currentFrame;
        if (!pmedit_pick_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof pmedit_pick_buf, &pmedit_pick_alloc[fr]);
        if (pmedit_pick_total)
                vulkan_populate_vertex_buffer(pmedit_pick_buf,
                        pmedit_pick_total * sizeof(struct pmvert), &pmedit_pick_alloc[fr]);

        // solid dark modal backdrop, then each visible cell's panel, hover lit
        static struct allocation bg[MAX_FRAMES_IN_FLIGHT], cellbg[MAX_FRAMES_IN_FLIGHT];
        static struct allocation hi[MAX_FRAMES_IN_FLIGHT];
        float full[1][4] = {{ 0, 0, (float)screenw, (float)screenh }};
        pmedit_fill_rects(cmdbuf, bg, full, 1, (float[3]){ 0.05f, 0.05f, 0.07f });
        float cells[PM_NR_PREVIEW][4];
        int nc = 0;
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                if (pmedit_pick_page * PM_NR_PREVIEW + c >= pmedit_pick_nr) continue;
                pmedit_pick_cell_rect(c, cells[nc++]);
        }
        pmedit_fill_rects(cmdbuf, cellbg, cells, nc, (float[3]){ 0.12f, 0.12f, 0.15f });
        int hov = pmedit_pick_hover();
        if (hov >= 0)
        {
                float hr[1][4]; pmedit_pick_cell_rect(hov, hr[0]);
                pmedit_fill_rects(cmdbuf, hi, hr, 1, (float[3]){ 0.20f, 0.23f, 0.32f });
        }

        // the thumbnails: each in its own cell viewport, on its own cleared depth
        if (pmedit_pick_total)
        {
                vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk.pipelines[pmodel_pipe].pipeline);
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pmedit_pick_alloc[fr].buf, &offset);
                for (int c = 0; c < PM_NR_PREVIEW; c++)
                {
                        if (!pmedit_pick_ct[c]) continue;
                        float r[4]; pmedit_pick_cell_rect(c, r);
                        VkViewport vp = { r[0], r[1], r[2] - r[0], r[3] - r[1], 0, 1 };
                        VkRect2D sc = { {(int)r[0], (int)r[1]},
                                {(uint32_t)(r[2] - r[0]), (uint32_t)(r[3] - r[1])} };
                        vkCmdSetViewport(cmdbuf, 0, 1, &vp);
                        vkCmdSetScissor(cmdbuf, 0, 1, &sc);
                        VkClearAttachment ca = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                .clearValue.depthStencil = {1.f, 0} };
                        VkClearRect cr = { .rect = sc, .baseArrayLayer = 0, .layerCount = 1 };
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        struct { float pv[16]; } push;
                        memcpy(push.pv, pmedit_pick_pv[c], sizeof push.pv);
                        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                        0, sizeof push, &push);
                        vkCmdDraw(cmdbuf, 4, pmedit_pick_ct[c], 0, pmedit_pick_start[c]);
                }
                // hand a full-screen viewport/scissor back to the UI passes
                VkViewport vp = { 0, 0, screenw, screenh, 0, 1 };
                VkRect2D sc = { {0, 0}, {screenw, screenh} };
                vkCmdSetViewport(cmdbuf, 0, 1, &vp);
                vkCmdSetScissor(cmdbuf, 0, 1, &sc);
        }
}

// the picker overlay's 2D layer: title, a name under each thumbnail, the page
// controls, and the hint. The thumbnails and panels themselves draw earlier,
// in pmedit_pick_render (they need the 3D pipeline and per-cell depth).
static void pmedit_pick_ui(void)
{
        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];

        char *title = "CHOOSE A MODEL";
        font_begin(screenw, screenh);
        font_add_text(title, screenw / 2.f - strlen(title) * 9.f, 64.f, 3);
        font_end(1, 1, 1);

        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                int e = pmedit_pick_page * PM_NR_PREVIEW + c;
                if (e >= pmedit_pick_nr) continue;
                char *lbl = pmedit_pick_ent[e].label;
                float r[4]; pmedit_pick_cell_rect(c, r);
                float mid = (r[0] + r[2]) / 2.f;
                font_begin(screenw, screenh);
                font_add_text(lbl, mid - strlen(lbl) * 6.f, r[3] - 26.f, 2);
                font_end(0.85f, 0.9f, 0.95f);
        }

        if (pmedit_pick_pages() > 1)
        {
                static struct allocation pgbg[MAX_FRAMES_IN_FLIGHT];
                float pr[4], nr[4];
                pmedit_pick_prev_rect(pr);
                pmedit_pick_next_rect(nr);
                float btns[2][4];
                memcpy(btns[0], pr, sizeof pr);
                memcpy(btns[1], nr, sizeof nr);
                pmedit_fill_rects(cmdbuf, pgbg, btns, 2, (float[3]){ 0.13f, 0.13f, 0.16f });

                float d = pmedit_pick_page > 0 ? 0.85f : 0.3f;
                font_begin(screenw, screenh);
                font_add_text("< PREV", pr[0] + 22, pr[1] + 12, 3);
                font_end(d, d, d);
                d = pmedit_pick_page + 1 < pmedit_pick_pages() ? 0.85f : 0.3f;
                font_begin(screenw, screenh);
                font_add_text("NEXT >", nr[0] + 22, nr[1] + 12, 3);
                font_end(d, d, d);

                char pg[32];
                snprintf(pg, sizeof pg, "PAGE %d / %d",
                                pmedit_pick_page + 1, pmedit_pick_pages());
                font_begin(screenw, screenh);
                font_add_text(pg, screenw / 2.f - strlen(pg) * 6.f, screenh - 60.f, 2);
                font_end(0.7f, 0.7f, 0.7f);
        }

        char *hint = "click a model to switch to it   left/right page   ESC cancel";
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1) scale = 1;
        font_begin(screenw, screenh);
        font_add_text(hint, screenw / 2.f - strlen(hint) * 4.f * scale,
                        screenh - 26.f * scale, 0);
        font_end(1, 1, 1);

        pmedit_cursor_ui();
}

#endif // BLOCKO_PMEDIT_PICKER_C_INCLUDED
