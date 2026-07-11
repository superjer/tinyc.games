#include "blocko.c"
#ifndef BLOCKO_CURSOR_C_INCLUDED
#define BLOCKO_CURSOR_C_INCLUDED

unsigned int cursor_prog_id;
unsigned int cursor_vbo, cursor_vao;

int cursor_screenw;
int cursor_screenh;

float cursor_buf[200];
float *cursor_buf_p = cursor_buf;

int cursor_pipe;

void cursor_rect(int x0, int y0, int x1, int y1)
{
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y0;

        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y1;
}

void cursor(VkCommandBuffer cmdbuf)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[cursor_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        static struct allocation allocation = {};
        static int last_w = 0, last_h = 0;
        int w = vk.bestSwapchainExtent.width;
        int h = vk.bestSwapchainExtent.height;
        int w2 = w/2;
        int h2 = h/2;
        static int outer_n;
        static int inner_n;

        if (!allocation.buf || w != last_w || h != last_h)
        {
                cursor_buf_p = cursor_buf;

                cursor_rect(w2 - 25 + 1, h2 -  1 + 1, w2 -  9 + 1, h2 +  1 + 1);
                cursor_rect(w2 +  9 + 1, h2 -  1 + 1, w2 + 25 + 1, h2 +  1 + 1);
                cursor_rect(w2 -  1 + 1, h2 - 25 + 1, w2 +  1 + 1, h2 -  9 + 1);
                cursor_rect(w2 -  1 + 1, h2 +  9 + 1, w2 +  1 + 1, h2 + 25 + 1);

                outer_n = (cursor_buf_p - cursor_buf) / 2;

                cursor_rect(w2 - 25, h2 -  1, w2 -  9, h2 +  1);
                cursor_rect(w2 +  9, h2 -  1, w2 + 25, h2 +  1);
                cursor_rect(w2 -  1, h2 - 25, w2 +  1, h2 -  9);
                cursor_rect(w2 -  1, h2 +  9, w2 +  1, h2 + 25);

                inner_n = (cursor_buf_p - cursor_buf) / 2;

                if (!allocation.buf)
                        vulkan_allocate_vertex_buffer(sizeof cursor_buf, &allocation);
                vulkan_populate_vertex_buffer(cursor_buf, sizeof cursor_buf, &allocation);

                last_w = w;
                last_h = h;
        }

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &allocation.buf, offsets);

        //push constants
        float near = -100.f;
        float far = 100.f;
        float x = 1.f / (w / 2.f);
        float y = -1.f / (h / 2.f);
        float z = -1.f / ((far - near) / 2.f);
        float tz = -(far + near) / (far - near);
        struct {
                float ortho[16];
                float incolor[3];
                float padding;
        } push = {
                {
                        x, 0, 0,  0,
                        0, y, 0,  0,
                        0, 0, z,  0,
                       -1, 1, tz, 1
                },
                { 0.0f, 0.0f, 0.0f }
        };

        vkCmdPushConstants(
                cmdbuf,
                vk.pipelines[cursor_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(push),
                &push
        );
        vkCmdDraw(cmdbuf, outer_n, 1, 0, 0);

        // crosshair whitens normally but reddens as a block is being mined
        push.incolor[0] = 1.0f;
        push.incolor[1] = 1.0f - mine_frac;
        push.incolor[2] = 1.0f - mine_frac;
        vkCmdPushConstants(
                cmdbuf,
                vk.pipelines[cursor_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(push),
                &push
        );
        vkCmdDraw(cmdbuf, inner_n - outer_n, 1, outer_n, 0);
}

void cursor_init()
{                         
        VkVertexInputAttributeDescription attributeDesc = {};
        attributeDesc.location = 0;
        attributeDesc.binding = 0;
        attributeDesc.format = VK_FORMAT_R32G32_SFLOAT; // Matches vec2
        attributeDesc.offset = 0;
        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = 0; // Must match attribute binding
        bindingDesc.stride = sizeof(float) * 2; // vec2: 2 floats
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        cursor_pipe = vulkan_make_pipeline(
                "cursor.vert", NULL, "cursor.frag",
                1, &bindingDesc, 1, &attributeDesc, NULL, VK_NULL_HANDLE, PIPE_NO_DEPTH_TEST | PIPE_NO_CULL
        );
}

#endif // BLOCKO_CURSOR_C_INCLUDED
