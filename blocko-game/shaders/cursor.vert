#version 450
layout(location = 0) in vec2 pos;

layout(push_constant) uniform PushConstants {
    mat4 proj;
    vec3 incolor;
} pc;

void main() {
    gl_Position = pc.proj * vec4(pos, 0.0, 1.0);
}
