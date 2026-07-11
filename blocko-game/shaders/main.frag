#version 450

// The whole sky is one flat color: it's the render clear color (set in
// blocko.c), the fog color, and what water mirrors. Keep the three in sync.
const vec3 SKY_COLOR = vec3(0.40, 0.62, 0.95);

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec4 world_pos;
layout(location = 6) in vec4 shadow_pos;
layout(location = 8) flat in vec3 normal;
layout(location = 9) flat in float shiny;  // >0 = wet/glossy surface (slimes); set by the vertex shader

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (the one near cascade)
    float BS;              // offset 256
    float fog_lo;          // offset 260
    float fog_hi;          // offset 264
    vec3 light_pos;        // offset 272
    vec3 view_pos;         // offset 288
    bool shadow_mapping;   // offset 300
    int water_frame;       // offset 304
    float underwater;      // offset 308 (camera eye is in water)
    float scootx;          // offset 312 (window->world block offset)
    float scootz;          // offset 316
} ubo;

// No push constants here: this fragment shader is shared by the terrain
// (main.vert), water, and mob (mob.vert) pipelines, which have different push
// layouts. The per-draw scalars it needs (shiny) arrive as flat varyings.

layout(set = 0, binding = 1) uniform sampler2DArray tarray;
// Shadow map at binding 2 (must match descriptor layout in vksetup.c)
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_near;

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
    float final_tex = tex;
    if (alpha < 1.0) {
        // uniform frame for all water: per-block offsets read as a checkerboard
        final_tex = 7.0 + float((ubo.water_frame / 12) % 4);
    }
    vec2 final_uv = uv;
    if (ubo.underwater > 0.5) {
        // gentle ripple over everything while the camera is underwater
        float t = float(ubo.water_frame) * 0.06;
        final_uv += 0.01 * vec2(sin(t       + world_pos.y * 0.007 + world_pos.x * 0.004),
                                sin(t * 1.3 + world_pos.x * 0.007 + world_pos.z * 0.004));
    }
    vec4 texel = texture(tarray, vec3(final_uv, final_tex));
    if (texel.a < 0.5) discard;

    // water gets an animated swell normal for lighting
    vec3 N = normal;
    float swell_shade = 0.0;  // view-independent brightness roll from the swell
    if (alpha < 1.0) {
        float t = float(ubo.water_frame) * 0.03;
        // absolute world coords (window mesh slides on scoot): subtract scoot so
        // the wave phase stays pinned to the world and doesn't snap at a scoot
        vec2 p = world_pos.xz / ubo.BS - vec2(ubo.scootx, ubo.scootz);

        // two long swells at odd angles and incommensurate frequencies so
        // their sum never lines up into a repeat. Wavelengths (~20-30 blocks)
        // stay many pixels wide even at range, so no moire at distance.
        const vec2 D1 = vec2( 0.71,  0.70), D2 = vec2(-0.34,  0.94);
        float a1 = dot(p, D1) * 0.19 + t * 0.6;
        float a2 = dot(p, D2) * 0.28 - t * 0.5;
        vec2 slope = 0.060 * D1 * cos(a1) + 0.048 * D2 * cos(a2);
        N = normalize(N + vec3(slope.x, 0.0, slope.y));

        // Height of that same swell (sin is the integral of the cos slope
        // above), used as a view-INDEPENDENT brightness roll. The reflection
        // detail vanishes looking straight down (fresnel -> 0), so this term
        // keeps the crests bright and troughs dark from directly overhead.
        swell_shade = 0.5 * sin(a1) + 0.5 * sin(a2);
    }

    vec3 glint = vec3(0.0);
    vec3 sky;
    {
        // Diffuse lighting
        vec3 light_dir = normalize(ubo.light_pos - world_pos.xyz);
        float diff = max(dot(light_dir, N), 0.0);

        // Specular lighting
        vec3 view_dir = normalize(ubo.view_pos - world_pos.xyz);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = alpha < 1.0 ? 0.0 : pow(max(dot(N, halfway_dir), 0), 16);

        // Shadow sampling with Poisson disk PCF
        // Per-pixel rotation of the Poisson disk turns PCF banding into dither.
        // Interleaved Gradient Noise (Jimenez 2014) instead of a sin() white-noise
        // hash: same cost, but its grain is spatially organized so the eye reads
        // the penumbra as much smoother/less pixely at the same 16 samples.
        float ign = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
        float rotation = ign * 6.28318;

        // Toggling shadow mapping off skips the map and leaves unshadow at
        // 1.0 - as if nothing casts a shadow - so the lighting model is otherwise
        // identical, instead of switching to a separate ambient-only path.
        float unshadow = 1.0;
        if (ubo.shadow_mapping) {
            // The one cascade: soft shadows with PCF, fading to fully lit over
            // the outer 0.1 of the map so the bubble has no hard edge. The fade
            // SATURATES past the [0,1] boundary: beyond the cascade the raw
            // sample is meaningless (z runs past the far plane -> spurious full
            // shadow), so the clamp forces lit there instead of a dark band.
            // PCF radius is a fraction of the map: 0.00075 of the +-40 block
            // volume = 60 world units, safely inside the 75-unit normal-offset
            // bias in main.vert (offset must exceed the PCF world radius)
            unshadow = sampleShadowPCF(shadow_near, shadow_pos.xyz, 0.00075, rotation);
            vec2 e = shadow_pos.xy;
            float fx = max((0.1 - e.x) * 10.0, (e.x - 0.9) * 10.0);
            float fy = max((0.1 - e.y) * 10.0, (e.y - 0.9) * 10.0);
            float edge = clamp(max(fx, fy), 0.0, 1.0);
            unshadow = max(unshadow, edge);
        }

        // Directional lighting on top of the baked block light
        float directional = unshadow * (diff + spec);

        // sharp sun glitter on the rippled water surface,
        // added after texturing so it reads as a reflection
        if (alpha < 1.0)
            glint = vec3(1.5 * unshadow * pow(max(dot(N, halfway_dir), 0.0), 80.0));

        // slimes read as wet: a tight glossy hotspot plus a fresnel sheen
        if (shiny > 0.5) {
            float wet = pow(max(dot(N, halfway_dir), 0.0), 64.0);
            float fres = pow(1.0 - max(dot(view_dir, N), 0.0), 3.0);
            glint += vec3(2.0 * unshadow * wet)
                   + vec3(0.18, 0.32, 0.24) * fres;
        }

        sky = vec3(illum + 0.6 * directional);
    }

    vec4 c = texel * vec4(sky, alpha);

    if (shiny > 0.5) c.rgb += glint;

    if (alpha < 1.0 && ubo.underwater < 0.5) {
        // fresnel: mirror the sky at grazing angles, clear straight down
        vec3 vdir = normalize(ubo.view_pos - world_pos.xyz);
        float fresnel = pow(1.0 - abs(dot(vdir, N)), 3.0);
        vec3 mirrored = SKY_COLOR;
        c.rgb = mix(c.rgb, mirrored, fresnel);
        c.a = mix(0.5, 0.95, fresnel);

        // View-independent swell shading, strongest where the reflection is
        // weakest (looking straight down), so top-down water rolls light/dark
        // instead of reading as one solid blue.
        c.rgb *= 1.0 + 0.13 * swell_shade * (1.0 - fresnel);

        // Only see-through up close: past ~40 blocks the alpha ramps to fully
        // opaque so distant water reads as a solid surface (looks better and
        // saves the see-through blend work). Reflection and glint are untouched,
        // so the specular highlight is identical near and far.
        float dist_b = length(ubo.view_pos.xz - world_pos.xz) / ubo.BS;
        c.a = mix(c.a, 1.0, smoothstep(40.0, 100.0, dist_b));

        c.rgb += glint;
    }

    if (ubo.underwater > 0.5) {
        // murky water: blue tint plus dense fog in every direction
        vec3 water_color = vec3(0.05, 0.18, 0.35);
        c.rgb *= vec3(0.4, 0.7, 1.0);
        float wfog = smoothstep(ubo.BS * 2.0, ubo.BS * 40.0, length(ubo.view_pos.xyz - world_pos.xyz));
        color = mix(c, vec4(water_color, 1.0), wfog);
    } else {
        // Fog based on distance from camera: distant terrain melts into the
        // flat sky color
        float dist = length(ubo.view_pos.xz - world_pos.xz);
        float fog_linear = smoothstep(ubo.fog_lo, ubo.fog_hi, dist);
        float fog = fog_linear * fog_linear * fog_linear;

        color = mix(c, vec4(SKY_COLOR, 1.0), fog);
    }
}
