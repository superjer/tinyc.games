#version 450

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec4 world_pos;
layout(location = 6) in vec4 shadow_pos;
layout(location = 7) in vec4 shadow2_pos;
layout(location = 8) flat in vec3 normal;

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
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_map;
layout(set = 0, binding = 3) uniform sampler2DShadow shadow2_map;

// Random jitter for soft shadow edges
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453) * 0.0004 - 0.0002;
}

float rand2(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453) * 0.00004 - 0.00002;
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

        // Shadow sampling with PCF
        float unshadow;
        if (shadow_pos.x > 0.001 && shadow_pos.x < 0.999 && shadow_pos.y > 0.001 && shadow_pos.y < 0.999) {
            // Near cascade
            vec2 sp0 = shadow_pos.xy + vec2(-0.0005, -0.0005);
            vec2 sp1 = shadow_pos.xy + vec2(-0.0005,  0.0000);
            vec2 sp2 = shadow_pos.xy + vec2(-0.0005, +0.0005);
            vec2 sp3 = shadow_pos.xy + vec2( 0.0000, -0.0005);
            vec2 sp4 = shadow_pos.xy + vec2( 0.0000, +0.0005);
            vec2 sp5 = shadow_pos.xy + vec2(+0.0005, -0.0005);
            vec2 sp6 = shadow_pos.xy + vec2(+0.0005,  0.0000);
            vec2 sp7 = shadow_pos.xy + vec2(+0.0005, +0.0005);
            unshadow = textureProj(shadow_map, vec4(shadow_pos.xyz, 1))
                + textureProj(shadow_map, vec4(sp0.x + rand(gl_FragCoord.xy), sp0.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp1.x + rand(gl_FragCoord.xy), sp1.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp2.x + rand(gl_FragCoord.xy), sp2.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp3.x + rand(gl_FragCoord.xy), sp3.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp4.x + rand(gl_FragCoord.xy), sp4.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp5.x + rand(gl_FragCoord.xy), sp5.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp6.x + rand(gl_FragCoord.xy), sp6.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                + textureProj(shadow_map, vec4(sp7.x + rand(gl_FragCoord.xy), sp7.y + rand(gl_FragCoord.xy), shadow_pos.z, 1));
            unshadow /= 9.0;
        } else {
            // Far cascade
            vec2 sp0 = shadow2_pos.xy + vec2(-0.00005, -0.00005);
            vec2 sp1 = shadow2_pos.xy + vec2(-0.00005,  0.00000);
            vec2 sp2 = shadow2_pos.xy + vec2(-0.00005, +0.00005);
            vec2 sp3 = shadow2_pos.xy + vec2( 0.00000, -0.00005);
            vec2 sp4 = shadow2_pos.xy + vec2( 0.00000, +0.00005);
            vec2 sp5 = shadow2_pos.xy + vec2(+0.00005, -0.00005);
            vec2 sp6 = shadow2_pos.xy + vec2(+0.00005,  0.00000);
            vec2 sp7 = shadow2_pos.xy + vec2(+0.00005, +0.00005);
            unshadow = textureProj(shadow2_map, vec4(shadow2_pos.xyz, 1))
                + textureProj(shadow2_map, vec4(sp0.x + rand2(gl_FragCoord.xy), sp0.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp1.x + rand2(gl_FragCoord.xy), sp1.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp2.x + rand2(gl_FragCoord.xy), sp2.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp3.x + rand2(gl_FragCoord.xy), sp3.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp4.x + rand2(gl_FragCoord.xy), sp4.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp5.x + rand2(gl_FragCoord.xy), sp5.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp6.x + rand2(gl_FragCoord.xy), sp6.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1))
                + textureProj(shadow2_map, vec4(sp7.x + rand2(gl_FragCoord.xy), sp7.y + rand2(gl_FragCoord.xy), shadow2_pos.z, 1));
            unshadow /= 9.0;
        }

        // Fade out shadow at edges of far cascade
        if (shadow2_pos.x >= 0.0 && shadow2_pos.x <= 0.1) { unshadow = max(unshadow, 1.0 - (shadow2_pos.x * 10.0)); }
        if (shadow2_pos.x >= 0.9 && shadow2_pos.x <= 1.0) { unshadow = max(unshadow, (shadow2_pos.x - 0.9) * 10.0); }
        if (shadow2_pos.y >= 0.0 && shadow2_pos.y <= 0.1) { unshadow = max(unshadow, 1.0 - (shadow2_pos.y * 10.0)); }
        if (shadow2_pos.y >= 0.9 && shadow2_pos.y <= 1.0) { unshadow = max(unshadow, (shadow2_pos.y - 0.9) * 10.0); }

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
