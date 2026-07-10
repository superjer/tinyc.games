// Sky gradient as a pure function of view direction — shared by the skydome
// (sky.frag) and the per-pixel fog in main.frag, so distant terrain fogs to
// exactly the sky color behind it. Stars and underwater murk are layered on
// separately in sky.frag; they don't belong in fog.
vec3 sky_color(vec3 dir, vec3 sun_dir, float night_amt)
{
    dir = normalize(dir);
    float y = -dir.y;  // world up is -Y

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
    vec3 sky, horizon, ground;
    if (night_amt < 0.5) {
        float t = night_amt * 2.0;
        sky = mix(day_sky, dusk_sky, t);
        horizon = mix(day_horizon, dusk_horizon, t);
        ground = mix(day_ground, night_ground, t * 0.5);
    } else {
        float t = (night_amt - 0.5) * 2.0;
        sky = mix(dusk_sky, night_sky, t);
        horizon = mix(dusk_horizon, night_horizon, t);
        ground = mix(day_ground * 0.5, night_ground, t);
    }

    // Alignment with the sun on the XZ plane: the horizon band widens toward
    // the sun and narrows away from it. Guard both normalizes — dir.xz
    // vanishes looking straight up/down, sun_dir.xz at noon.
    float dlen = length(dir.xz);
    float slen = length(sun_dir.xz);
    float sun_align = (dlen > 1e-4 && slen > 1e-4)
        ? dot(dir.xz / dlen, sun_dir.xz / slen) * (1.0 - abs(sun_dir.y))
        : 0.0;  // -1 (away) to 1 (toward sun)

    float horizon_scale = max(1.3 + sun_align, 0.3);
    if (horizon_scale < 1.0)
        horizon = mix(sky, horizon, horizon_scale * horizon_scale);

    if (y > 0.0)  // above horizon: horizon -> sky
        return mix(horizon, sky, smoothstep(0.0, 0.3 * horizon_scale, y));
    else          // below horizon: horizon -> ground
        return mix(horizon, ground, smoothstep(0.0, -0.3 * horizon_scale, y));
}
