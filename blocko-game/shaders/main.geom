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
layout(location = 8) flat out vec3 normal;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (near cascade)
    mat4 shadow2_space;    // offset 256 (mid cascade)
    mat4 shadow3a_space;   // offset 320 (far cascade A)
    mat4 shadow3b_space;   // offset 384 (far cascade B)
    mat4 shadow4a_space;   // offset 448 (extreme cascade A)
    mat4 shadow4b_space;   // offset 512 (extreme cascade B)
    float BS;              // offset 576
    float shadow3_blend;   // offset 580 (far: 0=A, 1=B)
    float shadow4_blend;   // offset 584 (extreme: 0=A, 1=B)

    vec3 day_color;        // offset 592
    vec3 glo_color;        // offset 608
    vec3 fog_color;        // offset 624
    float fog_lo;          // offset 636
    float fog_hi;          // offset 640
    vec3 light_pos;        // offset 656
    vec3 view_pos;         // offset 672
    float sharpness;       // offset 684
    bool shadow_mapping;   // offset 688
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

        // Calculate shadow space position for near cascade only
        // Mid/far cascade positions are computed in fragment shader (for A/B blending)
        // Normal offset bias prevents shadow bleeding at cube edges
        // Offset must exceed PCF world radius
        vec4 shadow_sample_pos = world_pos + vec4(normal_val * 75.0, 0.0);
        shadow_pos = ubo.shadow_space * shadow_sample_pos;

        EmitVertex();
    }

    EndPrimitive();
}
