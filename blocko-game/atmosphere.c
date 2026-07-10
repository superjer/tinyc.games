#include "blocko.c"
#ifndef BLOCKO_ATMOSPHERE_C_INCLUDED
#define BLOCKO_ATMOSPHERE_C_INCLUDED

int sun_pipe;

void sun_init()
{
        // Sun pipeline - no vertex inputs, the quad is generated in the shader
        sun_pipe = vulkan_make_pipeline("sun.vert", NULL, "sun.frag",
                0, NULL, 0, NULL, NULL, VK_NULL_HANDLE, PIPE_NO_DEPTH_WRITE | PIPE_BLEND | PIPE_NO_CULL | PIPE_DEPTH_LESS_EQUAL);
}

void sun_draw(VkCommandBuffer cmdbuf, float *proj, float *view)
{
        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };

        // View without translation (the sun is infinitely far)
        float view_rot[16];
        memcpy(view_rot, view, sizeof view_rot);
        view_rot[12] = 0;
        view_rot[13] = 0;
        view_rot[14] = 0;

        float pv[16];
        mat4_multiply(pv, proj, view_rot);

        struct { float pv[16]; float sun_dir[3]; float time; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        memcpy(push.sun_dir, sun_dir, sizeof push.sun_dir);
        push.time = (float)pframe;

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[sun_pipe].pipeline);
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        vkCmdPushConstants(cmdbuf, vk.pipelines[sun_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof push, &push);
        vkCmdDraw(cmdbuf, 6, 1, 0, 0);
}

#endif // BLOCKO_ATMOSPHERE_C_INCLUDED
