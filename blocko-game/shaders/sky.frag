#version 450

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 world_dir;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Push {
    mat4 pvm;
    vec3 sun_dir;
    float night_amt;  // 0 = day, 0.5 = dusk, 1 = night
} push;

void main()
{
    // uv.y goes from -1 (bottom) to 1 (top)
    // Horizon is at y=0 (negate for Vulkan Y-flip)
    float y = -uv.y;

    // Day colors
    vec3 day_sky = vec3(0.4, 0.6, 1.0);
    vec3 day_horizon = vec3(0.8, 0.85, 0.95);

    // Dusk colors (orange/pink)
    vec3 dusk_sky = vec3(0.3, 0.2, 0.4);
    vec3 dusk_horizon = vec3(1.0, 0.5, 0.3);

    // Night colors
    vec3 night_sky = vec3(0.02, 0.02, 0.08);
    vec3 night_horizon = vec3(0.05, 0.05, 0.1);

    // Ground colors
    vec3 day_ground = vec3(0.3, 0.25, 0.2);
    vec3 night_ground = vec3(0.02, 0.02, 0.03);

    // Blend based on night_amt: 0->0.5 is day->dusk, 0.5->1 is dusk->night
    vec3 sky_color, horizon_color, ground_color;
    if (push.night_amt < 0.5) {
        float t = push.night_amt * 2.0;
        sky_color = mix(day_sky, dusk_sky, t);
        horizon_color = mix(day_horizon, dusk_horizon, t);
        ground_color = mix(day_ground, night_ground, t * 0.5);
    } else {
        float t = (push.night_amt - 0.5) * 2.0;
        sky_color = mix(dusk_sky, night_sky, t);
        horizon_color = mix(dusk_horizon, night_horizon, t);
        ground_color = mix(day_ground * 0.5, night_ground, t);
    }

    // Compute alignment with sun direction (on XZ plane)
    vec2 dir_xz = normalize(world_dir.xz);
    vec2 sun_xz = normalize(push.sun_dir.xz);
    float sun_align = dot(dir_xz, sun_xz) * (1.0 - abs(push.sun_dir.y));  // -1 (away) to 1 (toward sun)

    // Scale horizon band: larger toward sun, smaller away
    float horizon_scale = 1.3 + sun_align;
    if (horizon_scale < 0.3) {
        horizon_scale = 0.3;
    }

    if (horizon_scale < 1.0) {
        horizon_color = mix(sky_color, horizon_color, horizon_scale * horizon_scale);
    }

    if (y > 0.0) {
        // Above horizon - gradient from horizon to sky
        float t = smoothstep(0.0, 0.3 * horizon_scale, y);
        color = vec4(mix(horizon_color, sky_color, t), 1.0);
    } else {
        // Below horizon - gradient from horizon to ground
        float t = smoothstep(0.0, -0.3 * horizon_scale, y);
        color = vec4(mix(horizon_color, ground_color, t), 1.0);
    }
}
