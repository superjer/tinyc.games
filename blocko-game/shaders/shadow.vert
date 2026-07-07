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
    gl_Position = push.pv * (vertex_pos + offsets[i]);
    uv = uvs[i];
}
