#version 450

// Mob geometry: instanced quads (one instance per face) like the terrain, but
// each piece spins about a vertical axis so the mob faces its heading. Shares
// main.frag with the terrain/water pipelines; the varying interface below must
// match main.vert's outputs exactly. No reject box (mobs are never patched).

layout(location = 0) in float tex_in;
layout(location = 1) in float orient_in;
layout(location = 2) in vec3 pos_in;
layout(location = 3) in vec4 illum_in;
layout(location = 4) in vec4 glow_in;
layout(location = 5) in float alpha_in;

layout(location = 0) flat out float tex;
layout(location = 1) out float illum;
layout(location = 2) out float glow;
layout(location = 3) flat out float alpha;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec4 world_pos_out;
layout(location = 6) out vec4 shadow_pos;
layout(location = 8) flat out vec3 normal;
layout(location = 9) flat out float shiny;  // slimes read as wet/glossy
layout(location = 10) flat out float tint;  // mobs are never tinted -> 0

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (the one near cascade)
    float BS;              // offset 256
    vec3 day_color;        // offset 272
    vec3 glo_color;        // offset 288
    float fog_lo;          // offset 300
    float fog_hi;          // offset 304
    vec3 light_pos;        // offset 320
    vec3 view_pos;         // offset 336
    float sharpness;       // offset 348
    bool shadow_mapping;   // offset 352
} ubo;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
    float yaw;    // rotate geometry about a vertical axis (0 = none)
    float cx;     // rotation pivot, world x
    float cz;     // rotation pivot, world z
    float shiny;  // >0 = wet/glossy surface (slimes)
} push;

void main(void) {
    float sidel = 0.0f;
    vec4 a, b, c, d;
    vec3 face_normal;
    float bs = push.bs;

    switch (int(orient_in)) {
        case 1: // UP (Y-)
            a = vec4(0, 0, 0, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(0, 0, bs, 0);
            d = vec4(bs, 0, bs, 0);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);
            break;
        case 2: // EAST (X+)
            a = vec4(bs, 0, bs, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(bs, bs, 0, 0);
            sidel = 0.9f;
            face_normal = vec3(1, 0, 0);
            break;
        case 3: // NORTH (Z+)
            a = vec4(0, 0, bs, 0);
            b = vec4(bs, 0, bs, 0);
            c = vec4(0, bs, bs, 0);
            d = vec4(bs, bs, bs, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, 1);
            break;
        case 4: // WEST (X-)
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, bs, 0);
            c = vec4(0, bs, 0, 0);
            d = vec4(0, bs, bs, 0);
            sidel = 0.9f;
            face_normal = vec3(-1, 0, 0);
            break;
        case 5: // SOUTH (Z-)
            a = vec4(bs, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(bs, bs, 0, 0);
            d = vec4(0, bs, 0, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, -1);
            break;
        case 6: // DOWN (Y+)
            a = vec4(bs, bs, 0, 0);
            b = vec4(0, bs, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(0, bs, bs, 0);
            sidel = 0.6f;
            face_normal = vec3(0, 1, 0);
            break;
    }

    vec3 world = push.bs * pos_in + vec3(push.chunk_x, push.chunk_y, push.chunk_z);
    vec4 vertex_pos = vec4(world, 1.0);
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    tex = tex_in;
    alpha = alpha_in;
    normal = face_normal;
    shiny = push.shiny;
    tint = 0.0;

    vec4 world_pos = vertex_pos + offsets[i];

    // spin the whole piece about a vertical axis so mobs face where they head
    if (push.yaw != 0.0) {
        float s = sin(push.yaw), c = cos(push.yaw);
        vec2 rel = world_pos.xz - vec2(push.cx, push.cz);
        world_pos.xz = vec2(rel.x * c - rel.y * s, rel.x * s + rel.y * c)
                     + vec2(push.cx, push.cz);
        normal = vec3(normal.x * c - normal.z * s, normal.y,
                      normal.x * s + normal.z * c);
    }

    gl_Position = push.pv * world_pos;

    illum = (0.1 + illum_in[i]) * sidel;
    glow = (0.1 + glow_in[i]) * sidel;
    uv = uvs[i];
    world_pos_out = world_pos;

    // Near-cascade shadow position (mid/far computed in the fragment shader).
    // Use the rotated normal so the offset bias follows the spun face.
    vec4 shadow_sample_pos = world_pos + vec4(normal * 75.0, 0.0);
    shadow_pos = ubo.shadow_space * shadow_sample_pos;
}
