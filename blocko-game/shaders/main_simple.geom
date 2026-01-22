#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in float tex_vs[];
layout(location = 1) in float orient_vs[];
layout(location = 2) in vec4 illum_vs[];
layout(location = 3) in vec4 glow_vs[];
layout(location = 4) in float alpha_vs[];
layout(location = 5) in vec4 world_pos_vs[];

layout(location = 0) flat out float tex;
layout(location = 1) out float illum;
layout(location = 2) out float glow;
layout(location = 3) flat out float alpha;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec4 world_pos_out;
layout(location = 6) out vec4 shadow_pos;
layout(location = 7) out vec4 shadow2_pos;
layout(location = 8) flat out vec3 normal;
layout(location = 9) out vec4 shadow3_pos;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;           // offset 0
    mat4 view;            // offset 64
    mat4 proj;            // offset 128
    mat4 shadow_space;    // offset 192
    mat4 shadow2_space;   // offset 256
    mat4 shadow3_space;   // offset 320
    float BS;             // offset 384

    vec3 day_color;       // offset 400
    vec3 glo_color;       // offset 416
    vec3 fog_color;       // offset 432
    float fog_lo;         // offset 444
    float fog_hi;         // offset 448
    vec3 light_pos;       // offset 464
    vec3 view_pos;        // offset 480
    float sharpness;      // offset 492
    bool shadow_mapping;  // offset 496
} ubo;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
} push;

void main(void) {
    float sidel = 0.0f;
    vec4 a, b, c, d;
    vec3 face_normal;

    switch (int(orient_vs[0])) {
        case 1: // UP (Y-)
            a = vec4(0, 0, 0, 0);
            b = vec4(push.bs, 0, 0, 0);
            c = vec4(0, 0, push.bs, 0);
            d = vec4(push.bs, 0, push.bs, 0);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);
            break;
        case 2: // EAST (X+)
            a = vec4(push.bs, 0, push.bs, 0);
            b = vec4(push.bs, 0, 0, 0);
            c = vec4(push.bs, push.bs, push.bs, 0);
            d = vec4(push.bs, push.bs, 0, 0);
            sidel = 0.9f;
            face_normal = vec3(1, 0, 0);
            break;
        case 3: // NORTH (Z+)
            a = vec4(0, 0, push.bs, 0);
            b = vec4(push.bs, 0, push.bs, 0);
            c = vec4(0, push.bs, push.bs, 0);
            d = vec4(push.bs, push.bs, push.bs, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, 1);
            break;
        case 4: // WEST (X-)
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, push.bs, 0);
            c = vec4(0, push.bs, 0, 0);
            d = vec4(0, push.bs, push.bs, 0);
            sidel = 0.9f;
            face_normal = vec3(-1, 0, 0);
            break;
        case 5: // SOUTH (Z-)
            a = vec4(push.bs, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(push.bs, push.bs, 0, 0);
            d = vec4(0, push.bs, 0, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, -1);
            break;
        case 6: // DOWN (Y+)
            a = vec4(push.bs, push.bs, 0, 0);
            b = vec4(0, push.bs, 0, 0);
            c = vec4(push.bs, push.bs, push.bs, 0);
            d = vec4(0, push.bs, push.bs, 0);
            sidel = 0.6f;
            face_normal = vec3(0, 1, 0);
            break;
    }

    float tex_val = tex_vs[0];
    float alpha_val = alpha_vs[0];
    vec3 normal_val = face_normal;

    vec4 vertex_pos = world_pos_vs[0];
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };

    for (int i = 0; i < 4; i++) {
        // Set flat outputs inside loop to ensure they're set for each vertex
        // (workaround for potential driver bugs with flat interpolation)
        tex = tex_val;
        alpha = alpha_val;
        normal = normal_val;

        vec4 world_pos = vertex_pos + offsets[i];
        gl_Position = push.pv * world_pos;
        illum = (0.1 + illum_vs[0][i]) * sidel;
        glow = (0.1 + glow_vs[0][i]) * sidel;
        uv = uvs[i];
        world_pos_out = world_pos;
        // Calculate shadow space positions with normal offset bias
        // Offset along normal to prevent shadow bleeding at cube edges
        vec4 shadow_sample_pos = world_pos + vec4(normal_val * 10.0, 0.0);
        shadow_pos = ubo.shadow_space * shadow_sample_pos;
        shadow2_pos = ubo.shadow2_space * shadow_sample_pos;
        shadow3_pos = ubo.shadow3_space * shadow_sample_pos;
        EmitVertex();
    }

    EndPrimitive();
}
