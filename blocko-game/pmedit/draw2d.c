#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_DRAW2D_C_INCLUDED
#define BLOCKO_PMEDIT_DRAW2D_C_INCLUDED

// pmedit/draw2d.c - tiny 2D draw helpers: solid rects, colored quads, pixel-art glyphs

// draw n solid pixel-space rects (x0,y0,x1,y1 each) in one rgb on the cursor
// pipeline. Each caller passes its OWN per-frame allocation: one populate per
// alloc per frame, so a recorded draw never reads a buffer a later call
// clobbered (the whole command buffer executes at submit).
static void pmedit_fill_rects(VkCommandBuffer cmdbuf, struct allocation *alloc,
                const float (*rects)[4], int n, const float rgb[3])
{
        if (n <= 0) return;
        static float buf[64 * 12];
        if (n > 64) n = 64;
        float *p = buf;
        for (int i = 0; i < n; i++)
        {
                float x0 = rects[i][0], y0 = rects[i][1];
                float x1 = rects[i][2], y1 = rects[i][3];
                *p++ = x0; *p++ = y0; *p++ = x1; *p++ = y1; *p++ = x1; *p++ = y0;
                *p++ = x1; *p++ = y1; *p++ = x0; *p++ = y0; *p++ = x0; *p++ = y1;
        }
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, n * 12 * sizeof *buf, &alloc[vk.currentFrame]);

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[cursor_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        struct { float proj[16]; float color[3]; float pad; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 },
        };
        memcpy(push.color, rgb, sizeof push.color);
        vkCmdPushConstants(cmdbuf, vk.pipelines[cursor_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, n * 6, 1, 0, 0);
}

// one quad into the palette panel's vertex stream, a color per corner -
// same corners, one color = a solid rect; the picker's gradient segments
// use all four
static float *pmedit_quad4(float *p, float x0, float y0, float x1, float y1,
                const float *c00, const float *c10,
                const float *c01, const float *c11)
{
        const struct { float x, y; const float *c; } v[6] = {
                {x0, y0, c00}, {x1, y1, c11}, {x1, y0, c10},
                {x1, y1, c11}, {x0, y0, c00}, {x0, y1, c01},
        };
        for (int i = 0; i < 6; i++)
        {
                *p++ = v[i].x;    *p++ = v[i].y;
                *p++ = v[i].c[0]; *p++ = v[i].c[1];
                *p++ = v[i].c[2]; *p++ = v[i].c[3];
        }
        return p;
}

static float *pmedit_quad(float *p, float x0, float y0, float x1, float y1,
                const float *c)
{
        return pmedit_quad4(p, x0, y0, x1, y1, c, c, c, c);
}

// append one glyph's quads at (mx,my) with hotspot (hx,hy). ccol is the
// packed color 'C' resolves to.
static float *pmedit_glyph(float *p, const char **rows, int hx, int hy,
                float mx, float my, float s, unsigned ccol)
{
        float black[4] = { 0, 0, 0, 1 }, white[4] = { 0.95f, 0.95f, 0.95f, 1 };
        float col[4] = { (ccol & 255) / 255.f, (ccol >> 8 & 255) / 255.f,
                         (ccol >> 16 & 255) / 255.f, 1 };
        for (int r = 0; rows[r]; r++)
                for (int c = 0; rows[r][c]; c++)
                {
                        const float *q;
                        switch (rows[r][c]) {
                        case 'K': q = black; break;
                        case 'W': q = white; break;
                        case 'C': q = col;   break;
                        default:  continue;
                        }
                        float x0 = mx + (c - hx) * s, y0 = my + (r - hy) * s;
                        p = pmedit_quad(p, x0, y0, x0 + s, y0 + s, q);
                }
        return p;
}

#endif // BLOCKO_PMEDIT_DRAW2D_C_INCLUDED
