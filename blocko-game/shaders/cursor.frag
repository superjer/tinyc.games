#version 450
layout(location = 0) out vec3 color;

layout(push_constant) uniform PushConstants {
    mat4 proj;
    vec3 incolor;
} pc;

void main() {
    color = pc.incolor;
}
