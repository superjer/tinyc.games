#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Push {
    mat4 pvm;
    float is_moon;
} push;

void main()
{
    // Distance from center of quad
    vec2 centered = uv - vec2(0.5);
    float dist = length(centered) * 2.0;

    if (push.is_moon > 0.5) {
        // Moon: cool bluish-white with craters
        float alpha = 1.0 - smoothstep(0.7, 0.8, dist);

        // Base moon color with slight variation
        vec3 moon_light = vec3(1.0, 1.0, 1.0);
        vec3 moon_dark = vec3(0.56, 0.58, 0.64);
        vec3 moon_deep = vec3(0.52, 0.56, 0.60);

        // Crater function: returns darkness (0-1) based on distance to crater center
        #define CRATER(cx, cy, r) smoothstep(r, r * 0.3, length(centered - vec2(cx, cy)))

        // Place craters at various positions and sizes
        float craters = 0.0;
        craters = max(craters, CRATER( 0.14,  0.18, 0.31));
        craters = max(craters, CRATER(-0.07,  0.10, 0.25));
        craters = max(craters, CRATER(-0.18,  0.15, 0.24));
        craters = max(craters, CRATER(-0.15,  0.28, 0.12));
        craters = max(craters, CRATER(-0.07,  0.00, 0.19));
        craters = max(craters, CRATER(-0.15, -0.14, 0.15));
        craters = max(craters, CRATER(-0.11, -0.26, 0.11));

        float deep_craters = 0.0;
        deep_craters = max(deep_craters, CRATER( 0.05,  0.18, 0.08));
        deep_craters = max(deep_craters, CRATER( 0.01,  0.07, 0.10));
        deep_craters = max(deep_craters, CRATER(-0.20,  0.17, 0.09));

        // Darken craters
        vec3 moon_color = mix(moon_light, moon_dark, craters * 0.7);
        moon_color = mix(moon_color, moon_deep, deep_craters * 0.7);

        // Slight limb darkening (darker at edges)
        moon_color *= 1.0 - dist * 0.15;

        color = vec4(moon_color, alpha);
    } else {
        // Sun: intense brightness with glow and rays

        // Bright core - overexposed white center
        float core_intensity = 1.0 - smoothstep(0.0, 0.3, dist);

        // Inner disc
        float disc = 1.0 - smoothstep(0.25, 0.35, dist);

        // Soft outer glow - extends further
        float glow = 1.0 - smoothstep(0.1, 1.0, dist);
        glow = pow(glow, 2.0);  // softer falloff

        // Radial rays - using angle from center
        float angle = atan(centered.y, centered.x);
        float rays = 0.0;
        // Multiple ray frequencies for complex pattern
        rays += sin(angle * 8.0) * 0.5 + 0.5;
        rays += sin(angle * 16.0 + 0.5) * 0.25 + 0.25;
        rays += sin(angle * 4.0 - 0.3) * 0.3 + 0.3;
        rays = rays / 3.0;
        // Rays fade with distance but extend beyond disc
        float ray_mask = (1.0 - smoothstep(0.2, 0.95, dist)) * smoothstep(0.15, 0.3, dist);
        rays *= ray_mask * 0.6;

        // Combine effects
        float brightness = core_intensity * 2.0 + disc + glow * 0.8 + rays;
        float alpha = disc + glow * 0.7 + rays * 0.5;
        alpha = clamp(alpha, 0.0, 1.0);

        // Color: white core fading to yellow/orange at edges
        vec3 white = vec3(1.0, 1.0, 1.0);
        vec3 yellow = vec3(1.0, 0.95, 0.7);
        vec3 orange = vec3(1.0, 0.7, 0.3);

        vec3 sun_color = mix(white, yellow, smoothstep(0.0, 0.3, dist));
        sun_color = mix(sun_color, orange, smoothstep(0.3, 0.8, dist));
        sun_color *= brightness;

        // HDR-ish: let it go above 1.0 for bloom-like effect
        sun_color = clamp(sun_color, 0.0, 1.5);

        color = vec4(sun_color, alpha);
    }
}
