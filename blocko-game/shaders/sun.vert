#version 450

layout(location = 0) out vec2 uv;

layout(push_constant) uniform Push {
    mat4 pv;       // proj * view (pre-multiplied)
    float pitch;
    float yaw;
    float roll;
    float time;    // frame counter for animation
} push;

// Quad half-size and distance
const float SIZE = 1000.0;
const float DIST = 10000.0;

void main()
{
    // Generate quad vertices from gl_VertexIndex (0-5 for two triangles)
    // Triangle 1: 0,1,2  Triangle 2: 3,4,5
    // Vertex order: BL, TL, TR, BL, TR, BR
    vec2 corners[6] = vec2[](
        vec2(-1, -1), vec2(-1, 1), vec2(1, 1),
        vec2(-1, -1), vec2(1, 1), vec2(1, -1)
    );

    vec2 corner = corners[gl_VertexIndex];

    // UV from corner (-1,-1 -> 0,0) to (1,1 -> 1,1)
    uv = corner * 0.5 + 0.5;

    // Quad positioned at +X, facing origin
    vec3 pos = vec3(DIST, corner.x * SIZE, corner.y * SIZE);

    // Build rotation matrix from pitch/yaw/roll
    float cosa = cos(push.pitch);
    float sina = sin(push.pitch);
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
    gl_Position.z = gl_Position.w;  // Force depth to 1.0 (always behind everything)
}
