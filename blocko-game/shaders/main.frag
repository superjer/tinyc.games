#version 450

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec4 world_pos;
layout(location = 6) in vec4 shadow_pos;
layout(location = 8) flat in vec3 normal;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (near cascade)
    mat4 shadow2a_space;   // offset 256 (mid cascade A)
    mat4 shadow2b_space;   // offset 320 (mid cascade B)
    mat4 shadow3a_space;   // offset 384 (far cascade A)
    mat4 shadow3b_space;   // offset 448 (far cascade B)
    float BS;              // offset 512
    float shadow2_blend;   // offset 516 (0=A, 1=B)
    float shadow3_blend;   // offset 520

    vec3 day_color;        // offset 528
    vec3 glo_color;        // offset 544
    vec3 fog_color;        // offset 560
    float fog_lo;          // offset 572
    float fog_hi;          // offset 576
    vec3 light_pos;        // offset 592
    vec3 view_pos;         // offset 608
    float sharpness;       // offset 620
    bool shadow_mapping;   // offset 624
} ubo;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_map;
layout(set = 0, binding = 3) uniform sampler2DShadow shadow2a_map;
layout(set = 0, binding = 4) uniform sampler2DShadow shadow2b_map;
layout(set = 0, binding = 5) uniform sampler2DShadow shadow3a_map;
layout(set = 0, binding = 6) uniform sampler2DShadow shadow3b_map;

// Poisson disk samples for soft shadows (16 samples, well-distributed)
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// PCF shadow sampling with Poisson disk
float sampleShadowPCF(sampler2DShadow shadowMap, vec3 shadowCoord, float radius, float rotation) {
    float shadow = 0.0;
    float c = cos(rotation);
    float s = sin(rotation);

    for (int i = 0; i < 16; i++) {
        vec2 offset = vec2(
            poissonDisk[i].x * c - poissonDisk[i].y * s,
            poissonDisk[i].x * s + poissonDisk[i].y * c
        ) * radius;
        shadow += textureProj(shadowMap, vec4(shadowCoord.xy + offset, shadowCoord.z, 1.0));
    }
    return shadow / 16.0;
}

void main(void) {
    vec4 texel = texture(tarray, vec3(uv, tex));
    if (texel.a < 0.5) discard;

    vec3 sky;
    if (ubo.shadow_mapping) {
        // Diffuse lighting
        vec3 light_dir = normalize(ubo.light_pos - world_pos.xyz);
        float diff = max(dot(light_dir, normal), 0.0);

        // Specular lighting
        vec3 view_dir = normalize(ubo.view_pos - world_pos.xyz);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0), 16);

        // Shadow sampling with Poisson disk PCF
        // Screen-space random rotation - no spatial coherence, should produce noise instead of moire
        float rotation = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.28318;

        // Compute shadow positions for mid/far cascades (A and B)
        // Apply normal offset bias to prevent shadow bleeding at cube edges
        vec4 shadow_sample_pos = world_pos + vec4(normal * 50.0, 0.0);
        vec4 shadow2a_pos = ubo.shadow2a_space * shadow_sample_pos;
        vec4 shadow2b_pos = ubo.shadow2b_space * shadow_sample_pos;
        vec4 shadow3a_pos = ubo.shadow3a_space * shadow_sample_pos;
        vec4 shadow3b_pos = ubo.shadow3b_space * shadow_sample_pos;

        float unshadow;
        if (shadow_pos.x > 0.001 && shadow_pos.x < 0.999 && shadow_pos.y > 0.001 && shadow_pos.y < 0.999) {
            // Near cascade - softest shadows, single shadow map
            unshadow = sampleShadowPCF(shadow_map, shadow_pos.xyz, 0.0015, rotation);
        } else if (shadow2a_pos.x > 0.001 && shadow2a_pos.x < 0.999 && shadow2a_pos.y > 0.001 && shadow2a_pos.y < 0.999) {
            // Mid cascade - blend between A and B shadow maps
            float shadow_a = textureProj(shadow2a_map, vec4(shadow2a_pos.xy, shadow2a_pos.z, 1.0));
            float shadow_b = textureProj(shadow2b_map, vec4(shadow2b_pos.xy, shadow2b_pos.z, 1.0));
            unshadow = mix(shadow_a, shadow_b, ubo.shadow2_blend);
        } else {
            // Far cascade - blend between A and B shadow maps
            float shadow_a = textureProj(shadow3a_map, vec4(shadow3a_pos.xy, shadow3a_pos.z, 1.0));
            float shadow_b = textureProj(shadow3b_map, vec4(shadow3b_pos.xy, shadow3b_pos.z, 1.0));
            unshadow = mix(shadow_a, shadow_b, ubo.shadow3_blend);

            // Fade out shadow at edges of far cascade
            if (shadow3a_pos.x >= 0.0 && shadow3a_pos.x <= 0.1) { unshadow = max(unshadow, 1.0 - (shadow3a_pos.x * 10.0)); }
            if (shadow3a_pos.x >= 0.9 && shadow3a_pos.x <= 1.0) { unshadow = max(unshadow, (shadow3a_pos.x - 0.9) * 10.0); }
            if (shadow3a_pos.y >= 0.0 && shadow3a_pos.y <= 0.1) { unshadow = max(unshadow, 1.0 - (shadow3a_pos.y * 10.0)); }
            if (shadow3a_pos.y >= 0.9 && shadow3a_pos.y <= 1.0) { unshadow = max(unshadow, (shadow3a_pos.y - 0.9) * 10.0); }
        }

        // Combine shadow with lighting
        float s0 = 0.6 + 0.4 * ubo.sharpness;
        float s1 = 0.3 + 0.7 * (1 - ubo.sharpness);
        sky = vec3(s1 * illum + s0 * unshadow * (diff + spec)) * ubo.day_color;
    } else {
        sky = vec3(illum) * ubo.day_color;
    }

    vec3 glo_contrib = vec3(glow) * ubo.glo_color;
    vec3 combined = sky + glo_contrib * (vec3(1.0) - sky);
    vec4 c = texel * vec4(combined, alpha);

    // Fog based on distance from camera
    float dist = length(ubo.view_pos.xz - world_pos.xz);
    float fog_linear = smoothstep(ubo.fog_lo, ubo.fog_hi, dist);
    float fog = fog_linear * fog_linear * fog_linear;

    color = mix(c, vec4(ubo.fog_color, 1.0), fog);
}
