#version 450

// Mob shadow pass: same instanced quads as mob.vert, spun about the vertical
// axis so a mob's shadow matches its facing. Depth-only; pairs with shadow.frag.
// No reject box (mobs are never patched). Push layout matches mob.vert so
// mob_render can push one struct for both the color and shadow pipelines.

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
    float yaw;    // rotate geometry about a vertical axis (0 = none)
    float cx;     // rotation pivot, world x
    float cz;     // rotation pivot, world z
    float shiny;  // unused here; keeps the push layout matching mob.vert
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

    vec4 world_pos = vertex_pos + offsets[i];
    if (push.yaw != 0.0) {
        float s = sin(push.yaw), c = cos(push.yaw);
        vec2 rel = world_pos.xz - vec2(push.cx, push.cz);
        world_pos.xz = vec2(rel.x * c - rel.y * s, rel.x * s + rel.y * c)
                     + vec2(push.cx, push.cz);
    }

    gl_Position = push.pv * world_pos;
    uv = uvs[i];
}
