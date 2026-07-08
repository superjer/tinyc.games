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
    float yaw;    // rotate geometry about a vertical axis (0 = none)
    float cx;     // rotation pivot, world x
    float cz;     // rotation pivot, world z
    float shiny;  // unused here; keeps layout matching main.vert
} push;

void main(void) {
    vec4 a, b, c, d;
    float bs = push.bs;

    switch (int(orient_in)) {
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

    vec3 world = push.bs * pos_in + vec3(push.chunk_x, push.chunk_y, push.chunk_z);
    vec4 vertex_pos = vec4(world, 1.0);
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    tex = tex_in;
    alpha = alpha_in;
    // match main.vert's rotation so a spun mob casts a matching shadow
    vec4 world_pos = vertex_pos + offsets[i];
    if (push.yaw != 0.0) {
        float s = sin(push.yaw), c = cos(push.yaw);
        vec2 rel = world_pos.xz - vec2(push.cx, push.cz);
        world_pos.xz = vec2(rel.x * c - rel.y * s, rel.x * s + rel.y * c)
                     + vec2(push.cx, push.cz);
    }

    gl_Position = push.pv * world_pos;

    // reject: cull the shadow of any face whose block-cell is in the pending edit
    // box (the patch casts the corrected shadow instead). Mirrors main.vert.
    vec3 cell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / push.bs + pos_in;
    if (all(greaterThanEqual(cell, push.reject_lo.xyz - 0.5)) &&
        all(lessThanEqual(cell, push.reject_hi.xyz + 0.5)))
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    uv = uvs[i];
}
