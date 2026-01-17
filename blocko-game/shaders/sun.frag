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

    // Circular falloff for a round sun/moon look
    float alpha = 1.0 - smoothstep(0.7, 1.0, dist);

    if (push.is_moon > 0.5) {
        // Moon: slightly dimmer, hint of yellow
        color = vec4(0.95, 0.95, 0.85, alpha);
    } else {
        // Sun: bright white/yellow
        color = vec4(1.0, 1.0, 0.9, alpha);
    }
}
