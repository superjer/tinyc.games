#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

void main()
{
    // uv.y goes from -1 (bottom) to 1 (top)
    // Horizon is at y=0 (negate for Vulkan Y-flip)
    float y = -uv.y;

    vec3 sky_blue = vec3(0.4, 0.6, 1.0);
    vec3 horizon_color = vec3(0.8, 0.85, 0.95);
    vec3 ground_color = vec3(0.3, 0.25, 0.2);

    if (y > 0.0) {
        // Above horizon - gradient from horizon to sky blue
        float t = smoothstep(0.0, 0.6, y);
        color = vec4(mix(horizon_color, sky_blue, t), 1.0);
    } else {
        // Below horizon - gradient from horizon to ground
        float t = smoothstep(0.0, -0.3, y);
        color = vec4(mix(horizon_color, ground_color, t), 1.0);
    }
}
