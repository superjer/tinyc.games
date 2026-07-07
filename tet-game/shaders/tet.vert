#version 450
layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 color2;

layout(push_constant) uniform Push {
    mat4 proj;
} push;

void main()
{
    gl_Position = push.proj * vec4(pos, 0.0, 1.0);
    color2 = color;
}
