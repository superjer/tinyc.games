#version 450

// Shadow pass fragment shader - depth-only
// No color output needed; depth is written automatically

layout(location = 0) flat in float tex;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in float alpha;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;

void main(void) {
    // Sample texture and discard transparent pixels (for leaves)
    vec4 texel = texture(tarray, vec3(uv, tex));
    if (texel.a < 0.5) discard;
}
