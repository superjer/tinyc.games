#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 uv_out;
layout(location = 1) out vec3 world_dir;

layout(push_constant) uniform Push {
    mat4 pvm;
    vec3 sun_dir;
    float night_amt;
} push;

void main()
{
    gl_Position = push.pvm * vec4(pos, 1.0);
    uv_out = uv;
    world_dir = normalize(pos);
}
