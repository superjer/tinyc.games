#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 uv_out;

layout(push_constant) uniform Push {
    mat4 pvm;
    float night_amt;
} push;

void main()
{
    gl_Position = push.pvm * vec4(pos, 1.0);
    uv_out = uv;
}
