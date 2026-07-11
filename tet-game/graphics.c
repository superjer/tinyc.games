#include "tet.c"
#ifndef TET_GRAPHICS_C_INCLUDED
#define TET_GRAPHICS_C_INCLUDED

#define VBUFLEN 40000
#define VBUF_REGION (8 * sizeof vbuf) // per-swapchain-image GPU space, all draw_end()s in a frame must fit

int main_pipe;
VkBuffer vbuf_gpu;
VkDeviceMemory vbuf_gpu_memory;
char *vbuf_mapped;
size_t vbuf_gpu_offset;
float vbuf[VBUFLEN];
int vbuf_n;
float color_r, color_g, color_b;

// render a line of text optionally with a %d value in it
void text(char *fstr, int value)
{
        if (!fstr) return;
        char str[100];
        snprintf(str, 99, fstr, value);
        font_begin(win_x, win_y);
        font_add_text(str, text_x, text_y, 3.f * bs / 4 / FONT_CH_H); // scale is a multiple of the base glyph size
        font_end(1, 1, 1);
        text_y += bs * 125 / 100 + (fstr[strlen(fstr) - 1] == ' ' ? bs : 0);
}

void draw_setup()
{
        VkVertexInputBindingDescription bindingDesc = {
                .binding = 0,
                .stride = 5 * sizeof(float),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        VkVertexInputAttributeDescription attributeDescs[] = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
                {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 2 * sizeof(float)},
        };
        main_pipe = vulkan_make_pipeline("tet.vert", NULL, "tet.frag",
                1, &bindingDesc, 2, attributeDescs, NULL, VK_NULL_HANDLE,
                PIPE_NO_DEPTH_TEST | PIPE_NO_CULL);

        // vertex buffer with one region per swapchain image, so we never
        // overwrite vertices a frame in flight is still reading
        VkBufferCreateInfo buf_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = VBUF_REGION * vk.swapchainImageCount,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(vk.device, &buf_info, NULL, &vbuf_gpu);

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(vk.device, vbuf_gpu, &mem_reqs);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &mem_props);
        uint32_t mem_type = 0;
        VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++)
                if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                                (mem_props.memoryTypes[i].propertyFlags & wanted) == wanted)
                {
                        mem_type = i;
                        break;
                }

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_reqs.size,
                .memoryTypeIndex = mem_type,
        };
        vkAllocateMemory(vk.device, &alloc_info, NULL, &vbuf_gpu_memory);
        vkBindBufferMemory(vk.device, vbuf_gpu, vbuf_gpu_memory, 0);
        vkMapMemory(vk.device, vbuf_gpu_memory, 0, buf_info.size, 0, (void **)&vbuf_mapped);
}

void vertex(float x, float y, float r, float g, float b)
{
        if (vbuf_n >= VBUFLEN - 5) return;
        vbuf[vbuf_n++] = x;
        vbuf[vbuf_n++] = y;
        vbuf[vbuf_n++] = r;
        vbuf[vbuf_n++] = g;
        vbuf[vbuf_n++] = b;
}

void rect(float x, float y, float w, float h)
{
        vertex(x    , y    , color_r, color_g, color_b);
        vertex(x + w, y    , color_r, color_g, color_b);
        vertex(x    , y + h, color_r, color_g, color_b);
        vertex(x    , y + h, color_r, color_g, color_b);
        vertex(x + w, y    , color_r, color_g, color_b);
        vertex(x + w, y + h, color_r, color_g, color_b);
}

void draw_start()
{
        vulkan_acquire_next();
        font_frame_reset();
        vbuf_gpu_offset = 0;
        vbuf_n = 0;
}

