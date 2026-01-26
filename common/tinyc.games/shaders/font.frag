#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Push {
    mat4 proj;
    vec3 incolor;
} push;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
    float alpha = texture(tex, uv).r;
    color = vec4(push.incolor, alpha);
}
