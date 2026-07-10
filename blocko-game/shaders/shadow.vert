#version 450

// Instanced quads: one instance per face, 4 vertices via triangle strip.
// All inputs are instance-rate attributes (one struct vbufv per face).

layout(location = 0) in float tex_in;
layout(location = 1) in float orient_in;
layout(location = 2) in vec3 pos_in;
layout(location = 3) in vec4 illum_in;
layout(location = 4) in float alpha_in;

layout(location = 0) flat out float tex;
layout(location = 1) out vec2 uv;
layout(location = 2) flat out float alpha;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
    vec4 reject_lo;   // .xyz = window-tile box lo (inclusive); box empty when lo > hi
    vec4 reject_hi;   // faces of cells inside the box are culled (the patch redraws them)
} push;

// only scootx/scootz are read here; the rest replicate the std140 layout so
// those two land at the right offset (matches struct main_ubo / main.vert)
layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 shadow_space;
    float BS;
    float fog_lo;
    float fog_hi;
    vec3 light_pos;
    vec3 view_pos;
    bool shadow_mapping;
    int water_frame;
    float underwater;
    float scootx;
    float scootz;
} ubo;

void main(void) {
    vec4 a, b, c, d;
    float bs = push.bs;

    int o = int(orient_in);

    // tall grass (orient 20/21): a crossed billboard that must cast a shadow
    // matching its visible shape, so replicate main.vert's per-cell rotation
    // and jitter exactly. everything else in the transparent buffer (water,
    // alpha < 1) is collapsed so only grass casts here.
    bool grass = o >= 20;
    // in the transparent buffer only grass casts: collapse water (alpha < 1).
    // terrain draws never carry it, so this is a no-op there.
    if (!grass && alpha_in < 1.0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    switch (o) {
        case 20: // grass plane A: full-height quad spanning X, centered in Z
            a = vec4(bs, 0, bs * 0.5, 0);
            b = vec4(0,  0, bs * 0.5, 0);
            c = vec4(bs, bs, bs * 0.5, 0);
            d = vec4(0,  bs, bs * 0.5, 0);
            break;
        case 21: // grass plane B: full-height quad spanning Z, centered in X
            a = vec4(bs * 0.5, 0, 0, 0);
            b = vec4(bs * 0.5, 0, bs, 0);
            c = vec4(bs * 0.5, bs, 0, 0);
            d = vec4(bs * 0.5, bs, bs, 0);
            break;
        case 1: // UP
            a = vec4(0, 0, 0, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(0, 0, bs, 0);
            d = vec4(bs, 0, bs, 0);
            break;
        case 2: // EAST
            a = vec4(bs, 0, bs, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(bs, bs, 0, 0);
            break;
        case 3: // NORTH
            a = vec4(0, 0, bs, 0);
            b = vec4(bs, 0, bs, 0);
            c = vec4(0, bs, bs, 0);
            d = vec4(bs, bs, bs, 0);
            break;
        case 4: // WEST
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, bs, 0);
            c = vec4(0, bs, 0, 0);
            d = vec4(0, bs, bs, 0);
            break;
        case 5: // SOUTH
            a = vec4(bs, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(bs, bs, 0, 0);
            d = vec4(0, bs, 0, 0);
            break;
        case 6: // DOWN
            a = vec4(bs, bs, 0, 0);
            b = vec4(0, bs, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(0, bs, bs, 0);
            break;
    }

    // tall grass rotation + jitter, identical to main.vert so the cast shadow
    // lines up with the rendered blades. hashed on the absolute cell (window
    // coords minus scoot) so it stays put across chunk boundaries.
    if (grass) {
        vec3 gcell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / bs + pos_in;
        gcell.x -= ubo.scootx;
        gcell.z -= ubo.scootz;
        vec2 h = floor(gcell.xz);
        float ang = fract(sin(dot(h, vec2(127.1, 311.7))) * 43758.5453) * 6.2831853;
        float jx  = (fract(sin(dot(h, vec2(269.5,  183.3))) * 43758.5453) - 0.5) * 0.45 * bs;
        float jz  = (fract(sin(dot(h, vec2(419.2,  371.9))) * 43758.5453) - 0.5) * 0.45 * bs;
        float ca = cos(ang), sa = sin(ang);
        vec4 g[4] = vec4[4](a, b, c, d);
        for (int k = 0; k < 4; k++) {
            vec2 rel = vec2(g[k].x, g[k].z) - bs * 0.5;
            g[k].x = rel.x * ca - rel.y * sa + bs * 0.5 + jx;
            g[k].z = rel.x * sa + rel.y * ca + bs * 0.5 + jz;
        }
        a = g[0]; b = g[1]; c = g[2]; d = g[3];
    }

    vec3 world = push.bs * pos_in + vec3(push.chunk_x, push.chunk_y, push.chunk_z);
    vec4 vertex_pos = vec4(world, 1.0);
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    tex = tex_in;
    alpha = alpha_in;
    vec4 world_pos = vertex_pos + offsets[i];
    gl_Position = push.pv * world_pos;

    // reject: cull the shadow of any face whose block-cell is in the pending edit
    // box (the patch casts the corrected shadow instead). Mirrors main.vert.
    vec3 cell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / push.bs + pos_in;
    if (all(greaterThanEqual(cell, push.reject_lo.xyz - 0.5)) &&
        all(lessThanEqual(cell, push.reject_hi.xyz + 0.5)))
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    uv = uvs[i];
}
