#version 330 core
in vec2 uv_v;

out vec4 color;

uniform sampler2D tex;

void main()
{
    //color = vec4(1);
    color = vec4(vec3(1 - texture(tex, uv_v).r), 1);
}
