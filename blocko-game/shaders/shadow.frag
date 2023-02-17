#version 330 core
flat in float tex;
in vec2 uv;

out vec4 color;

uniform sampler2DArray tarray;

void main(void)
{
    color = texture(tarray, vec3(uv, tex));
    if (color.a < 0.51) discard;
}
