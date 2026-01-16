#version 450

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;

void main(void) {
    vec4 texel = texture(tarray, vec3(uv, tex));
    if (texel.a < 0.5) discard;
    float brightness = max(illum, glow);
    color = texel * vec4(vec3(brightness), alpha);
}
