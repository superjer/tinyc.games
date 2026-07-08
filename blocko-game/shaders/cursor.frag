#version 450
layout(location = 0) out vec4 color;

layout(push_constant) uniform PushConstants {
    mat4 proj;
    vec3 incolor;
} pc;

void main() {
    color = vec4(pc.incolor, 1.0);
}
