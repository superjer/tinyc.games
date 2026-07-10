#version 450
#extension GL_GOOGLE_include_directive : require
#include "sky_color.glsl"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 world_dir;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Push {
    mat4 pvm;
    vec3 sun_dir;
    float night_amt;  // 0 = day, 0.5 = dusk, 1 = night
    float time;
    float underwater;
} push;

// Hash functions for procedural stars
float hash(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.x + p.y) * p.z);
}

vec3 hash3(vec3 p) {
    return vec3(hash(p), hash(p + 71.1), hash(p + 133.3));
}

// Generate stars using 3D cells (avoids pole pinching)
vec4 stars(vec3 dir) {
    vec3 n = normalize(dir);

    // Scale direction to create cell grid on unit sphere
    float scale = 50.0;
    vec3 p = n * scale;

    // Find the cell and local position
    vec3 cell = floor(p);
    vec3 local = fract(p);

    /*
    if (local.x < 0.01) return vec4(1.0, 0.0, 0.0, 1.0);
    if (local.y < 0.01) return vec4(0.0, 1.0, 0.0, 1.0);
    if (local.z < 0.01) return vec4(0.0, 0.0, 1.0, 1.0);
    */

    // Only some cells have a star
    float h = hash(cell + 0.5);
    if (h < 0.70) return vec4(0.0, 0.0, 0.0, 0.0);

    // Star position constrained to center of cell
    // so its glow never crosses cell boundaries
    vec3 star_offset = hash3(cell) * 0.8 + 0.1;

    // Distance from local position to star
    float d = length(local - star_offset);

    // Star brightness varies
    float brightness = hash(cell + 0.7);
    float is_bright = step(0.995, hash(cell + 1.3));  // .5% chance
    brightness = mix(brightness, 1.0, is_bright);

    // some stars twinkle
    float is_twinkly = step(0.06, hash(cell + 2.1));
    float twinkle_speed = hash(cell + 2.5) * 0.15 + 0.05;  // vary speed per star
    float twinkle_phase = hash(cell + 2.9) * 6.28;  // random phase
    float twinkle = sin(push.time * twinkle_speed + twinkle_phase) * 0.5 + 0.5;
    twinkle = (twinkle > 0.2 && twinkle < 0.3) || (twinkle > 0.6 && twinkle < 0.7) ? mix(0.4, 1.0, twinkle) : 1.0;

    // Sharp star point - max radius ~0.1 fits within cell margin
    float size = 0.05 + 0.05 * brightness;
    float intensity = smoothstep(size, 0.0, d);
    intensity *= mix(0.8 + 0.5 * brightness, 4.0, is_bright);
    intensity *= mix(1.0, twinkle, is_twinkly);
    vec3 color = hash3(cell) * 0.3 + vec3(0.7, 0.7, 0.7);

    color.b = (color.g > color.b && color.g > color.r) ? color.g : color.b; // no green stars

    return vec4(color, intensity);
}

void main()
{
    // The whole gradient lives in sky_color() (shared with main.frag's fog);
    // this shader just adds the stars and underwater murk on top.
    vec3 final_color = sky_color(world_dir, push.sun_dir, push.night_amt);
    float y = -normalize(world_dir).y;  // world up is -Y

    // Add stars at night (only above horizon)
    if (push.night_amt > 0.5 && y > 0.0) {
        vec4 star = stars(world_dir);
        float star_intensity = star.a;
        vec3 star_color = star.rgb;
        // Fade stars in as night deepens, and fade near horizon
        float night_factor = (push.night_amt - 0.5) * 2.0;
        float horizon_fade = smoothstep(0.0, 0.15, y);
        final_color += star_color * star_intensity * night_factor * horizon_fade;
    }

    if (push.underwater > 0.5) {
        // sink the whole sky toward deep-water murk (matches main.frag fog)
        vec3 water_deep = mix(vec3(0.05, 0.18, 0.35), vec3(0.0, 0.01, 0.04), push.night_amt);
        final_color = mix(final_color, water_deep, 0.9);
    }

    color = vec4(final_color, 1.0);
}
