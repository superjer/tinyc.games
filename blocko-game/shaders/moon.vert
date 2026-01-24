#version 450

layout(location = 0) out vec2 uv;

layout(push_constant) uniform Push {
    mat4 pv;       // proj * view (pre-multiplied)
    float pitch;
    float yaw;
    float roll;
    float time;    // frame counter for animation
} push;

// Quad half-size and distance (moon is smaller, opposite side)
const float SIZE = 400.0;
const float DIST = 10000.0;
const float PI = 3.14159265359;

void main()
{
    // Generate quad vertices from gl_VertexIndex (0-5 for two triangles)
    vec2 corners[6] = vec2[](
        vec2(-1, -1), vec2(-1, 1), vec2(1, 1),
        vec2(-1, -1), vec2(1, 1), vec2(1, -1)
    );

    vec2 corner = corners[gl_VertexIndex];

    // UV from corner
    uv = corner * 0.5 + 0.5;

    // Quad positioned at +X (will be rotated to opposite side of sun)
    vec3 pos = vec3(DIST, corner.x * SIZE, corner.y * SIZE);

    // Moon is opposite the sun: add PI to pitch
    float moon_pitch = push.pitch + PI;

    // Build rotation matrix from pitch/yaw/roll
    float cosa = cos(moon_pitch);
    float sina = sin(moon_pitch);
    float cosb = cos(push.yaw);
    float sinb = sin(push.yaw);
    float cosc = cos(push.roll);
    float sinc = sin(push.roll);

    mat3 rot = mat3(
        cosa * cosb,  cosa * sinb * cosc + sina * sinc,  cosa * sinb * sinc - sina * cosc,
        sina * cosb,  sina * sinb * cosc - cosa * sinc,  sina * sinb * sinc + cosa * cosc,
        -sinb,        cosb * cosc,                       cosb * sinc
    );

    vec3 rotated = rot * pos;

    gl_Position = push.pv * vec4(rotated, 1.0);
}
