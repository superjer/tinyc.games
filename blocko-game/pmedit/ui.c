#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_UI_C_INCLUDED
#define BLOCKO_PMEDIT_UI_C_INCLUDED

// pmedit/ui.c - the editor's 2D furniture: panels, palette, labels, hints

// the editor's flat 2D furniture: the group-container panels, then the button
// chips on top, then the lit row behind the selected type. All solid rects on
// the cursor pipeline, recorded before the text so the labels land on top.
// Geometry comes straight from pmedit_btn_rect, so the chips ARE the hit rects.
static void pmedit_boxes()
{
        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        int st = pmedit_ui_state();

        float grp[4][4]; int ng = 0;   // group-container panels
        float chip[16][4]; int nc = 0; // button chips
        struct pmodel *mo = &pm_models[my_player];

        // BACK / X rides every non-picker state
        if (pmedit_btn_rect(PB_BACK, chip[nc])) nc++;

        static const int piece_btns[] = {
                PB_RESTANG, PB_JOINT, PB_SOCKET, PB_PARENT,
                PB_RESIZE, PB_COPY, PB_HIDE, PB_DELETE };
        static const int model_btns[] = { PB_LOAD, PB_NEWPART, PB_ANIMATE };

        if (st == PMEDIT_S_NONE)
        {
                grp[ng][0] = PMEDIT_COL_X0; grp[ng][1] = 24;
                grp[ng][2] = PMEDIT_COL_X1; grp[ng][3] = 212; ng++;
                for (int i = 0; i < 3; i++)
                        if (pmedit_btn_rect(model_btns[i], chip[nc])) nc++;
        }
        else if (st == PMEDIT_S_PIECE)
        {
                static const float box[3][2] = {
                        { 16, 300 }, { 312, 540 }, { 552, 728 } };
                for (int i = 0; i < 3; i++)
                {
                        grp[ng][0] = PMEDIT_COL_X0; grp[ng][1] = box[i][0];
                        grp[ng][2] = PMEDIT_COL_X1; grp[ng][3] = box[i][1]; ng++;
                }
                for (int i = 0; i < 8; i++)
                        if (pmedit_btn_rect(piece_btns[i], chip[nc])) nc++;
        }
        else if (st == PMEDIT_S_ANIM)
        {
                if (pmedit_btn_rect(PB_STYLE, chip[nc])) nc++;
        }
        // MODAL: nothing but the BACK chip already queued

        static struct allocation ga[MAX_FRAMES_IN_FLIGHT];
        static struct allocation ca[MAX_FRAMES_IN_FLIGHT];
        pmedit_fill_rects(cmdbuf, ga, grp, ng, (float[3]){ 0.05f, 0.05f, 0.07f });

        // the lit bar behind the selected type row, under its chip layer
        if (st == PMEDIT_S_PIECE && pmedit_sel >= 0)
        {
                int ty = mo->piece[pmedit_sel].type;
                float hr[1][4] = {{ PMEDIT_COL_X0 + 4,
                        PMEDIT_TYPE_Y0 + ty * PMEDIT_TYPE_RH, PMEDIT_COL_X1 - 4,
                        PMEDIT_TYPE_Y0 + ty * PMEDIT_TYPE_RH + PMEDIT_TYPE_RH }};
                static struct allocation ha[MAX_FRAMES_IN_FLIGHT];
                pmedit_fill_rects(cmdbuf, ha, hr, 1, (float[3]){ 0.16f, 0.26f, 0.16f });
        }

        pmedit_fill_rects(cmdbuf, ca, chip, nc, (float[3]){ 0.13f, 0.13f, 0.16f });
}

