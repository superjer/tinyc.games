#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_RAYCAST_C_INCLUDED
#define BLOCKO_PMEDIT_RAYCAST_C_INCLUDED

// pmedit/raycast.c - cursor ray vs the preview prisms, and world->screen projection

// cast the cursor's ray against the preview prisms. Returns the piece hit
// (or -1), plus which face (orient code) and the texel under the cursor.
// `only` restricts the test to one piece (painting), -1 tests all (selecting),
// -2 - i tests all EXCEPT piece i (PARENT mode: the selection draws behind
// everything, so rays pass through it to the candidates you actually see).
// `pad` fattens the prisms by that many px - it's the miss tolerance that
// keeps a near-miss click from backing out of the piece view. Paint rays use
// pad 0: a padded hit lands on the inflated surface, whose texel can be a
// pixel off from the real face under the cursor. `hitp`, when non-NULL, gets
// the hit point in the prism's local px space (0..dims from its min corner).
static int pmedit_pick(float mx, float my, int only, float pad,
                int *face, int *tu, int *tv, float *hitp)
{
        if (!pmedit_proj[0] || !pmedit_proj[5]) return -1;
        struct pmodel *mo = &pm_models[my_player];

        // cursor -> world ray, through the editor's own leveled frustum
        // (the same narrow-FOV projection the model is drawn with)
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw);
        float kx = (2.f * mx / screenw - 1.f) / pmedit_proj[0];
        float ky = (2.f * my / screenh - 1.f) / pmedit_proj[5];
        float D[3] = { f[0] + vm[0] * kx + vm[1] * ky,
                       f[1] + vm[4] * kx + vm[5] * ky,
                       f[2] + vm[8] * kx + vm[9] * ky };
        float O[3] = { peye0, peye1, peye2 };

        int best = -1;
        float best_t = 1e30f;
        int i0 = only < 0 ? 0 : only;
        int i1 = only < 0 ? pmedit_nr - 1 : only;
        int skip = only < -1 && only != PMEDIT_PICK_ELIG
                        ? -2 - only : -1; // only = -2-i: all EXCEPT i
        for (int i = i0; i <= i1; i++)
        {
                if (i == skip) continue;
                if (pmedit_hidden >> i & 1) continue; // hidden: dead to rays
                if (only == PMEDIT_PICK_ELIG && pmedit_cycle(i)) continue;
                float *M = pmedit_mats[i];
                // upper 3x3 is rotation * uniform scale: inverse = transpose/s^2
                float s2 = M[0]*M[0] + M[1]*M[1] + M[2]*M[2];
                if (!s2) continue;
                float w[3] = { O[0] - M[12], O[1] - M[13], O[2] - M[14] };
                float Ol[3], Dl[3];
                for (int a = 0; a < 3; a++)
                {
                        Ol[a] = (M[4*a]*w[0] + M[4*a+1]*w[1] + M[4*a+2]*w[2]) / s2;
                        Dl[a] = (M[4*a]*D[0] + M[4*a+1]*D[1] + M[4*a+2]*D[2]) / s2;
                }

                // slab test against the prism [0, dims], fattened by pad
                float tmin = -1e30f, tmax = 1e30f;
                int ax = -1, sgn = 0, miss = 0;
                for (int a = 0; a < 3; a++)
                {
                        float size = mo->piece[i].dims[a];
                        if (fabsf(Dl[a]) < 1e-9f)
                        {
                                if (Ol[a] < -pad || Ol[a] > size + pad) { miss = 1; break; }
                                continue;
                        }
                        float t0 = (-pad - Ol[a]) / Dl[a];
                        float t1 = (size + pad - Ol[a]) / Dl[a];
                        int s = Dl[a] > 0 ? 1 : -1;
                        if (t0 > t1) { float sw = t0; t0 = t1; t1 = sw; }
                        if (t0 > tmin) { tmin = t0; ax = a; sgn = s; }
                        if (t1 < tmax) tmax = t1;
                }
                if (miss || tmin > tmax || tmin < 0 || ax < 0 || tmin >= best_t)
                        continue;

                // entry axis + ray direction give the face (orient code)
                static const int face_of[3][2] = {  // [axis][dir > 0]
                        { EAST, WEST }, { DOWN, UP }, { NORTH, SOUTH } };
                int fc = face_of[ax][sgn > 0];

                // hit point -> texel, inverting pmodel.vert's uv mapping
                float h[3] = { Ol[0] + tmin * Dl[0], Ol[1] + tmin * Dl[1],
                               Ol[2] + tmin * Dl[2] };
                float sx = mo->piece[i].dims[0], sz = mo->piece[i].dims[2];
                float u, v;
                switch (fc) {
                case UP:    u = sx - h[0]; v = h[2]; break;
                case DOWN:  u = h[0];      v = h[2]; break;
                case EAST:  u = h[2];      v = h[1]; break;
                case WEST:  u = sz - h[2]; v = h[1]; break;
                case NORTH: u = sx - h[0]; v = h[1]; break;
                default:    u = h[0];      v = h[1]; break; // SOUTH
                }
                int eu, ev;
                pm_face_extent(&mo->piece[i], fc, &eu, &ev);
                best = i;
                best_t = tmin;
                *face = fc;
                *tu = ICLAMP((int)u, 0, eu - 1);
                *tv = ICLAMP((int)v, 0, ev - 1);
                if (hitp) { hitp[0] = h[0]; hitp[1] = h[1]; hitp[2] = h[2]; }
        }
        return best;
}

// a world point through the editor's view-projection to screen pixels
// (top-left origin, matching the cursor). Returns 0 behind the eye.
static int pmedit_to_screen(const float *w, float *sx, float *sy)
{
        float c[4];
        for (int r = 0; r < 4; r++)
                c[r] = pmedit_pv[r] * w[0] + pmedit_pv[r + 4] * w[1]
                     + pmedit_pv[r + 8] * w[2] + pmedit_pv[r + 12];
        if (c[3] <= 1e-4f) return 0;
        *sx = (c[0] / c[3] * 0.5f + 0.5f) * screenw;
        *sy = (c[1] / c[3] * 0.5f + 0.5f) * screenh;
        return 1;
}

#endif // BLOCKO_PMEDIT_RAYCAST_C_INCLUDED