void draw_end()
{
        if (vbuf_n == 0) return;

        size_t bytes = vbuf_n * sizeof(float);
        if (vbuf_gpu_offset + bytes > VBUF_REGION)
        {
                fprintf(stderr, "vbuf GPU region overflow (%zu + %zu)\n", vbuf_gpu_offset, bytes);
                vbuf_n = 0;
                return;
        }

        size_t base = vk.imageIndex * VBUF_REGION;
        memcpy(vbuf_mapped + base + vbuf_gpu_offset, vbuf, bytes);

        VkCommandBuffer cmd = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // pixel coords to Vulkan clip space, origin top-left, Y down
        float ortho[16] = {
                2.f / win_x, 0, 0, 0,
                0, 2.f / win_y, 0, 0,
                0, 0, 1, 0,
               -1, -1, 0, 1,
        };
        vkCmdPushConstants(cmd, vk.pipelines[main_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof ortho, ortho);

        VkDeviceSize vb_offset = base + vbuf_gpu_offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf_gpu, &vb_offset);
        vkCmdDraw(cmd, vbuf_n / 5, 1, 0, 0);

        if (vbuf_n > VBUFLEN * 3 / 4)
                fprintf(stderr, "vbuf fullness (%d/%d)\n", vbuf_n, VBUFLEN);
        vbuf_gpu_offset += bytes;
        vbuf_n = 0;
}

// set the current draw color to the color assoc. with a shape
void set_color_from_shape(int shape, int shade)
{
        color_r = MAX((colors[shape] >> 16 & 0xFF) + shade, 0) / 255.f;
        color_g = MAX((colors[shape] >>  8 & 0xFF) + shade, 0) / 255.f;
        color_b = MAX((colors[shape] >>  0 & 0xFF) + shade, 0) / 255.f;
}

void set_color(int r, int g, int b)
{
        color_r = r / 255.f;
        color_g = g / 255.f;
        color_b = b / 255.f;
}

// draw a single mino (square) of a shape
void draw_mino(int x, int y, int shape, int outline, int part)
{
        if (!part) return;
        int bw = MAX(1, outline ? bs / 10 : bs / 6);
        set_color_from_shape(shape, -50);
        rect(x, y, bs, bs);
        set_color_from_shape(shape, outline ? -255 : 0);
        rect( // horizontal band
                        x + (part & 8 ? 0 : bw),
                        y + bw,
                        bs - (part & 8 ? 0 : bw) - (part & 2 ? 0 : bw),
                        bs - bw - bw);
        rect( // vertical band
                        x + (part & 32 ? 0 : bw),
                        y + (part & 1 ? 0 : bw),
                        bs - (part & 32 ? 0 : bw) - (part & 16 ? 0 : bw),
                        bs - (part & 1 ? 0 : bw) - (part & 4 ? 0 : bw));
}

#define CENTER 1
#define OUTLINE 2

void draw_shape(int x, int y, int color, int rot, int flags)
{
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_mino(
                        x + bs * i + ((flags & CENTER) ? bs2 + bs2 * center[2 * color    ] : 0),
                        y + bs * j + ((flags & CENTER) ? bs  + bs2 * center[2 * color + 1] : 0),
                        color,
                        flags & OUTLINE,
                        is_solid_part(color, rot, i, j)
                );
}

