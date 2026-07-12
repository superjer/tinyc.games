#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_GLYPHS_C_INCLUDED
#define BLOCKO_PMEDIT_GLYPHS_C_INCLUDED

// pmedit/glyphs.c - the tool cursors: pixel-art glyph tables + the cursor overlay

// the editor draws its own cursor, a little pixel-art glyph that names the
// tool in hand - the OS cursor is hidden while editing. Each glyph is a
// NULL-terminated list of equal-length rows; the legend is 'K' black
// outline, 'W' white, 'C' the current paint color (so the brush and bucket
// carry the color they'll lay), ' ' transparent. The hotspot (the pixel
// that sits exactly on the pointer) is given per glyph.
static const char *pmg_pointer[] = { // cursor_default.png, tip top-left (0,0)
        "KK             ",
        "KWK            ",
        "KWWK           ",
        "KWWWK          ",
        "KWWWWK         ",
        "KWWWWWK        ",
        "KWWWWWWK       ",
        "KWWWWWWWK      ",
        "KWWWWWWWWK     ",
        "KWWWWWWWWWK    ",
        "KWWWWWWWWWWK   ",
        "KWWWWWWWWWWWK  ",
        "KWWWWWWWWWWWWK ",
        "KWWWWWWWWWWWWWK",
        "KWWWWWWKKKKKKK ",
        "KWWWWWWWK      ",
        "KWWWWKWWK      ",
        "KWWWK KWWK     ",
        "KWWK  KWWK     ",
        "KWK    KWWK    ",
        " K     KWWK    ",
        "        KWWK   ",
        "        KWWK   ",
        "         KK    ",
        0 };
static const char *pmg_parent[] = { // cursor_parenter.png, tip top-left (0,0)
        "KK               ",
        "KWK              ",
        "KWWK             ",
        "KWWWK            ",
        "KWWWWK           ",
        "KWWWWWK          ",
        "KWWWWWWK         ",
        "KWWWWWWWK        ",
        "KWWWWWWWWK       ",
        "KWWWWWWWWWK      ",
        "KWWWWWWWWWWK     ",
        "KWWWWWWWWWWWK    ",
        "KWWWWWWKKKKKK    ",
        "KWWWWKK          ",
        "KWWWKKKKKKKKK    ",
        "KWWK KCCCCCCCKK  ",
        "KWK  KCCCCCCCCCK ",
        "KWK  KCCCCCCCCCK ",
        "KK   KCCCKKKCCCCK",
        "     KCCCK  KCCCK",
        "     KCCCK  KCCCK",
        "     KCCCK  KCCCK",
        "     KCCCKKKCCCCK",
        "     KCCCCCCCCCK ",
        "     KCCCCCCCCCK ",
        "     KCCCCCCCKK  ",
        "     KCCCKKKK    ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KKKKK       ",
        0 };
// the four detailed tools are traced from blocko-game/assets/cursors/*.png
// (black/white/red/transparent -> K/W/C/space). Brush and dropper have their
// business end at the top-left tip (hotspot 0,0); the buckets pour from the
// bottom-left (hotspots set where the glyph is chosen below).
static const char *pmg_brush[] = { // cursor_paintbrush.png, tip top-left
        "KK                           ",
        "KCK                          ",
        "KCK                          ",
        "KCCKKKKKK                    ",
        "KCCCCCCCCK                   ",
        " KCCCCCCCCK                  ",
        "  KCCCCCCCWK                 ",
        "  KCCCCCCCWK                 ",
        " KCCCCCCCWWK                 ",
        " KCCCCCWWWWK                 ",
        " KCCCWWWWWKWK                ",
        "  KWWWWWWKWWK                ",
        "   KKWWWKWWKK                ",
        "     KKKWWKWWK               ",
        "        KKWWWWK              ",
        "          KWWWWK             ",
        "           KWWWWK            ",
        "            KWWWWK           ",
        "             KWWWWK          ",
        "              KWWWWK         ",
        "               KWWWWK        ",
        "                KWWWWK       ",
        "                 KWWWWK      ",
        "                  KWWWWK     ",
        "                   KWWWWK    ",
        "                    KWWWWK   ",
        "                     KWWWWK  ",
        "                      KWWWWK ",
        "                       KWWWWK",
        "                        KWWWK",
        "                         KWWK",
        "                          KK ",
        0 };
