#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in float tex_vs[];
layout(location = 1) in float orient_vs[];
layout(location = 2) in float alpha_vs[];
layout(location = 3) in vec4 world_pos_vs[];

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

    switch (int(orient_vs[0])) {
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

    float tex_val = tex_vs[0];
    float alpha_val = alpha_vs[0];

    vec4 vertex_pos = world_pos_vs[0];
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };

    for (int i = 0; i < 4; i++) {
        // Set flat outputs inside loop (driver bug workaround)
        tex = tex_val;
        alpha = alpha_val;

        vec4 world_pos = vertex_pos + offsets[i];
        gl_Position = push.pv * world_pos;
        uv = uvs[i];
        EmitVertex();
    }

    EndPrimitive();
}
