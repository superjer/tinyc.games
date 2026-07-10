#version 450

// Instanced quads: one instance per face, 4 vertices via triangle strip.
// All inputs are instance-rate attributes (one struct vbufv per face).

layout(location = 0) in float tex_in;
layout(location = 1) in float orient_in;
layout(location = 2) in vec3 pos_in;
layout(location = 3) in vec4 illum_in;
layout(location = 4) in vec4 glow_in;
layout(location = 5) in float alpha_in;

layout(location = 0) flat out float tex;
layout(location = 1) out float illum;
layout(location = 2) out float glow;
layout(location = 3) flat out float alpha;
layout(location = 4) out vec2 uv;
layout(location = 5) out vec4 world_pos_out;
layout(location = 6) out vec4 shadow_pos;
layout(location = 8) flat out vec3 normal;
layout(location = 9) flat out float shiny;  // terrain is never glossy -> 0
layout(location = 10) flat out float tint;  // debug patch tint flag (reject_lo.w)

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 model;            // offset 0
    mat4 view;             // offset 64
    mat4 proj;             // offset 128
    mat4 shadow_space;     // offset 192 (the one near cascade)
    float BS;              // offset 256
    vec3 glo_color;        // offset 272
    float fog_lo;          // offset 284
    float fog_hi;          // offset 288
    vec3 light_pos;        // offset 304
    vec3 view_pos;         // offset 320
    bool shadow_mapping;   // offset 332
    int water_frame;       // offset 336
    float underwater;      // offset 340
    float scootx;          // offset 344 (window->world block offset)
    float scootz;          // offset 348
} ubo;

layout(push_constant) uniform Push {
    mat4 pv;
    float chunk_x;
    float chunk_y;
    float chunk_z;
    float bs;
    vec4 reject_lo;   // .xyz = window-tile box lo (inclusive); box empty when lo > hi
    vec4 reject_hi;   // faces of cells inside the box are culled (the patch redraws them)
                      // .w of reject_lo = debug patch tint flag (passed to the frag as `tint`)
} push;

