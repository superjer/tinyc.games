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
    mat4 shadow_space[6];  // offset 192 (near, mid, far_a, far_b, ext_a, ext_b)
    float BS;              // offset 576
    float shadow_far_blend;  // offset 580 (far: 0=A, 1=B)
    float shadow_ext_blend;  // offset 584 (extreme: 0=A, 1=B)

    vec3 day_color;        // offset 592
    vec3 glo_color;        // offset 608
    vec3 fog_color;        // offset 624
    float fog_lo;          // offset 636
    float fog_hi;          // offset 640
    vec3 light_pos;        // offset 656
    vec3 view_pos;         // offset 672
    float sharpness;       // offset 684
    bool shadow_mapping;   // offset 688
    float sun_strength;    // offset 692
    float sun_warmth;      // offset 696
    float outside_cascade_lit; // offset 700
} ubo;

layout(set = 0, binding = 1) uniform sampler2DArray tarray;
// Shadow maps at bindings 2-7 (must match descriptor layout in glsetup.c)
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_near;
layout(set = 0, binding = 3) uniform sampler2DShadow shadow_mid;
layout(set = 0, binding = 4) uniform sampler2DShadow shadow_far_a;
layout(set = 0, binding = 5) uniform sampler2DShadow shadow_far_b;
layout(set = 0, binding = 6) uniform sampler2DShadow shadow_ext_a;
layout(set = 0, binding = 7) uniform sampler2DShadow shadow_ext_b;

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

        // Compute shadow positions for all cascades
        // Apply normal offset bias to prevent shadow bleeding at cube edges
        // shadow_space indices: 0=near, 1=mid, 2=far_a, 3=far_b, 4=ext_a, 5=ext_b
        vec4 shadow_sample_pos = world_pos + vec4(normal * 75.0, 0.0);
        vec4 shadow_pos_mid = ubo.shadow_space[1] * shadow_sample_pos;
        vec4 shadow_pos_far_a = ubo.shadow_space[2] * shadow_sample_pos;
        vec4 shadow_pos_far_b = ubo.shadow_space[3] * shadow_sample_pos;
        vec4 shadow_pos_ext_a = ubo.shadow_space[4] * shadow_sample_pos;
        vec4 shadow_pos_ext_b = ubo.shadow_space[5] * shadow_sample_pos;

        float unshadow;
        if (shadow_pos.x > 0.007 && shadow_pos.x < 0.993 && shadow_pos.y > 0.007 && shadow_pos.y < 0.993) {
            // Near cascade - soft shadows with PCF
            unshadow = sampleShadowPCF(shadow_near, shadow_pos.xyz, 0.006, rotation);
        } else if (shadow_pos_mid.x > 0.002 && shadow_pos_mid.x < 0.998 && shadow_pos_mid.y > 0.002 && shadow_pos_mid.y < 0.998) {
            // Mid cascade - PCF, no A/B blending
            unshadow = sampleShadowPCF(shadow_mid, shadow_pos_mid.xyz, 0.0015, rotation);
        } else if (shadow_pos_far_a.x > 0.001 && shadow_pos_far_a.x < 0.999 && shadow_pos_far_a.y > 0.001 && shadow_pos_far_a.y < 0.999) {
            // Far cascade - blend between A and B shadow maps
            float shad_a = textureProj(shadow_far_a, vec4(shadow_pos_far_a.xy, shadow_pos_far_a.z, 1.0));
            float shad_b = textureProj(shadow_far_b, vec4(shadow_pos_far_b.xy, shadow_pos_far_b.z, 1.0));
            unshadow = mix(shad_a, shad_b, ubo.shadow_far_blend);
        } else {
            // Extreme cascade - blend between A and B shadow maps
            float shad_a = textureProj(shadow_ext_a, vec4(shadow_pos_ext_a.xy, shadow_pos_ext_a.z, 1.0));
            float shad_b = textureProj(shadow_ext_b, vec4(shadow_pos_ext_b.xy, shadow_pos_ext_b.z, 1.0));
            unshadow = mix(shad_a, shad_b, ubo.shadow_ext_blend);

            // Fade out shadow at edges of extreme cascade - only light up edges during day
            // At night, areas outside shadow cascades should remain dark
            float edge_lit = ubo.outside_cascade_lit;
            if (shadow_pos_ext_a.x >= 0.0 && shadow_pos_ext_a.x <= 0.1) { unshadow = max(unshadow, edge_lit * (1.0 - (shadow_pos_ext_a.x * 10.0))); }
            if (shadow_pos_ext_a.x >= 0.9 && shadow_pos_ext_a.x <= 1.0) { unshadow = max(unshadow, edge_lit * ((shadow_pos_ext_a.x - 0.9) * 10.0)); }
            if (shadow_pos_ext_a.y >= 0.0 && shadow_pos_ext_a.y <= 0.1) { unshadow = max(unshadow, edge_lit * (1.0 - (shadow_pos_ext_a.y * 10.0))); }
            if (shadow_pos_ext_a.y >= 0.9 && shadow_pos_ext_a.y <= 1.0) { unshadow = max(unshadow, edge_lit * ((shadow_pos_ext_a.y - 0.9) * 10.0)); }
        }

        // Combine shadow with lighting
        float s0 = 0.6 + 0.4 * ubo.sharpness;
        float s1 = 0.3 + 0.7 * (1 - ubo.sharpness);

        // Sunlight color: white at noon, warm orange at sunrise/sunset
        // Moonlight stays unwarmed (sun_warmth = 0 at night)
        vec3 warm_orange = vec3(1.0, 0.6, 0.3);
        vec3 light_tint = mix(vec3(1.0), warm_orange, ubo.sun_warmth);

        // Directional lighting modulated by sun strength (0 at sunrise/sunset, 1 at noon)
        // At night, sun_strength = 1.0 to allow moonlight directional contribution
        float directional = unshadow * (diff + spec) * ubo.sun_strength;

        sky = (s1 * illum + s0 * directional) * light_tint * ubo.day_color;
    } else {
        // No shadow mapping - just use ambient with day_color (no directional to warm/dim)
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
