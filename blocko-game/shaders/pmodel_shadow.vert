#version 450

// Player model shadow pass: same instanced prism faces as pmodel.vert.
// Depth-only; pairs with shadow.frag (alpha-tests the face tile).

layout(location = 0) in vec4 r0_in;   // piece transform rows: world = M * (px,1)
layout(location = 1) in vec4 r1_in;
layout(location = 2) in vec4 r2_in;
layout(location = 3) in vec3 dims_in; // prism size in px
layout(location = 4) in vec4 misc_in; // orient, tex layer, illum, glow

layout(location = 0) flat out float tex;
layout(location = 1) out vec2 uv;
layout(location = 2) flat out float alpha;

layout(push_constant) uniform Push {
    mat4 pv;
} push;

void main(void) {
    vec3 a, b, c, d;
    vec3 s = dims_in;
    vec2 extent;

    switch (int(misc_in.x)) {
        case 1: // UP
            a = vec3(0, 0, 0);
            b = vec3(s.x, 0, 0);
            c = vec3(0, 0, s.z);
            d = vec3(s.x, 0, s.z);
            extent = s.xz;
            break;
        case 2: // EAST
            a = vec3(s.x, 0, s.z);
            b = vec3(s.x, 0, 0);
            c = vec3(s.x, s.y, s.z);
            d = vec3(s.x, s.y, 0);
            extent = s.zy;
            break;
        case 3: // NORTH
            a = vec3(0, 0, s.z);
            b = vec3(s.x, 0, s.z);
            c = vec3(0, s.y, s.z);
            d = vec3(s.x, s.y, s.z);
            extent = s.xy;
            break;
        case 4: // WEST
            a = vec3(0, 0, 0);
            b = vec3(0, 0, s.z);
            c = vec3(0, s.y, 0);
            d = vec3(0, s.y, s.z);
            extent = s.zy;
            break;
        case 5: // SOUTH
            a = vec3(s.x, 0, 0);
            b = vec3(0, 0, 0);
            c = vec3(s.x, s.y, 0);
            d = vec3(0, s.y, 0);
            extent = s.xy;
            break;
        case 6: // DOWN
            a = vec3(s.x, s.y, 0);
            b = vec3(0, s.y, 0);
            c = vec3(s.x, s.y, s.z);
            d = vec3(0, s.y, s.z);
            extent = s.xz;
            break;
    }

    vec3 corners[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    vec4 px = vec4(corners[i], 1.0);
    vec4 world_pos = vec4(dot(r0_in, px), dot(r1_in, px), dot(r2_in, px), 1.0);

    gl_Position = push.pv * world_pos;
    tex = misc_in.y;
    alpha = 1.0;
    uv = uvs[i] * extent / 16.0;
}
