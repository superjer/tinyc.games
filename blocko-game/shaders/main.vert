#version 450

layout (location = 0) in float tex_in;
layout (location = 1) in float orient_in;
layout (location = 2) in vec3 pos_in;
layout (location = 3) in vec4 illum_in;
layout (location = 4) in vec4 glow_in;
layout (location = 5) in float alpha_in;

layout (location = 0) out float tex_vs;
layout (location = 1) out float orient_vs;
layout (location = 2) out vec4 illum_vs;
layout (location = 3) out vec4 glow_vs;
layout (location = 4) out float alpha_vs;
layout (location = 5) out vec4 world_pos_vs;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 shadow_space;
    mat4 shadow2_space;
    float BS;
    
    vec3 day_color;
    vec3 glo_color;
    vec3 fog_color;
    float fog_lo;
    float fog_hi;
    vec3 light_pos;
    vec3 view_pos;
    float sharpness;
    bool shadow_mapping;
} ubo;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_map;
layout(set = 0, binding = 3) uniform sampler2DShadow shadow2_map;

void main() {
    vec3 pos = ubo.BS * pos_in;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(pos, 1.0);
    world_pos_vs = ubo.model * vec4(pos, 1.0);
    tex_vs = tex_in;
    orient_vs = orient_in;
    illum_vs = illum_in;
    glow_vs = glow_in;
    alpha_vs = alpha_in;
}
