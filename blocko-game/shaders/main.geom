#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in float tex_vs[];
in float orient_vs[];
in vec4 illum_vs[];
in vec4 glow_vs[];
in float alpha_vs[];
in vec4 world_pos_vs[];

flat out float tex;
out float illum;
out float glow;
flat out float alpha;
out vec2 uv;
flat out float eyedist;
out vec4 shadow_pos;
out vec4 world_pos;
flat out vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 shadow_space;
uniform float BS;

void main(void) // geometry
{
    float sidel = 0.0f;
    vec4 a, b, c, d;
    mat4 mvp = proj * view * model;
    switch(int(orient_vs[0])) {
        case 1: // UP
            a = vec4( 0, 0, 0,0);
            b = vec4(BS, 0, 0,0);
            c = vec4( 0, 0,BS,0);
            d = vec4(BS, 0,BS,0);
            normal = vec3(0, -1, 0);
            sidel = 1.0f;
            break;
        case 2: // EAST
            a = vec4(BS, 0,BS,0);
            b = vec4(BS, 0, 0,0);
            c = vec4(BS,BS,BS,0);
            d = vec4(BS,BS, 0,0);
            normal = vec3(1, 0, 0);
            sidel = 0.9f;
            break;
        case 3: // NORTH
            a = vec4( 0, 0,BS,0);
            b = vec4(BS, 0,BS,0);
            c = vec4( 0,BS,BS,0);
            d = vec4(BS,BS,BS,0);
            normal = vec3(0, 0, 1);
            sidel = 0.8f;
            break;
        case 4: // WEST
            a = vec4( 0, 0, 0,0);
            b = vec4( 0, 0,BS,0);
            c = vec4( 0,BS, 0,0);
            d = vec4( 0,BS,BS,0);
            normal = vec3(-1, 0, 0);
            sidel = 0.9f;
            break;
        case 5: // SOUTH
            a = vec4(BS, 0, 0,0);
            b = vec4( 0, 0, 0,0);
            c = vec4(BS,BS, 0,0);
            d = vec4( 0,BS, 0,0);
            normal = vec3(0, 0, -1);
            sidel = 0.8f;
            break;
        case 6: // DOWN
            a = vec4(BS,BS, 0,0);
            b = vec4( 0,BS, 0,0);
            c = vec4(BS,BS,BS,0);
            d = vec4( 0,BS,BS,0);
            normal = vec3(0, 1, 0);
            sidel = 0.6f;
            break;
    }

    tex = tex_vs[0];
    alpha = alpha_vs[0];
    eyedist = length(gl_in[0].gl_Position);

    gl_Position = gl_in[0].gl_Position + mvp * a;
    world_pos = world_pos_vs[0] + a;
    shadow_pos = shadow_space * world_pos;
    uv = vec2(1,0);
    illum = (0.1 + illum_vs[0].x) * sidel;
    glow = (0.1 + glow_vs[0].x) * sidel;
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + mvp * b;
    world_pos = world_pos_vs[0] + b;
    shadow_pos = shadow_space * world_pos;
    uv = vec2(0,0);
    illum = (0.1 + illum_vs[0].y) * sidel;
    glow = (0.1 + glow_vs[0].y) * sidel;
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + mvp * c;
    world_pos = world_pos_vs[0] + c;
    shadow_pos = shadow_space * world_pos;
    uv = vec2(1,1);
    illum = (0.1 + illum_vs[0].z) * sidel;
    glow = (0.1 + glow_vs[0].z) * sidel;
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + mvp * d;
    world_pos = world_pos_vs[0] + d;
    shadow_pos = shadow_space * world_pos;
    uv = vec2(0,1);
    illum = (0.1 + illum_vs[0].w) * sidel;
    glow = (0.1 + glow_vs[0].w) * sidel;
    EmitVertex();

    EndPrimitive();
}
