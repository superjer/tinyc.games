#version 330 core
layout (location = 0) in float tex_in;
layout (location = 1) in float orient_in;
layout (location = 2) in vec3 pos_in;
layout (location = 3) in vec4 illum_in;
layout (location = 4) in vec4 glow_in;
layout (location = 5) in float alpha_in;

out float tex_vs;
out float orient_vs;
out vec4 illum_vs;
out vec4 glow_vs;
out float alpha_vs;
out vec4 world_pos_vs;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform float BS;

void main(void)
{
    vec3 pos = BS * pos_in;
    gl_Position = proj * view * model * vec4(pos, 1);
    world_pos_vs = model * vec4(pos, 1);
    tex_vs = tex_in;
    orient_vs = orient_in;
    illum_vs = illum_in;
    glow_vs = glow_in;
    alpha_vs = alpha_in;
}
