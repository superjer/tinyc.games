// the model editor's palette panel: 2D pixel-space quads with per-vertex
// color, so the swatches and the hue-lightness picker gradient all land in
// one draw
#version 450
layout(location = 0) in vec2 pos;
layout(location = 1) in vec4 incolor;

layout(location = 0) out vec4 vcolor;

layout(push_constant) uniform PushConstants {
    mat4 proj;
} pc;

void main() {
    gl_Position = pc.proj * vec4(pos, 0.0, 1.0);
    vcolor = incolor;
}
