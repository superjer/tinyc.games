#version 450

// Player model geometry: instanced quads (one instance per prism face) like
// the mobs, but each instance carries its piece's full px->world transform as
// three matrix rows, so pieces articulate about arbitrary pivots. Shares
// main.frag; the varying interface below must match main.vert's outputs.

layout(location = 0) in vec4 r0_in;   // piece transform rows: world = M * (px,1)
layout(location = 1) in vec4 r1_in;
layout(location = 2) in vec4 r2_in;
layout(location = 3) in vec3 dims_in; // prism size in px
layout(location = 4) in vec4 misc_in; // orient, tex layer, illum, glow

layout(location = 0) flat out float tex;
layout(location = 1) out float illum;
layout(location = 2) out float glow;
layout(location = 3) flat out float alpha;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec4 world_pos_out;
layout(location = 6) out vec4 shadow_pos;
layout(location = 8) flat out vec3 normal;
layout(location = 9) flat out float shiny;
layout(location = 10) flat out float tint;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space[6];  // offset 192 (near, mid, far_a, far_b, ext_a, ext_b)
    float BS;              // offset 576
    float shadow_far_blend;  // offset 580 (far: 0=A, 1=B)
    float shadow_ext_blend;  // offset 584 (extreme: 0=A, 1=B)

    vec3 day_color;        // offset 592
    vec3 glo_color;        // offset 608
    float fog_lo;          // offset 620
    float fog_hi;          // offset 624
    vec3 light_pos;        // offset 640
    vec3 view_pos;         // offset 656
    float sharpness;       // offset 668
    bool shadow_mapping;   // offset 672
} ubo;

layout(push_constant) uniform Push {
    mat4 pv;
} push;

void main(void) {
    float sidel = 0.0f;
    vec3 a, b, c, d;
    vec3 face_normal;
    vec3 s = dims_in;
    vec2 extent; // face size in px, for sampling from the tile's top-left

    switch (int(misc_in.x)) {
        case 1: // UP (Y-)
            a = vec3(0, 0, 0);
            b = vec3(s.x, 0, 0);
            c = vec3(0, 0, s.z);
            d = vec3(s.x, 0, s.z);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);
            extent = s.xz;
            break;
        case 2: // EAST (X+)
            a = vec3(s.x, 0, s.z);
            b = vec3(s.x, 0, 0);
            c = vec3(s.x, s.y, s.z);
            d = vec3(s.x, s.y, 0);
            sidel = 0.9f;
            face_normal = vec3(1, 0, 0);
            extent = s.zy;
            break;
        case 3: // NORTH (Z+)
            a = vec3(0, 0, s.z);
            b = vec3(s.x, 0, s.z);
            c = vec3(0, s.y, s.z);
            d = vec3(s.x, s.y, s.z);
            sidel = 0.8f;
            face_normal = vec3(0, 0, 1);
            extent = s.xy;
            break;
        case 4: // WEST (X-)
            a = vec3(0, 0, 0);
            b = vec3(0, 0, s.z);
            c = vec3(0, s.y, 0);
            d = vec3(0, s.y, s.z);
            sidel = 0.9f;
            face_normal = vec3(-1, 0, 0);
            extent = s.zy;
            break;
        case 5: // SOUTH (Z-)
            a = vec3(s.x, 0, 0);
            b = vec3(0, 0, 0);
            c = vec3(s.x, s.y, 0);
            d = vec3(0, s.y, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, -1);
            extent = s.xy;
            break;
        case 6: // DOWN (Y+)
            a = vec3(s.x, s.y, 0);
            b = vec3(0, s.y, 0);
            c = vec3(s.x, s.y, s.z);
            d = vec3(0, s.y, s.z);
            sidel = 0.6f;
            face_normal = vec3(0, 1, 0);
            extent = s.xz;
            break;
    }

    vec3 corners[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    vec4 px = vec4(corners[i], 1.0);
    vec4 world_pos = vec4(dot(r0_in, px), dot(r1_in, px), dot(r2_in, px), 1.0);

    // rows are rotation * uniform scale; normalize recovers the rotated normal
    normal = normalize(vec3(dot(r0_in.xyz, face_normal),
                            dot(r1_in.xyz, face_normal),
                            dot(r2_in.xyz, face_normal)));

    tex = misc_in.y;
    alpha = 1.0;
    shiny = 0.0;
    tint = 0.0;

    gl_Position = push.pv * world_pos;

    illum = (0.1 + misc_in.z) * sidel;
    glow = (0.1 + misc_in.w) * sidel;
    uv = uvs[i] * extent / 16.0;
    world_pos_out = world_pos;

    // Near-cascade shadow position (mid/far computed in the fragment shader)
    vec4 shadow_sample_pos = world_pos + vec4(normal * 75.0, 0.0);
    shadow_pos = ubo.shadow_space[0] * shadow_sample_pos;
}
