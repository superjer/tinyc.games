#version 450

layout(location = 0) out vec2 uv;

layout(push_constant) uniform Push {
    mat4 pv;       // proj * view (pre-multiplied, no translation)
    vec3 sun_dir;  // unit vector toward the sun
    float time;    // frame counter for animation
} push;

// Quad half-size and distance
const float SIZE = 1000.0;
const float DIST = 10000.0;

void main()
{
    // Generate quad vertices from gl_VertexIndex (0-5 for two triangles)
    vec2 corners[6] = vec2[](
        vec2(-1, -1), vec2(-1, 1), vec2(1, 1),
        vec2(-1, -1), vec2(1, 1), vec2(1, -1)
    );

    vec2 corner = corners[gl_VertexIndex];

    // UV from corner (-1,-1 -> 0,0) to (1,1 -> 1,1)
    uv = corner * 0.5 + 0.5;

    // A quad centered on the sun direction, facing the origin
    vec3 dir = normalize(push.sun_dir);
    vec3 right = normalize(cross(vec3(0, 1, 0), dir));
    vec3 up = cross(dir, right);
    vec3 pos = dir * DIST + (corner.x * right + corner.y * up) * SIZE;

    gl_Position = push.pv * vec4(pos, 1.0);
    gl_Position.z = gl_Position.w;  // Force depth to 1.0 (always behind everything)
}
