#version 450

// Shadow pass fragment shader - depth-only
// No color output needed; depth is written automatically

layout(location = 0) flat in float tex;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in float alpha;

void main(void) {
    // Alpha test for transparent blocks (leaves)
    // If alpha is less than 1, discard (only fully opaque blocks cast shadows)
    if (alpha < 0.99) discard;
}
