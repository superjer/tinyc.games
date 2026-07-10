#version 450
#extension GL_GOOGLE_include_directive : require
#include "sky_color.glsl"

layout(location = 0) out vec4 color;

layout(location = 0) flat in float tex;
layout(location = 1) in float illum;
layout(location = 2) in float glow;
layout(location = 3) flat in float alpha;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec4 world_pos;
layout(location = 6) in vec4 shadow_pos;
layout(location = 8) flat in vec3 normal;
layout(location = 9) flat in float shiny;  // >0 = wet/glossy surface (slimes); set by the vertex shader
layout(location = 10) flat in float tint;  // >0 = debug: blend this fragment 50% red (patch viz)

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (the one near cascade)
    float BS;              // offset 256
    vec3 day_color;        // offset 272
    vec3 glo_color;        // offset 288
    float fog_lo;          // offset 300
    float fog_hi;          // offset 304
    vec3 light_pos;        // offset 320
    vec3 view_pos;         // offset 336
    float sharpness;       // offset 348
    bool shadow_mapping;   // offset 352
    float sun_strength;    // offset 356
    float sun_warmth;      // offset 360
    int water_frame;           // offset 364
    float underwater;          // offset 368 (camera eye is in water)
    float scootx;              // offset 372 (window->world block offset)
    float scootz;              // offset 376
    vec3 sun_dir;              // offset 384 (unit vector toward the sun)
    float night_amt;           // offset 396 (0 day, 0.5 dusk, 1 night)
    float shadow_fade;         // offset 400 (1 full shadows, ->0 eases contrast out before the idle cutoff)
} ubo;

// No push constants here: this fragment shader is shared by the terrain
// (main.vert), water, and mob (mob.vert) pipelines, which have different push
// layouts. The per-draw scalars it needs (shiny, tint) arrive as flat varyings.

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

