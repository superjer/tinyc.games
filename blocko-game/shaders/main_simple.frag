#version 450

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec4 world_pos;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;           // offset 0
    mat4 view;            // offset 64
    mat4 proj;            // offset 128
    mat4 shadow_space;    // offset 192
    mat4 shadow2_space;   // offset 256
    float BS;             // offset 320

    vec3 day_color;       // offset 336
    vec3 glo_color;       // offset 352
    vec3 fog_color;       // offset 368
    float fog_lo;         // offset 380
    float fog_hi;         // offset 384
    vec3 light_pos;       // offset 400
    vec3 view_pos;        // offset 416
    float sharpness;      // offset 428
    bool shadow_mapping;  // offset 432
} ubo;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;

void main(void) {
    vec4 texel = texture(tarray, vec3(uv, tex));
    if (texel.a < 0.5) discard;

    vec3 sky = vec3(illum) * ubo.day_color;
    vec3 glo_contrib = vec3(glow) * ubo.glo_color;
    vec3 combined = sky + glo_contrib * (vec3(1.0) - sky);
    vec4 c = texel * vec4(combined, alpha);

    // Fog based on distance from camera
    float dist = length(ubo.view_pos.xz - world_pos.xz);
    float fog_linear = smoothstep(ubo.fog_lo, ubo.fog_hi, dist);
    float fog = fog_linear * fog_linear * fog_linear;

    color = mix(c, vec4(ubo.fog_color, 1.0), fog);
}
