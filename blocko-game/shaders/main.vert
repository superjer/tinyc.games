#version 450

// Instanced quads: one instance per face, 4 vertices via triangle strip.
// All inputs are instance-rate attributes (one struct vbufv per face).

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
    vec3 fog_color;        // offset 624
    float fog_lo;          // offset 636
    float fog_hi;          // offset 640
    vec3 light_pos;        // offset 656
    vec3 view_pos;         // offset 672
    float sharpness;       // offset 684
    bool shadow_mapping;   // offset 688
} ubo;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
    vec4 reject_lo;   // .xyz = window-tile box lo (inclusive); box empty when lo > hi
    vec4 reject_hi;   // faces of cells inside the box are culled (the patch redraws them)
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

    // reject: if this face's block-cell is inside the pending edit box, collapse
    // the whole quad to a zero-area degenerate so it emits no fragments. pos_in
    // is the cell (chunk-local m,y,n); reconstruct the absolute window cell the
    // same way world position is built. The patch mesh redraws the box.
    vec3 cell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / push.bs + pos_in;
    if (all(greaterThanEqual(cell, push.reject_lo.xyz - 0.5)) &&
        all(lessThanEqual(cell, push.reject_hi.xyz + 0.5)))
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    illum = (0.1 + illum_in[i]) * sidel;
    glow = (0.1 + glow_in[i]) * sidel;
    uv = uvs[i];
    world_pos_out = world_pos;

    // Calculate shadow space position for near cascade only
    // Mid/far cascade positions are computed in fragment shader (for A/B blending)
    // Normal offset bias prevents shadow bleeding at cube edges
    // Offset must exceed PCF world radius
    vec4 shadow_sample_pos = world_pos + vec4(face_normal * 75.0, 0.0);
    shadow_pos = ubo.shadow_space[0] * shadow_sample_pos;  // shadow_space[0] = near cascade
}
