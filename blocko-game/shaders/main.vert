#version 450

layout(location = 0) in float tex_in;
layout(location = 1) in float orient_in;
layout(location = 2) in vec3 pos_in;
layout(location = 3) in vec4 illum_in;
layout(location = 4) in vec4 glow_in;
layout(location = 5) in float alpha_in;

layout(location = 0) out float tex_vs;
layout(location = 1) out float orient_vs;
layout(location = 2) out vec4 illum_vs;
layout(location = 3) out vec4 glow_vs;
layout(location = 4) out float alpha_vs;
layout(location = 5) out vec4 world_pos_vs;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
} push;

void main() {
    vec3 world = push.bs * pos_in + vec3(push.chunk_x, push.chunk_y, push.chunk_z);
    gl_Position = push.pv * vec4(world, 1.0);
    world_pos_vs = vec4(world, 1.0);
    tex_vs = tex_in;
    orient_vs = orient_in;
    illum_vs = illum_in;
    glow_vs = glow_in;
    alpha_vs = alpha_in;
}