void main(void) {
    float sidel = 0.0f;
    vec4 a, b, c, d;
    vec3 face_normal;
    float bs = push.bs;

    // water surface faces are tagged with orient + 10: the top edge is pulled
    // down to the recessed water line (see below) while the bottom stays put,
    // so the wall is shortened (not lowered) to meet the lowered top face.
    int o = int(orient_in);
    bool recess_top = o >= 10 && o < 20;
    if (recess_top) o -= 10;

    // tall grass (orient 20/21): two crossed billboard planes centered in the
    // cell, rotated by a per-cell random angle and jittered - handled below
    // after the corner offsets are chosen.
    bool grass = o >= 20;

    switch (o) {
        case 1: // UP (Y-)
            a = vec4(0, 0, 0, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(0, 0, bs, 0);
            d = vec4(bs, 0, bs, 0);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);
            break;
        case 2: // EAST (X+)
            a = vec4(bs, 0, bs, 0);
            b = vec4(bs, 0, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(bs, bs, 0, 0);
            sidel = 0.9f;
            face_normal = vec3(1, 0, 0);
            break;
        case 3: // NORTH (Z+)
            a = vec4(0, 0, bs, 0);
            b = vec4(bs, 0, bs, 0);
            c = vec4(0, bs, bs, 0);
            d = vec4(bs, bs, bs, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, 1);
            break;
        case 4: // WEST (X-)
            a = vec4(0, 0, 0, 0);
            b = vec4(0, 0, bs, 0);
            c = vec4(0, bs, 0, 0);
            d = vec4(0, bs, bs, 0);
            sidel = 0.9f;
            face_normal = vec3(-1, 0, 0);
            break;
        case 5: // SOUTH (Z-)
            a = vec4(bs, 0, 0, 0);
            b = vec4(0, 0, 0, 0);
            c = vec4(bs, bs, 0, 0);
            d = vec4(0, bs, 0, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, -1);
            break;
        case 6: // DOWN (Y+)
            a = vec4(bs, bs, 0, 0);
            b = vec4(0, bs, 0, 0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(0, bs, bs, 0);
            sidel = 0.6f;
            face_normal = vec3(0, 1, 0);
            break;
        case 20: // grass plane A: full-height quad spanning X, centered in Z
            a = vec4(bs, 0, bs * 0.5, 0);
            b = vec4(0,  0, bs * 0.5, 0);
            c = vec4(bs, bs, bs * 0.5, 0);
            d = vec4(0,  bs, bs * 0.5, 0);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);  // sky-facing (Y is down here), lit like a block top
            break;
        case 21: // grass plane B: full-height quad spanning Z, centered in X
            a = vec4(bs * 0.5, 0, 0, 0);
            b = vec4(bs * 0.5, 0, bs, 0);
            c = vec4(bs * 0.5, bs, 0, 0);
            d = vec4(bs * 0.5, bs, bs, 0);
            sidel = 1.0f;
            face_normal = vec3(0, -1, 0);  // sky-facing (Y is down here), lit like a block top
            break;
    }

    // tall grass: rotate the crossed planes about the cell's vertical center
    // axis by a per-cell random angle, and jitter them within the cell. both
    // planes of a cell share the same angle/jitter so they stay crossed. all
    // deterministic from the absolute cell coords (reconstructed like `cell`
    // below), so the shaggy patch pattern is stable in the world grid.
    if (grass) {
        vec3 gcell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / bs + pos_in;
        // window->world: the mesh is built in window coords that slide with
        // scoot, so hash the absolute cell or the pattern shifts at boundaries
        gcell.x -= ubo.scootx;
        gcell.z -= ubo.scootz;
        vec2 h = floor(gcell.xz);
        float ang = fract(sin(dot(h, vec2(127.1, 311.7))) * 43758.5453) * 6.2831853;
        float jx  = (fract(sin(dot(h, vec2(269.5,  183.3))) * 43758.5453) - 0.5) * 0.45 * bs;
        float jz  = (fract(sin(dot(h, vec2(419.2,  371.9))) * 43758.5453) - 0.5) * 0.45 * bs;
        float ca = cos(ang), sa = sin(ang);
        vec4 g[4] = vec4[4](a, b, c, d);
        for (int k = 0; k < 4; k++) {
            vec2 rel = vec2(g[k].x, g[k].z) - bs * 0.5;
            g[k].x = rel.x * ca - rel.y * sa + bs * 0.5 + jx;
            g[k].z = rel.x * sa + rel.y * ca + bs * 0.5 + jz;
        }
        a = g[0]; b = g[1]; c = g[2]; d = g[3];
    }

    // pull the top edge (the verts at local y == 0) down to the water line. top
    // faces have all four verts there (whole face drops); side faces only the top
    // two (wall shortens); bottom faces none (untouched).
    if (recess_top) {
        float r = 0.06 * bs;
        if (a.y < bs * 0.5) a.y = r;
        if (b.y < bs * 0.5) b.y = r;
        if (c.y < bs * 0.5) c.y = r;
        if (d.y < bs * 0.5) d.y = r;
    }

    vec3 world = push.bs * pos_in + vec3(push.chunk_x, push.chunk_y, push.chunk_z);
    vec4 vertex_pos = vec4(world, 1.0);
    vec4 offsets[4] = {a, b, c, d};
    vec2 uvs[4] = { vec2(1,0), vec2(0,0), vec2(1,1), vec2(0,1) };
    int i = gl_VertexIndex;

    tex = tex_in;
    alpha = alpha_in;
    normal = face_normal;
    shiny = 0.0;               // terrain is matte
    tint = push.reject_lo.w;   // debug: patch_render sets this to tint the patch red

    vec4 world_pos = vertex_pos + offsets[i];
    gl_Position = push.pv * world_pos;

    // reject: if this face's block-cell is inside the pending edit box, collapse
    // the whole quad to a zero-area degenerate so it emits no fragments. pos_in
    // is the cell (chunk-local m,y,n); reconstruct the absolute window cell the
    // same way world position is built. The patch mesh redraws the box.
    vec3 cell = vec3(push.chunk_x, push.chunk_y, push.chunk_z) / push.bs + pos_in;
    if (all(greaterThanEqual(cell, push.reject_lo.xyz - 0.5)) &&
        all(lessThanEqual(cell, push.reject_hi.xyz + 0.5)))
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    illum = (0.1 + illum_in[i]) * sidel;
    glow = (0.1 + glow_in[i]) * sidel;
    uv = uvs[i];
    world_pos_out = world_pos;

    // Calculate shadow space position. Normal offset bias prevents shadow
    // bleeding at cube edges; the offset must exceed the PCF world radius
    vec4 shadow_sample_pos = world_pos + vec4(face_normal * 75.0, 0.0);
    shadow_pos = ubo.shadow_space * shadow_sample_pos;
}
