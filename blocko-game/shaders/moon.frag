#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

void main()
{
    // Distance from center of quad
    vec2 centered = uv - vec2(0.5);
    float dist = length(centered) * 2.0;

    // Moon: cool with craters
    float alpha = 1.0 - smoothstep(0.77, 0.8, dist);

    // Base moon color
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
}
