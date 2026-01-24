#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Push {
    mat4 pv;
    float pitch;
    float yaw;
    float roll;
    float time;    // frame counter for animation
} push;

void main()
{
    // Distance from center of quad
    vec2 centered = uv - vec2(0.5);
    float dist = length(centered) * 2.0;

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
    rays += sin(angle * 8.0 + push.time * 0.01) * 0.5 + 0.5;
    rays += sin(angle * 13.0 + 0.5 - push.time * 0.012) * 0.25 + 0.25;
    rays += sin(angle * 5.0 - 0.3) * 0.3 + 0.3;
    rays = rays / 2.0;
    // Rays fade with distance but extend beyond disc
    float ray_mask = (1.0 - smoothstep(0.2, 0.95, dist)) * smoothstep(0.15, 0.3, dist);
    rays *= ray_mask * 0.6;

    // Combine effects
    float brightness = core_intensity * 2.0 + disc + glow * 0.8 + rays;
    float alpha = disc + glow * 0.1 + rays * 0.4;
    alpha = clamp(alpha, 0.0, 1.0);

    // Color: white core fading to yellow/orange at edges
    vec3 white = vec3(1.0, 1.0, 1.0);
    vec3 yellow = vec3(1.0, 0.95, 0.7);
    vec3 orange = vec3(1.0, 0.7, 0.3);

    vec3 sun_color = mix(white, yellow, smoothstep(0.0, 0.5, dist));
    sun_color = mix(sun_color, orange, smoothstep(0.5, 0.9, dist));
    sun_color *= brightness;

    // HDR-ish: let it go above 1.0 for bloom-like effect
    sun_color = clamp(sun_color, 0.0, 1.5);

    color = vec4(sun_color, alpha);
}
