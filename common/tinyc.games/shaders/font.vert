#version 450

layout(location = 0) in vec4 pos;  // xy = position, zw = uv

layout(location = 0) out vec2 uv;

layout(push_constant) uniform Push {
    mat4 proj;
    vec3 color;
} push;

void main() {
    gl_Position = push.proj * vec4(pos.xy, 0.0, 1.0);
    uv = pos.zw;
}