// draw everything in the game on the screen for current player
void draw_player()
{
        int x = p->board_x + bs * p->shake_x;
        int y = p->board_y + bs * p->shake_y;

        // draw background, black boxes
        set_color(16, 26, 24);
        rect(p->held.x, p->held.y, p->box_w, p->box_w);
        rect(x, y, p->board_w, bs * VHEIGHT);

        // find ghost piece position
        int ghost_y = p->it.y;
        while (ghost_y < BHEIGHT && !collide(p->it.x, ghost_y + 1, p->it.rot))
                ghost_y++;

        // draw shadow
        if (p->it.color)
        {
                struct shadow shadow = shadows[p->it.rot][p->it.color];
                int top = MAX(0, p->it.y + shadow.y - 5);
                set_color(8, 13, 12);
                rect(x + bs * (p->it.x + shadow.x),
                     y + bs * top,
                     bs * shadow.w,
                     MAX(0, bs * (ghost_y - top + shadow.y - 5)));
        }

        // draw hard drop beam
        float loss = .1f * (tick - p->beam_tick);
        if (loss < 1.f && p->beam.color)
        {
                struct shadow shadow = shadows[p->beam.rot][p->beam.color];
                int rw = bs * shadow.w;
                int rh = bs * (p->beam.y + shadow.y - 5);
                int lossw = (1.f - ((1.f - loss) * (1.f - loss))) * rw;
                int lossh = loss < .5f ? 0.f : (1.f - ((1.f - loss) * (1.f - loss))) * rh;
                set_color(66, 74, 86);
                rect(x + bs * (p->beam.x + shadow.x) + lossw / 2,
                     y + lossh,
                     rw - lossw,
                     rh - lossh);
        }

        // draw pieces on board
        for (int i = 0; i < BWIDTH; i++) for (int j = 0; j < BHEIGHT; j++)
                draw_mino(x + bs * i, y + bs * (j-5) - p->row[j].offset,
                                p->row[j].col[i].color, 0, p->row[j].col[i].part);

        // draw falling piece & ghost
        draw_shape(x + bs * p->it.x, y + bs * (ghost_y - 5), p->it.color, p->it.rot, OUTLINE);
        draw_shape(x + bs * p->it.x, y + bs * (p->it.y - 5), p->it.color, p->it.rot, 0);

        // draw row crash
        if (p->crash_time > 0 && p->row[p->crash_row].offset < bs * 2)
        {
                p->crash_time = MIN(p->crash_time, 10);
                set_color(255, 255, 255);
                int h = MAX(2, p->row[p->crash_row].offset);
                int w = 200 - p->crash_time * 20;
                int crash_y = y + (p->crash_row - BHEIGHT + VHEIGHT + 1) * bs - h;
                rect(x - w,
                     crash_y,
                     p->board_w + w * 2,
                     h);

                if (p->crash_time == 5 && p->combo >= 2)
                {
                        audio_tone(SQUARE, A0, C1, 100, 20, 40, 1000);
                        audio_tone(SQUARE, D1, F1, 60, 20, 40, 300);
                        audio_tone(SQUARE, G1, B1, 20, 20, 40, 100);
                        p->shake_y += .04f;
                }
        }

        // draw next pieces
        for (int n = 0; n < 5; n++)
                draw_shape(p->preview_x, p->preview_y + 3 * bs * n, p->next[n], 0, CENTER);

        // draw held piece
        draw_shape(p->held.x, p->held.y, p->held.color, 0, CENTER);

        draw_end();

        // draw scores etc
        text_x = p->held.x;
        text_y = p->held.y + p->box_w + bs2;
        text("%d pts ", p->score);
        text("%d lines ", p->lines);

        int secs = p->ticks / 120 % 60;
        int mins = p->ticks / 120 / 60 % 60;
        char minsec[80];
        sprintf(minsec, "%d:%02d.%02d ", mins, secs, p->ticks % 120 * 1000 / 1200);
        text(minsec, 0);
        if (p->combo > 1) text("%d combo ", p->combo);
        if (p->tspin == TSPIN_FULL)
                text("T-SPIN", 0);
        else if (p->tspin == TSPIN_MINI)
                text("T-SPIN MINI", 0);

        if (p->reward)
        {
                text_x = p->reward_x - bs;
                text_y = p->reward_y--;
                text("%d", p->reward);
        }

        text_x = x + bs2;
        text_y = y + bs2 * 19;
        if (p->countdown_time > 0)
                text(countdown_msg[p->countdown_time / CTDN_TICKS], 0);
}

// recalculate sizes and positions on resize
void resize(int x, int y)
{
        win_x = x;
        win_y = y;
        bs = MIN(win_x / 22, win_y / 24);
        bs2 = bs / 2;
        bs4 = bs / 4;
        line_height = bs * 125 / 100;
        p->board_x = (x / 2) - bs2 * BWIDTH;
        p->board_y = (y / 2) - bs2 * VHEIGHT;
        p->board_w = bs * 10;
        p->box_w = bs * 5;
        p->held.x = p->board_x - p->box_w - bs2;
        p->held.y = p->board_y;
        p->preview_x = p->board_x + p->board_w + bs2;
        p->preview_y = p->board_y;
}

#endif // TET_GRAPHICS_C_INCLUDED
