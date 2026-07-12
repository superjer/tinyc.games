// block_geom.glsl - the single source of truth for a block face's local
// geometry, shared by every block pipeline (main.vert, shadow.vert, mob.vert,
// mob_shadow.vert) via #include. Given a face's orient `o` and cell size `bs`
// it fills the four triangle-strip corners a,b,c,d plus the face normal and the
// side-shading factor `sidel`.
//
// Handled here: the six solid cube faces (orient 1-6 = UP..DOWN) and the grass-
// slope wedge (orient 30-45 = 30 + kind*4 + facing, kinds 0 top / 1 west tri /
// 2 east tri / 3 back wall). Pass-specific shapes stay in the shaders that use
// them: the tall-grass billboards (orient 20/21, need per-cell hashing) and the
// water-surface recess (orient +10). A new block *shape* is added here once and
// every pipeline can draw it.

void block_geom(int o, float bs,
                out vec4 a, out vec4 b, out vec4 c, out vec4 d,
                out vec3 face_normal, out float sidel)
{
    // defaults so callers get defined values for orients handled elsewhere
    // (grass billboards) - those callers overwrite a..d/normal/sidel themselves.
    a = vec4(0); b = vec4(0); c = vec4(0); d = vec4(0);
    face_normal = vec3(0, -1, 0);
    sidel = 1.0f;

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
    }

    // grass slope wedge: build the descend-south base piece, then rotate it
    // 90*facing about the cell's vertical center. base = high side north
    // (z=bs, y=0), low side south (z=0, y=bs). the two side walls are triangles
    // emitted as degenerate quads (one vertex doubled). winding matches the cube
    // faces so back-face culling keeps them visible.
    if (o >= 30) {
        int kind = (o - 30) / 4;
        int facing = (o - 30) - kind * 4;
        switch (kind) {
        case 0: // sloped grass top (mirrors UP's XZ, low edge lifted to y=bs)
            a = vec4(0,  bs, 0,  0);
            b = vec4(bs, bs, 0,  0);
            c = vec4(0,  0,  bs, 0);
            d = vec4(bs, 0,  bs, 0);
            sidel = 1.0f;
            face_normal = normalize(vec3(0, -1, -1));
            break;
        case 1: // west triangle wall (x=0)
            a = vec4(0, bs, 0,  0);
            b = vec4(0, 0,  bs, 0);
            c = vec4(0, bs, 0,  0);
            d = vec4(0, bs, bs, 0);
            sidel = 0.9f;
            face_normal = vec3(-1, 0, 0);
            break;
        case 2: // east triangle wall (x=bs)
            a = vec4(bs, 0,  bs, 0);
            b = vec4(bs, bs, 0,  0);
            c = vec4(bs, bs, bs, 0);
            d = vec4(bs, bs, 0,  0);
            sidel = 0.9f;
            face_normal = vec3(1, 0, 0);
            break;
        default: // back wall = full north face (z=bs)
            a = vec4(0,  0,  bs, 0);
            b = vec4(bs, 0,  bs, 0);
            c = vec4(0,  bs, bs, 0);
            d = vec4(bs, bs, bs, 0);
            sidel = 0.8f;
            face_normal = vec3(0, 0, 1);
            break;
        }
        // rotate 90*facing: (x,z) -> (z, bs-x) turns the high side
        // north->east->south->west (SLOPE_S/W/N/E), matching player.c collision
        vec4 g[4] = vec4[4](a, b, c, d);
        for (int r = 0; r < facing; r++) {
            for (int k = 0; k < 4; k++) {
                float nx = g[k].z;
                float nz = bs - g[k].x;
                g[k].x = nx; g[k].z = nz;
            }
            float fnx = face_normal.z;
            float fnz = -face_normal.x;
            face_normal.x = fnx; face_normal.z = fnz;
        }
        a = g[0]; b = g[1]; c = g[2]; d = g[3];
    }
}

// slope surface UV fixup for the color passes. The sloped top is sqrt(2) longer
// up-slope than across, so its 0..1 uv would stretch texels tall - run the
// up-slope uv to 23/16 (23 texels to 16 across = square; the sampler repeats).
// The side/back walls drive the vertical from height (rotation leaves y alone)
// so the grass strip lands at the top; the west triangle (kind 1) uses the
// mirror of the diagonal-grass texture, so flip its u.
vec2 slope_uv(int o, vec2 uv, vec4 off, float bs)
{
    int k = (o - 30) / 4;
    if (k == 0)
        uv.y *= 23.0 / 16.0;
    else {
        uv.y = off.y / bs;
        if (k == 1) uv.x = 1.0 - uv.x;
    }
    return uv;
}