// cheap 2D value noise for breaking up water wave regularity
float hash2(vec2 q) {
    vec3 p3 = fract(vec3(q.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 q) {
    vec2 i = floor(q);
    vec2 f = fract(q);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i),               hash2(i + vec2(1, 0)), u.x),
               mix(hash2(i + vec2(0, 1)),  hash2(i + vec2(1, 1)), u.x), u.y);
}

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

    // water gets an animated rippled normal for lighting
    vec3 N = normal;
    float swell_shade = 0.0;  // view-independent brightness roll from the long swell
    if (alpha < 1.0) {
        float t = float(ubo.water_frame) * 0.03;
        // absolute world coords (window mesh slides on scoot): subtract scoot so
        // the wave phase stays pinned to the world and doesn't snap at a scoot
        vec2 p = world_pos.xz / ubo.BS - vec2(ubo.scootx, ubo.scootz);

        // fade ripple detail with distance so normals flatten out
        // long before a pixel spans a whole wave (kills moire)
        float dist_b = length(ubo.view_pos.xz - world_pos.xz) / ubo.BS;
        float swell_amt = 1.0 - smoothstep(20.0, 120.0, dist_b);
        float chop_amt  = 1.0 - smoothstep(4.0, 28.0, dist_b);

        // noise kills the repetition: warp bends the wavefronts so they
        // wander, energy makes patches of calm and choppy water
        float warp = 6.283 * vnoise(p * 0.13 + t * 0.06);
        float energy = 0.5 + vnoise(p * 0.045 - t * 0.03);

        // directional waves at odd angles and incommensurate
        // frequencies so no grid or repeat pattern lines up
        const vec2 D1 = vec2( 0.36,  0.93), D2 = vec2(-0.80,  0.60),
                   D3 = vec2( 0.98, -0.17), D4 = vec2(-0.51, -0.86);
        vec2 slope = energy * (
            swell_amt * (0.055 * D1 * cos(dot(p, D1) *  2.9 + t * 1.6 + warp)
                       + 0.045 * D2 * cos(dot(p, D2) *  4.3 - t * 1.2 + 1.7 * warp))
          + chop_amt  * (0.040 * D3 * cos(dot(p, D3) * 14.7 + t * 4.1 + 2.6 * warp)
                       + 0.035 * D4 * cos(dot(p, D4) * 18.3 - t * 3.3 + 3.4 * warp)));

        // Long, low-frequency swell that survives to the horizon. Its wavelength
        // (~20-35 blocks) stays many pixels wide even at range, so unlike the
        // short ripples above it doesn't alias when a pixel spans a wave - it
        // gives distant water gently rolling light/dark detail instead of a flat
        // mirror. Only lightly faded, so it's still present at the fog line.
        const vec2 L1 = vec2( 0.71,  0.70), L2 = vec2(-0.34,  0.94);
        float long_amt = 1.0 - 0.5 * smoothstep(40.0, 300.0, dist_b);
        slope += long_amt * (
              0.030 * L1 * cos(dot(p, L1) * 0.19 + t * 0.6 + warp)
            + 0.024 * L2 * cos(dot(p, L2) * 0.28 - t * 0.5 + 0.7 * warp));

        N = normalize(N + vec3(slope.x, 0.0, slope.y));

        // Height of that same long swell (sin is the integral of the cos slope
        // above), used as a view-INDEPENDENT brightness roll. The reflection
        // detail vanishes looking straight down (fresnel -> 0), so this term
        // keeps the crests bright and troughs dark from directly overhead.
        swell_shade = long_amt * (0.5 * sin(dot(p, L1) * 0.19 + t * 0.6 + warp)
                                + 0.5 * sin(dot(p, L2) * 0.28 - t * 0.5 + 0.7 * warp));
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
            unshadow = sampleShadowPCF(shadow_near, shadow_pos.xyz, 0.003, rotation);
            vec2 e = shadow_pos.xy;
            float fx = max((0.1 - e.x) * 10.0, (e.x - 0.9) * 10.0);
            float fy = max((0.1 - e.y) * 10.0, (e.y - 0.9) * 10.0);
            float edge = clamp(max(fx, fy), 0.0, 1.0);
            unshadow = max(unshadow, edge);
        }

        // Ease shadow contrast out as the light nears the horizon, so the
        // idle cutoff (shadow_mapping -> false at ~5% strength) lands on an
        // already-shadowless scene instead of popping.
        unshadow = mix(1.0, unshadow, ubo.shadow_fade);

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

        // sharp sun/moon glitter on the rippled water surface,
        // added after texturing so it reads as a reflection
        if (alpha < 1.0)
            glint = light_tint * (1.5 * unshadow * ubo.sun_strength
                    * pow(max(dot(N, halfway_dir), 0.0), 80.0));

        // slimes read as wet: a tight glossy hotspot plus a fresnel sheen
        if (shiny > 0.5) {
            float wet = pow(max(dot(N, halfway_dir), 0.0), 64.0);
            float fres = pow(1.0 - max(dot(view_dir, N), 0.0), 3.0);
            glint += light_tint * (2.0 * unshadow * ubo.sun_strength * wet)
                   + vec3(0.18, 0.32, 0.24) * fres;
        }

        sky = (s1 * illum + s0 * directional) * light_tint * ubo.day_color;
    }

    vec3 glo_contrib = vec3(glow) * ubo.glo_color;
    vec3 combined = sky + glo_contrib * (vec3(1.0) - sky);
    vec4 c = texel * vec4(combined, alpha);

    if (shiny > 0.5) c.rgb += glint;

    if (alpha < 1.0 && ubo.underwater < 0.5) {
        // fresnel: mirror the sky at grazing angles, clear straight down
        vec3 vdir = normalize(ubo.view_pos - world_pos.xyz);
        float fresnel = pow(1.0 - abs(dot(vdir, N)), 3.0);
        vec3 mirrored = sky_color(reflect(-vdir, N), ubo.sun_dir, ubo.night_amt);
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
        vec3 water_color = vec3(0.05, 0.18, 0.35) * ubo.day_color;
        c.rgb *= vec3(0.4, 0.7, 1.0);
        float wfog = smoothstep(ubo.BS * 2.0, ubo.BS * 40.0, length(ubo.view_pos.xyz - world_pos.xyz));
        color = mix(c, vec4(water_color, 1.0), wfog);
    } else {
        // Fog based on distance from camera, colored per-pixel to match the
        // sky in this exact direction so distant terrain melts into the dome
        float dist = length(ubo.view_pos.xz - world_pos.xz);
        float fog_linear = smoothstep(ubo.fog_lo, ubo.fog_hi, dist);
        float fog = fog_linear * fog_linear * fog_linear;
        vec3 fog_color = sky_color(world_pos.xyz - ubo.view_pos, ubo.sun_dir, ubo.night_amt);

        color = mix(c, vec4(fog_color, 1.0), fog);
    }

    // debug viz: tint the reject+patch mesh red (socket `tint`, via reject_lo.w)
    if (tint > 0.5)
        color.rgb = mix(color.rgb, vec3(1.0, 0.0, 0.0), 0.5);
}
