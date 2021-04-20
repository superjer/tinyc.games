#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in float tex_vs[];
in float orient_vs[];

flat out float tex;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform float BS;

void main(void) // geometry
{
    vec4 a, b, c, d;
    mat4 mvp = proj * view * model;
    switch(int(orient_vs[0])) {
        case 1: // UP
            b = vec4(BS, 0, 0,0);
            a = vec4( 0, 0, 0,0);
            c = vec4( 0, 0,BS,0);
            d = vec4(BS, 0,BS,0);
            break;
        case 2: // EAST
            a = vec4(BS, 0,BS,0);
            b = vec4(BS, 0, 0,0);
            c = vec4(BS,BS,BS,0);
            d = vec4(BS,BS, 0,0);
            break;
        case 3: // NORTH
            a = vec4( 0, 0,BS,0);
            b = vec4(BS, 0,BS,0);
            c = vec4( 0,BS,BS,0);
            d = vec4(BS,BS,BS,0);
            break;
        case 4: // WEST
            a = vec4( 0, 0, 0,0);
            b = vec4( 0, 0,BS,0);
            c = vec4( 0,BS, 0,0);
            d = vec4( 0,BS,BS,0);
            break;
        case 5: // SOUTH
            a = vec4(BS, 0, 0,0);
            b = vec4( 0, 0, 0,0);
            c = vec4(BS,BS, 0,0);
            d = vec4( 0,BS, 0,0);
            break;
        case 6: // DOWN
            a = vec4(BS,BS, 0,0);
            b = vec4( 0,BS, 0,0);
            c = vec4(BS,BS,BS,0);
            d = vec4( 0,BS,BS,0);
            break;
    }

    tex = tex_vs[0];

    gl_Position = gl_in[0].gl_Position + mvp * a;
    uv = vec2(1,0);
    EmitVertex();
    gl_Position = gl_in[0].gl_Position + mvp * b;
    uv = vec2(0,0);
    EmitVertex();
    gl_Position = gl_in[0].gl_Position + mvp * c;
    uv = vec2(1,1);
    EmitVertex();
    gl_Position = gl_in[0].gl_Position + mvp * d;
    uv = vec2(0,1);
    EmitVertex();
    EndPrimitive();
}
