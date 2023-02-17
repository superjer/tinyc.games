#version 330 core
layout (location = 0) in float tex_in;
layout (location = 1) in float orient_in;
layout (location = 2) in vec3 pos_in;
layout (location = 3) in vec4 illum_in;
layout (location = 4) in vec4 glow_in;
layout (location = 5) in float alpha_in;

out float tex_vs;
out float orient_vs;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform float BS;

void main(void)
{
    vec3 pos = BS * pos_in;
    gl_Position = proj * view * model * vec4(pos, 1.0f);
    tex_vs = tex_in;
    orient_vs = orient_in;
}
