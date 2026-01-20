#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in float tex_vs[];
layout(location = 1) in float orient_vs[];
layout(location = 2) in vec4 illum_vs[];
layout(location = 3) in vec4 glow_vs[];
layout(location = 4) in float alpha_vs[];
layout(location = 5) in vec4 world_pos_vs[];

layout(location = 0) flat out float tex;
layout(location = 1) out float illum;
layout(location = 2) out float glow;
layout(location = 3) flat out float alpha;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec4 world_pos_out;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
} push;

void main(void) {
    float sidel = 0.0f;
    vec4 a, b, c, d;

    switch (int(orient_vs[0])) {
        case 1: // UP
            a = vec4(0, 0, 0, 0);
            b = vec4(push.bs, 0, 0, 0);
            c = vec4(0, 0, push.bs, 0);
            d = vec4(push.bs, 0, push.bs, 0);
            sidel = 1.0f;
            break;
        case 2: // EAST
            a = vec4(push.bs, 0, push.bs, 0);
            b = vec4(push.bs, 0, 0, 0);
            c = vec4(push.bs, push.bs, push.bs, 0);
            d = vec4(push.bs, push.bs, 0, 0);
            sidel = 0.9f;
            break;
        case 3: // NORTH
            a = vec4(0, 0, push.bs, 0);
            b = vec4(push.bs, 0, push.bs, 0);
            c = vec4(0, push.bs, push.bs, 0);
            d = vec4(push.bs, push.bs, push.bs, 0);
            sidel = 0.8f;
            break;
        case 4: // WEST
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, push.bs, 0);
            c = vec4(0, push.bs, 0, 0);
            d = vec4(0, push.bs, push.bs, 0);
            sidel = 0.9f;
            break;
        case 5: // SOUTH
            a = vec4(push.bs, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(push.bs, push.bs, 0, 0);
            d = vec4(0, push.bs, 0, 0);
            sidel = 0.8f;
            break;
        case 6: // DOWN
            a = vec4(push.bs, push.bs, 0, 0);
            b = vec4(0, push.bs, 0, 0);
            c = vec4(push.bs, push.bs, push.bs, 0);
            d = vec4(0, push.bs, push.bs, 0);
            sidel = 0.6f;
            break;
    }

    tex = tex_vs[0];
    alpha = alpha_vs[0];

    vec4 vertex_pos = world_pos_vs[0];
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };

    for (int i = 0; i < 4; i++) {
        vec4 world_pos = vertex_pos + offsets[i];
        gl_Position = push.pv * world_pos;
        illum = (0.1 + illum_vs[0][i]) * sidel;
        glow = (0.1 + glow_vs[0][i]) * sidel;
        uv = uvs[i];
        world_pos_out = world_pos;
        EmitVertex();
    }

    EndPrimitive();
}