static const char *pmg_dropper[] = { // cursor_dropper.png, tip top-left
        "KK                           ",
        "KCK                          ",
        " KCKK                        ",
        "  KCCK                       ",
        "  KCCCK                      ",
        "   KCCCK                     ",
        "    KCCCK                    ",
        "     KCCCK                   ",
        "      KCCCK                  ",
        "       KCCCK                 ",
        "        KCWWK                ",
        "         KWWWK               ",
        "          KWWWK              ",
        "           KWWWK             ",
        "            KWWWK            ",
        "             KWWWK           ",
        "              KWWWK          ",
        "               KWWWK KK      ",
        "                KWWKKKK      ",
        "                 KKKKK       ",
        "                  KKKKKK     ",
        "                 KKKKKKKKKK  ",
        "                 KK KKKKKKKK ",
        "                    KKKKKKKKK",
        "                     KKKKKKKK",
        "                     KKKKKKKK",
        "                     KKKKKKKK",
        "                      KKKKKK ",
        "                       KKKK  ",
        0 };
static const char *pmg_bucket[] = { // cursor_bucket.png, pours bottom-left
        "           K               ",
        "          KWK              ",
        "          KWK              ",
        "          KWK              ",
        "        KKKWK              ",
        "       KWWKWK              ",
        "      KWKKKWKKK            ",
        "     KWKKKKWKWWKK          ",
        "     KWKKKKWKWWWCKK        ",
        "    KWKKKKKWKWWWCCCKK      ",
        "    KWKKKKKWKWWWCCCCCKK    ",
        "   KWKKKKKKWKWWWCCCCCWWKK  ",
        "   KWKKKKKKWKWWWCCCCCWWWWK ",
        "   KWKKKKKKWWKWCCCCCCWWWWWK",
        "  KKKKKKKWKWWKWCCCCCWWWWWWK",
        "  KCCCCCKWWKKWWCCCCCWWWWWWK",
        " KCCCCCKWWWWWWCCCCCCWWWWWWK",
        " KCCCCKWWWWWWCCCCCCCWWWWWWK",
        "KCCCCKWWWWWWWCCCCCCWWWWWWK ",
        "KCCCKWWWWWWWCCCCCCCWWWWWWK ",
        "KCCCKKWWWWWCCCCCCCWWWWWWWK ",
        "KCCK  KKWWCCCCCCCWWWWWWWK  ",
        "KCCK    KKCCCCCCWWWWWWWWK  ",
        "KCCK      KKCCCWWWWWWWWK   ",
        "KCCK        KKWWWWWWWWK    ",
        "KCK           KKWWWWWK     ",
        "KCK             KKWWK      ",
        "KCK               KK       ",
        " K                         ",
        0 };
static const char *pmg_super[] = { // cursor_superbucket.png, pours bottom-left
        "                   K      KK       ",
        "                  KWK   KKWWK      ",
        "                  KWK KKWWWWWK     ",
        "                  KWKKWWWWWWWWK    ",
        "                  KWKWWWWKKWWWWK   ",
        "                KKKWKWWWKKKKWWWWK  ",
        "              KKWWKWKWWWKKKKWWWWK  ",
        "            KKWWWWKWKWWWKKKKWWWWWK ",
        "           KKWWWWWKWKWWWKKKKWWWWWK ",
        "          KKCKWWWWKWKWWWKKKKWWWWWK ",
        "       KKKKCCCKWWWKWKWWWKKKKWWWWWWK",
        "      KCCCCCCCCKWWKWKWWWWKKWWWWWWWK",
        "     KCCCCCCCCCCKWKWKWWWWKKWWWWWWWK",
        "     KCCCKCCKKKKKKKWWKWWWWWWWWWWWWK",
        "     KCCK KKWKCCCKKWWKWWWKKWWWWWWWK",
        "      KK   KWKCCCKKKKWWWWKKWWWWWWK ",
        "           KWKCCCCKWWWWWWWWWWWWKK  ",
        "            KCCCCCKWWWWWWWWWWKK    ",
        "  KKK      KKCCCCCCKWWWWWWWKK      ",
        " KCCCK    KCCCCCCCCKWWWWWKK        ",
        " KCCK   KKCCCCCCCCCKWWWKK          ",
        "  KK   KCCCCCCCCKKKKWKK            ",
        "      KCCCCCCCCKWWWKK              ",
        "      KCCCCCCCK KKK                ",
        "     KCCCKCCCCK                    ",
        "     KCCKCCCCCK                    ",
        "     KCCKCCCCCK                    ",
        "    KCCCCCCCCK                     ",
        "    KCCCCCCCCK                     ",
        "   KCCCCCCCCCK                     ",
        "   KCCCCCCCCCK                     ",
        "  KKCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCKCCCCK                    ",
        " KKCCCCCCKKCCCK                    ",
        "KKCCCCCCCK KKK                     ",
        "KCCCCKKCCK                         ",
        "KCCCK  KK                          ",
        " KKK                               ",
        0 };
