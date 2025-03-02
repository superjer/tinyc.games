#version 450

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout (location = 0) in float tex_vs[];
layout (location = 1) in float orient_vs[];
layout (location = 2) in vec4 illum_vs[];
layout (location = 3) in vec4 glow_vs[];
layout (location = 4) in float alpha_vs[];
layout (location = 5) in vec4 world_pos_vs[];

layout (location = 0) flat out float tex;
layout (location = 1) out float illum;
layout (location = 2) out float glow;
layout (location = 3) flat out float alpha;
layout (location = 4) out vec2 uv;
layout (location = 5) flat out float eyedist;
layout (location = 6) out vec4 shadow_pos;
layout (location = 7) out vec4 shadow2_pos;
layout (location = 8) out vec4 world_pos;
layout (location = 9) flat out vec3 normal;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 shadow_space;
    mat4 shadow2_space;
    float BS;
} ubo;

void main(void) {
    float sidel = 0.0f;
    vec4 a, b, c, d;
    mat4 mvp = ubo.proj * ubo.view * ubo.model;

    switch (int(orient_vs[0])) {
        case 1: // UP
            a = vec4( 0, 0, 0, 0);
            b = vec4(ubo.BS, 0, 0, 0);
            c = vec4( 0, 0, ubo.BS, 0);
            d = vec4(ubo.BS, 0, ubo.BS, 0);
            normal = vec3(0, -1, 0);
            sidel = 1.0f;
            break;
        case 2: // EAST
            a = vec4(ubo.BS, 0, ubo.BS, 0);
            b = vec4(ubo.BS, 0, 0, 0);
            c = vec4(ubo.BS, ubo.BS, ubo.BS, 0);
            d = vec4(ubo.BS, ubo.BS, 0, 0);
            normal = vec3(1, 0, 0);
            sidel = 0.9f;
            break;
        case 3: // NORTH
            a = vec4(0, 0, ubo.BS, 0);
            b = vec4(ubo.BS, 0, ubo.BS, 0);
            c = vec4(0, ubo.BS, ubo.BS, 0);
            d = vec4(ubo.BS, ubo.BS, ubo.BS, 0);
            normal = vec3(0, 0, 1);
            sidel = 0.8f;
            break;
        case 4: // WEST
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, ubo.BS, 0);
            c = vec4(0, ubo.BS, 0, 0);
            d = vec4(0, ubo.BS, ubo.BS, 0);
            normal = vec3(-1, 0, 0);
            sidel = 0.9f;
            break;
        case 5: // SOUTH
            a = vec4(ubo.BS, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(ubo.BS, ubo.BS, 0, 0);
            d = vec4(0, ubo.BS, 0, 0);
            normal = vec3(0, 0, -1);
            sidel = 0.8f;
            break;
        case 6: // DOWN
            a = vec4(ubo.BS, ubo.BS, 0, 0);
            b = vec4(0, ubo.BS, 0, 0);
            c = vec4(ubo.BS, ubo.BS, ubo.BS, 0);
            d = vec4(0, ubo.BS, ubo.BS, 0);
            normal = vec3(0, 1, 0);
            sidel = 0.6f;
            break;
    }

    tex = tex_vs[0];
    alpha = alpha_vs[0];

    vec4 vertex_pos = world_pos_vs[0];

    // Compute eye distance
    eyedist = length((ubo.view * vertex_pos).xyz);

    // Emit four vertices forming the quad
    vec4 offsets[4] = { a, b, c, d };
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };

    for (int i = 0; i < 4; i++) {
        world_pos = vertex_pos + offsets[i];
        gl_Position = mvp * world_pos;
        shadow_pos = ubo.shadow_space * world_pos;
        shadow2_pos = ubo.shadow2_space * world_pos;
        uv = uvs[i];
        illum = (0.1 + illum_vs[0][i]) * sidel;
        glow = (0.1 + glow_vs[0][i]) * sidel;
        EmitVertex();
    }

    EndPrimitive();
}
