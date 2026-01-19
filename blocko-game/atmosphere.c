#include "blocko.c"
#ifndef BLOCKO_ATMOSPHERE_C_INCLUDED
#define BLOCKO_ATMOSPHERE_C_INCLUDED

int sky_pipe;
VkBuffer sky_vbuf;
VkDeviceMemory sky_vmem;
int sky_vertex_count;

int sun_pipe;
VkBuffer sun_vbuf;
VkDeviceMemory sun_vmem;

void do_atmos_colors()
{
        if (sun_pitch < PI) // in the day, linearly change the sky color
        {
                night_amt = fmodf(sun_pitch + 3*PI2, TAU) / TAU;
                if (night_amt > 0.5f) night_amt = 1.f - night_amt;
                night_amt *= 2.f;
        }
        else // at night change via cubic-sine so that it's mostly dark all night
        {
                night_amt = 1.f + sinf(sun_pitch);  //  0 to  1
                night_amt *= night_amt * night_amt; //  0 to  1
                night_amt *= -0.5f;                 //-.5 to  0
                night_amt += 1.f;                   //  1 to .5
        }

        if (night_amt > 0.5f)
        {
                fog_r = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_R, FOG_NIGHT_R);
                fog_g = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_G, FOG_NIGHT_G);
                fog_b = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_B, FOG_NIGHT_B);
        }
        else
        {
                fog_r = lerp(2.f*night_amt, FOG_DAY_R, FOG_DUSK_R);
                fog_g = lerp(2.f*night_amt, FOG_DAY_G, FOG_DUSK_G);
                fog_b = lerp(2.f*night_amt, FOG_DAY_B, FOG_DUSK_B);
        }
}

void sun_init()
{
        // Create skydome vertices - full sphere
        #define SKY_RINGS 32
        #define SKY_SEGMENTS 32
        float sky_verts[(SKY_RINGS * SKY_SEGMENTS * 6) * 5]; // 6 verts per quad, 5 floats per vert (pos + uv)
        float *p = sky_verts;

        for (int ring = 0; ring < SKY_RINGS; ring++) {
                float theta0 = (float)ring / SKY_RINGS * PI - PI * 0.5f;       // -PI/2 to PI/2
                float theta1 = (float)(ring + 1) / SKY_RINGS * PI - PI * 0.5f;
                float y0 = sinf(theta0);
                float y1 = sinf(theta1);
                float r0 = cosf(theta0);
                float r1 = cosf(theta1);

                for (int seg = 0; seg < SKY_SEGMENTS; seg++) {
                        float phi0 = (float)seg / SKY_SEGMENTS * TAU;
                        float phi1 = (float)(seg + 1) / SKY_SEGMENTS * TAU;

                        // 4 corners of quad
                        float x00 = r0 * cosf(phi0), z00 = r0 * sinf(phi0);
                        float x01 = r0 * cosf(phi1), z01 = r0 * sinf(phi1);
                        float x10 = r1 * cosf(phi0), z10 = r1 * sinf(phi0);
                        float x11 = r1 * cosf(phi1), z11 = r1 * sinf(phi1);

                        // UV based on height (y) for gradient
                        float u0 = (float)seg / SKY_SEGMENTS;
                        float u1 = (float)(seg + 1) / SKY_SEGMENTS;
                        float v0 = y0;  // height = v for gradient
                        float v1 = y1;

                        // Triangle 1 (reversed winding for inside view)
                        *p++ = x00; *p++ = y0; *p++ = z00; *p++ = u0; *p++ = v0;
                        *p++ = x01; *p++ = y0; *p++ = z01; *p++ = u1; *p++ = v0;
                        *p++ = x10; *p++ = y1; *p++ = z10; *p++ = u0; *p++ = v1;

                        // Triangle 2 (reversed winding for inside view)
                        *p++ = x01; *p++ = y0; *p++ = z01; *p++ = u1; *p++ = v0;
                        *p++ = x11; *p++ = y1; *p++ = z11; *p++ = u1; *p++ = v1;
                        *p++ = x10; *p++ = y1; *p++ = z10; *p++ = u0; *p++ = v1;
                }
        }

        sky_vertex_count = (p - sky_verts) / 5;
        size_t buf_size = (p - sky_verts) * sizeof(float);

        // Create vertex buffer
        VkBufferCreateInfo bufInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = buf_size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(vk.device, &bufInfo, NULL, &sky_vbuf);

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(vk.device, sky_vbuf, &memReq);

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &memProps);
        uint32_t memType = 0;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
                if ((memReq.memoryTypeBits & (1 << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                        memType = i;
                        break;
                }
        }

        VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReq.size,
                .memoryTypeIndex = memType,
        };
        vkAllocateMemory(vk.device, &allocInfo, NULL, &sky_vmem);
        vkBindBufferMemory(vk.device, sky_vbuf, sky_vmem, 0);

        void *data;
        vkMapMemory(vk.device, sky_vmem, 0, buf_size, 0, &data);
        memcpy(data, sky_verts, buf_size);
        vkUnmapMemory(vk.device, sky_vmem);

        // Create pipeline - no depth write, depth test disabled
        VkVertexInputBindingDescription bindingDesc = {
                .binding = 0,
                .stride = 5 * sizeof(float),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        VkVertexInputAttributeDescription attrDescs[] = {
                { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
                { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 3 * sizeof(float) },
        };

        sky_pipe = vulkan_make_pipeline_flags("shaders/sky.vert.spv", NULL, "shaders/sky.frag.spv",
                1, &bindingDesc, 2, attrDescs, PIPE_NO_DEPTH_WRITE);

        // Create sun/moon vertex buffer - two quads as triangle lists
        // Sun at +X, moon at -X (they rotate around origin)
        float sun_verts[] = {
                // Sun quad (2000x2000 at x=10000) - two triangles
                10000, -1000, -1000,  0, 0,
                10000,  1000, -1000,  1, 0,
                10000,  1000,  1000,  1, 1,
                10000, -1000, -1000,  0, 0,
                10000,  1000,  1000,  1, 1,
                10000, -1000,  1000,  0, 1,
                // Moon quad (800x800 at x=-10000) - two triangles
                -10000, -400, -400,  0, 0,
                -10000,  400, -400,  1, 0,
                -10000,  400,  400,  1, 1,
                -10000, -400, -400,  0, 0,
                -10000,  400,  400,  1, 1,
                -10000, -400,  400,  0, 1,
        };

        size_t sun_buf_size = sizeof(sun_verts);
        VkBufferCreateInfo sunBufInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sun_buf_size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(vk.device, &sunBufInfo, NULL, &sun_vbuf);

        VkMemoryRequirements sunMemReq;
        vkGetBufferMemoryRequirements(vk.device, sun_vbuf, &sunMemReq);

        VkMemoryAllocateInfo sunAllocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = sunMemReq.size,
                .memoryTypeIndex = memType,  // reuse memType from sky
        };
        vkAllocateMemory(vk.device, &sunAllocInfo, NULL, &sun_vmem);
        vkBindBufferMemory(vk.device, sun_vbuf, sun_vmem, 0);

        void *sunData;
        vkMapMemory(vk.device, sun_vmem, 0, sun_buf_size, 0, &sunData);
        memcpy(sunData, sun_verts, sun_buf_size);
        vkUnmapMemory(vk.device, sun_vmem);

        // Sun pipeline - same vertex format, no depth write, with blending
        sun_pipe = vulkan_make_pipeline_flags("shaders/sun.vert.spv", NULL, "shaders/sun.frag.spv",
                1, &bindingDesc, 2, attrDescs, PIPE_NO_DEPTH_WRITE | PIPE_BLEND);
}