static const char *pmg_resize[] = { // cursor_resizer.png, center hotspot
        "                   KK            ",
        "                  KWK            ",
        "                  KWK            ",
        "                 KWWK            ",
        "                 KWWK            ",
        "                KWWWK            ",
        "                KWWWK            ",
        "               KWWWWK            ",
        "               KWWWWK            ",
        "              KWWWWWK            ",
        "              KWWWWWK            ",
        "      K      KWWWWWWK     K      ",
        "     KWK     KWWWWWWK    KWK     ",
        "    KWWK    KWWWWWWWK    KWWK    ",
        "   KWWWK    KWWWWWWWK    KWWWK   ",
        "  KWWWWK    KWWWWWWWK    KWWWWK  ",
        " KWWWWWKKKKKKWWWWKKKKKKKKKWWWWWK ",
        "KWWWWWWWWWWWKWWWWWWWWWWWWWWWWWWWK",
        " KWWWWWKKKKKKWWWWKKKKKKKKKWWWWWK ",
        "  KWWWWK    KWWWWWWWK    KWWWWK  ",
        "   KWWWK    KWWWWWWWK    KWWWK   ",
        "    KWWK    KWWWWWWWK    KWWK    ",
        "     KWK    KWWWWWWK     KWK     ",
        "      K     KWWWWWWK      K      ",
        "            KWWWWWK              ",
        "            KWWWWWK              ",
        "            KWWWWK               ",
        "            KWWWWK               ",
        "            KWWWK                ",
        "            KWWWK                ",
        "            KWWK                 ",
        "            KWWK                 ",
        "            KWK                  ",
        "            KWK                  ",
        "            KK                   ",
        0 };
static const char *pmg_mover[] = { // cursor_mover.png, center hotspot
        "                K                ",
        "               KWK               ",
        "              KWWWK              ",
        "             KWWWWWK             ",
        "            KWWWWWWWK            ",
        "           KWWWWWWWWWK           ",
        "          KWWWWWWWWWWWK          ",
        "           KKKKKWKKKKK           ",
        "               KWK               ",
        "               KWK               ",
        "      K        KWK        K      ",
        "     KWK       KWK       KWK     ",
        "    KWWK        K        KWWK    ",
        "   KWWWK                 KWWWK   ",
        "  KWWWWK                 KWWWWK  ",
        " KWWWWWKKKKK    K    KKKKKWWWWWK ",
        "KWWWWWWWWWWWK  KWK  KWWWWWWWWWWWK",
        " KWWWWWKKKKK    K    KKKKKWWWWWK ",
        "  KWWWWK                 KWWWWK  ",
        "   KWWWK                 KWWWK   ",
        "    KWWK        K        KWWK    ",
        "     KWK       KWK       KWK     ",
        "      K        KWK        K      ",
        "               KWK               ",
        "               KWK               ",
        "           KKKKKWKKKKK           ",
        "          KWWWWWWWWWWWK          ",
        "           KWWWWWWWWWK           ",
        "            KWWWWWWWK            ",
        "             KWWWWWK             ",
        "              KWWWK              ",
        "               KWK               ",
        "                K                ",
        0 };

static const char *pmg_hide[] = { // cursor_eye.png, tip top-left (0,0)
        "KK                      ",
        "KWK                     ",
        "KWWK                    ",
        "KWWWK                   ",
        "KWWWWK                  ",
        "KWWWWWK                 ",
        "KWWWWWWK                ",
        "KWWWWWWWK               ",
        "KWWWWWWWWK              ",
        "KWWWWWWWWWK             ",
        "KWWWWWWWWWWK  K   K     ",
        "KWWWWWWWWWWWK K  K   K  ",
        "KWWWWWWKKKKKK K  K  K   ",
        "KWWWWKK             K   ",
        "KWWWK     KKKKKKK      K",
        "KWWK   KKKWWWWWWWKKK  K ",
        "KWK   KWWKWKKKWWWKWWK   ",
        "KWK  KWWKWKWKKKWWWKWWK  ",
        "KK  KWWWKWKKKKKWWWKWWWK ",
        "    KWWWKWKKKKKWWWKWWWK ",
        "     KWWKWWKKKWWWWKWWK  ",
        "      KWWKWWWWWWWKWWK   ",
        "       KKKWWWWWWWKKK    ",
        "          KKKKKKK       ",
        0 };