// build and draw the palette panel: backdrop, the 16 swatches (a white ring
// on the selected one, a checker on transparent), and the hue-lightness
// picker as six vertex-colored segments - between adjacent primary and
// secondary corners linear interpolation IS the hue wheel, so the gradient
// is exact, and pmedit_pal_pick computes the same colors the pixels show
static void pmedit_palette_ui()
{
        struct pmodel *mo = &pm_models[my_player];
        static float buf[34 * 6 * 6]; // 34 quads is the panel's worst case
        float *p = buf;

        p = pmedit_quad(p, PMEDIT_SW_X0 - PMEDIT_PAL_PAD,
                        PMEDIT_SW_Y0 - PMEDIT_PAL_PAD,
                        PMEDIT_HSL_X1 + PMEDIT_PAL_PAD,
                        PMEDIT_SF_Y1 + PMEDIT_PAL_PAD,
                        (float[4]){ 0.08f, 0.08f, 0.10f, 1 });

        // the flood buttons' boxes; their labels ride the font pass and
        // read yellow while the mode is on, like the right-hand column
        p = pmedit_quad(p, PMEDIT_HSL_X0, PMEDIT_FF_Y0,
                        PMEDIT_HSL_X1, PMEDIT_FF_Y1,
                        (float[4]){ 0.13f, 0.13f, 0.16f, 1 });
        p = pmedit_quad(p, PMEDIT_HSL_X0, PMEDIT_SF_Y0,
                        PMEDIT_HSL_X1, PMEDIT_SF_Y1,
                        (float[4]){ 0.13f, 0.13f, 0.16f, 1 });

        for (int i = 0; i < PM_NR_COLORS; i++)
        {
                float x0 = PMEDIT_SW_X0 + i % 8 * PMEDIT_SW_STRIDE;
                float y0 = PMEDIT_SW_Y0 + i / 8 * PMEDIT_SW_STRIDE;
                float x1 = x0 + PMEDIT_SW_SZ, y1 = y0 + PMEDIT_SW_SZ;
                if (i == pmedit_color) // the ring: a bigger white rect under
                        p = pmedit_quad(p, x0 - 3, y0 - 3, x1 + 3, y1 + 3,
                                        (float[4]){ 1, 1, 1, 1 });
                if (!i) // transparent wears the classic light/dark checker
                {
                        float xm = x0 + PMEDIT_SW_SZ / 2.f;
                        float ym = y0 + PMEDIT_SW_SZ / 2.f;
                        p = pmedit_quad(p, x0, y0, x1, y1,
                                        (float[4]){ 0.75f, 0.75f, 0.75f, 1 });
                        p = pmedit_quad(p, x0, y0, xm, ym,
                                        (float[4]){ 0.4f, 0.4f, 0.4f, 1 });
                        p = pmedit_quad(p, xm, ym, x1, y1,
                                        (float[4]){ 0.4f, 0.4f, 0.4f, 1 });
                        continue;
                }
                unsigned c = mo->palette[i];
                p = pmedit_quad(p, x0, y0, x1, y1, (float[4]){
                                (c       & 255) / 255.f,
                                (c >>  8 & 255) / 255.f,
                                (c >> 16 & 255) / 255.f, 1 });
        }

        float ym = (PMEDIT_HSL_Y0 + PMEDIT_HSL_Y1) / 2.f;
        const float wht[4] = { 1, 1, 1, 1 }, blk[4] = { 0, 0, 0, 1 };
        for (int k = 0; k < 6; k++)
        {
                float x0 = PMEDIT_HSL_X0
                        + (PMEDIT_HSL_X1 - PMEDIT_HSL_X0) * k / 6.f;
                float x1 = PMEDIT_HSL_X0
                        + (PMEDIT_HSL_X1 - PMEDIT_HSL_X0) * (k + 1) / 6.f;
                float h0[4] = { pmedit_hue6[k][0], pmedit_hue6[k][1],
                                pmedit_hue6[k][2], 1 };
                float h1[4] = { pmedit_hue6[k + 1][0], pmedit_hue6[k + 1][1],
                                pmedit_hue6[k + 1][2], 1 };
                p = pmedit_quad4(p, x0, PMEDIT_HSL_Y0, x1, ym,
                                wht, wht, h0, h1);
                p = pmedit_quad4(p, x0, ym, x1, PMEDIT_HSL_Y1,
                                h0, h1, blk, blk);
        }

        static struct allocation alloc[MAX_FRAMES_IN_FLIGHT];
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, (p - buf) * sizeof *buf,
                        &alloc[vk.currentFrame]);

        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmui_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        struct { float proj[16]; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 }, // pixel space, y 0 at the top
        };
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmui_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, (int)(p - buf) / 6, 1, 0, 0);
}

// a button's label, vertically centred in its chip and left-padded; drawn
// yellow when its mode is active, else in (cr,cg,cb). No-op if the button
// isn't present in this state.
static void pmedit_label(int b, const char *s, float scale, int active,
                float cr, float cg, float cb)
{
        float r[4];
        if (!pmedit_btn_rect(b, r)) return;
        float y = r[1] + ((r[3] - r[1]) - 12 * scale) / 2.f;
        font_begin(screenw, screenh);
        font_add_text((char *)s, r[0] + 12, y, scale);
        if (active) font_end(1, 1, 0.25f);
        else font_end(cr, cg, cb);
}

