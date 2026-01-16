#version 450

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;

void main(void) {
    float brightness = max(illum, glow);
    color = vec4(vec3(brightness), alpha);
}