static const char *pmg_rotate[] = { // cursor_rotate.png, hotspot (9,9)
        "        K                     ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "        K                     ",
        " KKKKK     KKKKK              ",
        "KWWWWWK K KWWWWWK      K      ",
        " KKKKK     KKKKK      KWK     ",
        "        K             KWWK    ",
        "       KWK            KWWWK   ",
        "       KWK        KKKKKWWWWK  ",
        "       KWK      KKWWWWWWWWWWK ",
        "       KWK     KWWWWWWWWWWWWWK",
        "       KWK    KWWWWWWWWWWWWWK ",
        "        K    KWWWWWWKKKWWWWK  ",
        "             KWWWWKK  KWWWK   ",
        "            KWWWWK    KWWK    ",
        "            KWWWWK    KWK     ",
        "            KWWWK      K      ",
        "            KWWWK             ",
        "         KKKKWWWKKKK          ",
        "        KWWWWWWWWWWWK         ",
        "         KWWWWWWWWWK          ",
        "          KWWWWWWWK           ",
        "           KWWWWWK            ",
        "            KWWWK             ",
        "             KWK              ",
        "              K               ",
        0 };

static void pmedit_cursor_ui()
{
        // which tool is in hand? over the button column or the palette panel
        // it's a plain pointer (you're clicking UI); on the model it's the
        // active paint tool, or the mode's own glyph. Every glyph is traced
        // art drawn at 2x, so it all stays about a cursor's height
        const char **g = pmg_pointer;
        int hx = 0, hy = 0;
        float gs = 2.f;
        // the right column only counts as UI where it actually holds buttons
        // (NONE and PIECE); modal/anim states leave it empty, so the tool glyph
        // should show over there. BACK and the anim STYLE chip are always UI.
        int st = pmedit_ui_state();
        int over_ui = pmedit_in_btn(PB_BACK, pmedit_mx, pmedit_my)
                || pmedit_in_btn(PB_STYLE, pmedit_mx, pmedit_my)
                || ((st == PMEDIT_S_NONE || st == PMEDIT_S_PIECE)
                        && pmedit_mx >= PMEDIT_BTN_X - 10)
                || (pmedit_panel_on() && pmedit_in_panel(pmedit_mx, pmedit_my));
        if (over_ui)
                ; // pointer
        else if (pmedit_panel_on())
        {
                // a copy modifier held previews the eyedropper (RMB copies too)
                if (SDL_GetModState() & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL
                                | SDL_KMOD_ALT | SDL_KMOD_GUI))
                        g = pmg_dropper; // tip top-left (0,0)
                else if (pmedit_flood == 2) { g = pmg_super;  hy = 39; } // pour tip
                else if (pmedit_flood == 1) { g = pmg_bucket; hy = 27; } // pour tip
                else                          g = pmg_brush;  // tip top-left (0,0)
        }
        // the positioning modes get their own glyphs: a four-way arrow to
        // resize (centered), a move cross to place a part or its attachment
        // point (centered), and the P cursor to pick a new parent (tip 0,0)
        else if (pmedit_resize)          { g = pmg_resize; hx = 16; hy = 17; }
        else if (pmedit_joint || pmedit_socket) { g = pmg_mover; hx = 16; hy = 16; }
        else if (pmedit_parent)            g = pmg_parent; // tip top-left (0,0)
        else if (pmedit_restang)         { g = pmg_rotate; hx = 9; hy = 9; }
        else if (pmedit_hide)              g = pmg_hide;   // tip top-left (0,0)

        unsigned ccol = pmedit_color ? pm_models[my_player].palette[pmedit_color]
                                     : PM_RGB(150, 150, 150); // eraser: gray
        if (g == pmg_parent) // its P reads pink, like the parent button/rims
                ccol = PM_RGB(255, 70, 160);
        static float buf[1500 * 6 * 6]; // the superbucket's worst case
        float *p = pmedit_glyph(buf, g, hx, hy, pmedit_mx, pmedit_my, gs, ccol);

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
                         -1, -1, 0, 1 },
        };
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmui_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, (int)(p - buf) / 6, 1, 0, 0);
}

#endif // BLOCKO_PMEDIT_GLYPHS_C_INCLUDED