// horizontally centred text at a given baseline
static void pmedit_center(const char *s, float y, float scale,
                float cr, float cg, float cb)
{
        font_begin(screenw, screenh);
        font_add_text((char *)s, screenw / 2.f - strlen(s) * 4.f * scale, y, scale);
        font_end(cr, cg, cb);
}

// a small heading at the top of a right-column group box
static void pmedit_group_head(const char *s, float y)
{
        font_begin(screenw, screenh);
        font_add_text((char *)s, PMEDIT_COL_X0 + 8, y, 2);
        font_end(0.5f, 0.55f, 0.62f);
}

void pmedit_draw_ui()
{
        if (!pmedit_on) return;
        if (pmedit_picker) { pmedit_pick_ui(); return; }

        pmedit_boxes();
        int st = pmedit_ui_state();
        struct pmodel *mo = &pm_models[my_player];

        float hs = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (hs < 1) hs = 1;
        float hy = screenh - 30.f * hs; // bottom hint baseline

        // BACK / X, top-left in every editing state
        {
                float r[4]; pmedit_btn_rect(PB_BACK, r);
                font_begin(screenw, screenh);
                if (st == PMEDIT_S_NONE) font_add_text("X", r[0] + 16, r[1] + 8, 3);
                else font_add_text("< BACK", r[0] + 12, r[1] + 12, 2);
                font_end(0.85f, 0.85f, 0.92f);
        }

        if (st == PMEDIT_S_NONE)
        {
                pmedit_center("EDIT MODEL", 22.f, 2, 0.6f, 0.65f, 0.72f);
                pmedit_group_head("MODEL", 30.f);
                pmedit_label(PB_LOAD,    "LOAD",     3, 0, 0.7f, 0.85f, 1.f);
                pmedit_label(PB_NEWPART, "NEW PART", 3, 0, 0.55f, 0.7f, 0.55f);
                pmedit_label(PB_ANIMATE, "ANIMATE",  3, 0, 0.55f, 0.7f, 0.55f);
                pmedit_center("click a piece to edit   WASD rotate   X or U close",
                                hy, hs, 1, 1, 1);
                pmedit_cursor_ui();
                return;
        }

        if (st == PMEDIT_S_PIECE)
        {
                // the paint palette + its flood labels, down the left side
                pmedit_palette_ui();
                font_begin(screenw, screenh);
                font_add_text("FLOOD FILL", PMEDIT_HSL_X0 + 10, PMEDIT_FF_Y0 + 10.f, 3);
                if (pmedit_flood == 1) font_end(1, 1, 0.25f);
                else font_end(0.55f, 0.7f, 0.55f);
                font_begin(screenw, screenh);
                font_add_text("SUPER FLOOD", PMEDIT_HSL_X0 + 10, PMEDIT_SF_Y0 + 10.f, 3);
                if (pmedit_flood == 2) font_end(1, 1, 0.25f);
                else font_end(0.55f, 0.7f, 0.55f);

                // group 1 - PART TYPE: the type as a list, lit row selected,
                // RESTING ANGLE bundled beneath it
                pmedit_group_head("PART TYPE", 20.f);
                int ty = mo->piece[pmedit_sel].type;
                for (int i = 0; i < PM_T_COUNT; i++)
                {
                        font_begin(screenw, screenh);
                        font_add_text((char *)pmedit_type_name[i],
                                PMEDIT_COL_X0 + 12,
                                PMEDIT_TYPE_Y0 + i * PMEDIT_TYPE_RH + 3, 2);
                        if (i == ty) font_end(0.85f, 1.f, 0.85f);
                        else font_end(0.45f, 0.5f, 0.45f);
                }
                pmedit_label(PB_RESTANG, "RESTING ANGLE", 2, pmedit_restang,
                                0.55f, 0.7f, 0.55f);

                // group 2 - EDIT GEOMETRY: the four modal tools
                pmedit_group_head("EDIT GEOMETRY", 316.f);
                pmedit_label(PB_JOINT, "ATTACHMENT POINT", 2, pmedit_joint,
                                0.55f, 0.7f, 0.55f);
                pmedit_label(PB_SOCKET, "MOVE PART", 2, pmedit_socket,
                                0.55f, 0.7f, 0.55f);
                pmedit_label(PB_PARENT,
                                mo->piece[pmedit_sel].parent >= 0
                                        ? "DETACH" : "SELECT PARENT",
                                2, 0, 1.f, 0.35f, 0.66f); // pink like the rims
                pmedit_label(PB_RESIZE, "RESIZE", 2, pmedit_resize,
                                0.55f, 0.7f, 0.55f);

                // group 3 - THIS PART: copy / hide / delete, one contained box
                pmedit_group_head("THIS PART", 556.f);
                int can_new = mo->nr_pieces < PM_MAX_PIECES;
                pmedit_label(PB_COPY, "MAKE COPY", 2, 0,
                                can_new ? 0.55f : 0.3f, can_new ? 0.7f : 0.3f,
                                can_new ? 0.55f : 0.3f);
                int nhid = 0;
                for (unsigned m = pmedit_hidden; m; m >>= 1) nhid += m & 1;
                char hidebuf[24] = "HIDE";
                if (nhid) sprintf(hidebuf, "UNHIDE (%d)", nhid);
                pmedit_label(PB_HIDE, hidebuf, 2, 0, 0.55f, 0.7f, 0.55f);
                pmedit_label(PB_DELETE, "DELETE", 2, 0, 0.8f, 0.35f, 0.35f);

                char *hint = pmedit_flood == 2 ?
                        "LMB recoat the whole piece   RMB copy a color" :
                        pmedit_flood ?
                        "LMB fill the same-color region   RMB copy a color" :
                        "LMB paint   RMB copy a color   ctrl-C copy piece   Del delete";
                pmedit_center(hint, hy, hs, 1, 1, 1);
                pmedit_cursor_ui();
                return;
        }

        // MODAL or ANIM: a top-centre title + a control line, panels cleared
        static char restbuf[80];
        if (pmedit_restang && pmedit_sel >= 0)
                sprintf(restbuf, "PITCH %+d   YAW %+d   ROLL %+d"
                        "   arrows pitch/yaw   Q/E roll",
                        mo->piece[pmedit_sel].rest[0],
                        mo->piece[pmedit_sel].rest[1],
                        mo->piece[pmedit_sel].rest[2]);

        char *title = pmedit_joint  ? "PLACE ATTACHMENT POINT" :
                      pmedit_socket ? "MOVE PART" :
                      pmedit_parent ? (pmedit_newpart ? "PLACE NEW PART"
                                                      : "SELECT PARENT") :
                      pmedit_resize  ? "RESIZE" :
                      pmedit_restang ? "RESTING ANGLE" :
                      pmedit_hide    ? "HIDE PARTS" : "ANIMATE";
        char *sub = pmedit_joint ?
                "point to preview   LMB place the point   arrows nudge   WASD rotate" :
                pmedit_socket ?
                "point to preview   LMB place the part   arrows nudge   WASD rotate" :
                pmedit_parent ?
                (pmedit_newpart ?
                "LMB place the new piece on a parent   BACK cancels" :
                "LMB place the part on its new parent   BACK cancels") :
                pmedit_resize ?
                (pmedit_face ?
                "drag the face to resize   arrows push/pull 1px   LMB pick another face" :
                "LMB pick a face, then drag it to resize   WASD rotate") :
                pmedit_restang ? restbuf :
                pmedit_hide ?
                "click parts to hide them   click away or BACK when done   WASD rotate" :
                "WASD rotate   click anywhere or BACK to stop";

        // pink title in the parent flow to match its rims, yellow otherwise
        if (pmedit_parent) pmedit_center(title, 20.f, 3, 1, 0.55f, 0.8f);
        else               pmedit_center(title, 20.f, 3, 1, 1, 0.25f);
        pmedit_center(sub, 62.f, 1, 0.85f, 0.85f, 0.9f);

        // the WALK/FLAIL toggle rides under the title, ANIMATE only
        if (st == PMEDIT_S_ANIM)
        {
                float r[4]; pmedit_btn_rect(PB_STYLE, r);
                pmedit_center(mo->style == PM_STYLE_FLAIL ? "FLAIL" : "WALK",
                                r[1] + 10, 2, 0.7f, 0.85f, 0.7f);
        }

        pmedit_cursor_ui(); // the tool cursor, on top of everything
}

#endif // BLOCKO_PMEDIT_UI_C_INCLUDED