void sky_draw(VkCommandBuffer cmdbuf, float *proj, float *view)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[sky_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        // Create view matrix without translation (sky follows camera)
        float view_rot[16];
        memcpy(view_rot, view, sizeof view_rot);
        view_rot[12] = 0;  // Clear translation
        view_rot[13] = 0;
        view_rot[14] = 0;

        // Scale the dome to be large
        float scale = 100000.f;
        float scale_mtrx[16] = {
                scale, 0, 0, 0,
                0, scale, 0, 0,
                0, 0, scale, 0,
                0, 0, 0, 1,
        };

        float pv[16], pvm[16];
        mat4_multiply(pv, proj, view_rot);
        mat4_multiply(pvm, pv, scale_mtrx);

        struct { float pvm[16]; float sun_dir[3]; float night_amt; } push;
        memcpy(push.pvm, pvm, sizeof pvm);
        push.sun_dir[0] = cosf(sun_pitch) * cosf(sun_yaw);
        push.sun_dir[1] = sinf(sun_pitch);
        push.sun_dir[2] = -cosf(sun_pitch) * sinf(sun_yaw);
        push.night_amt = night_amt;

        vkCmdPushConstants(cmdbuf, vk.pipelines[sky_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof push, &push);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &sky_vbuf, &offset);
        vkCmdDraw(cmdbuf, sky_vertex_count, 1, 0, 0);
}

void sun_draw(VkCommandBuffer cmdbuf, float *proj, float *view, float pitch, float yaw, float roll)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[sun_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        // Create rotation matrix from pitch/yaw/roll
        float cosa = cosf(pitch);
        float sina = sinf(pitch);
        float cosb = cosf(yaw);
        float sinb = sinf(yaw);
        float cosc = cosf(roll);
        float sinc = sinf(roll);
        float model[] = {
                cosa * cosb,       cosa * sinb * cosc + sina * sinc,      cosa * sinb * sinc - sina * cosc,       0,
                sina * cosb,       sina * sinb * cosc - cosa * sinc,      sina * sinb * sinc + cosa * cosc,       0,
                      -sinb,              cosb * cosc              ,             cosb * sinc              ,       0,
                          0,                                      0,                                     0,       1,
        };

        // View without translation (sun is infinitely far)
        float view_rot[16];
        memcpy(view_rot, view, sizeof view_rot);
        view_rot[12] = 0;
        view_rot[13] = 0;
        view_rot[14] = 0;

        float pv[16], pvm[16];
        mat4_multiply(pv, proj, view_rot);
        mat4_multiply(pvm, pv, model);

        struct { float pvm[16]; float is_moon; float pad[3]; } push;
        memcpy(push.pvm, pvm, sizeof pvm);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &sun_vbuf, &offset);

        // Draw sun (first 6 vertices)
        push.is_moon = 0.0f;
        vkCmdPushConstants(cmdbuf, vk.pipelines[sun_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof push, &push);
        vkCmdDraw(cmdbuf, 6, 1, 0, 0);

        // Draw moon (next 6 vertices)
        push.is_moon = 1.0f;
        vkCmdPushConstants(cmdbuf, vk.pipelines[sun_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof push, &push);
        vkCmdDraw(cmdbuf, 6, 1, 6, 0);
}

#endif // BLOCKO_ATMOSPHERE_C_INCLUDED
