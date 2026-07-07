#version 450
layout(location = 0) in vec3 color2;

layout(location = 0) out vec4 color;

void main()
{
    color = vec4(color2, 1.0);
}
