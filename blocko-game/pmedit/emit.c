#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_EMIT_C_INCLUDED
#define BLOCKO_PMEDIT_EMIT_C_INCLUDED

// pmedit/emit.c - build the preview's instances: pieces, outline hulls, gizmos, floor

// emit piece pc's highlight shell: the prism inflated by a thin membrane in
// a solid color, CAMERA-BACK faces only (an inverted hull) - the piece draws
// in front of all of it except a thin rim at its silhouette, and depth stays
// honest against every other piece (unlike the cleared-depth silhouette
// trick paint mode uses). M = the piece's px->world matrix. Returns the
// number of face instances emitted.
static int pmedit_shell(struct pmvert **bp, float *M, struct pm_piece *pc,
                int layer, float glow)
{
        float e = 0.15f;
        float t1[16], H[16];
        pm_mat_translate(t1, -e, -e, -e);
        mat4_multiply(H, M, t1);
        float d[3] = { pc->dims[0] + 2*e, pc->dims[1] + 2*e, pc->dims[2] + 2*e };
        // orient code order UP,EAST,NORTH,WEST,SOUTH,DOWN -> normal axis+sign
        static const signed char ax[PM_FACES] = {  1, 0, 2,  0,  2, 1 };
        static const signed char sn[PM_FACES] = { -1, 1, 1, -1, -1, 1 };
        // the pipeline back-face culls, so each back wall is emitted as the
        // OPPOSITE orient code translated onto it: the same quad, wound
        // facing the camera (the UVs differ, but the layer is solid color)
        static const signed char opp[PM_FACES] =
                        { DOWN, WEST, SOUTH, EAST, NORTH, UP };
        int n = 0;
        for (int fc = 0; fc < PM_FACES; fc++)
        {
                int a = ax[fc], s = sn[fc];
                float N[3] = { s * H[4*a], s * H[4*a+1], s * H[4*a+2] };
                float c[3] = { d[0]/2, d[1]/2, d[2]/2 };
                c[a] += s * d[a]/2;
                float P[4];
                mat4_f3_multiply(P, H, c[0], c[1], c[2]);
                if (N[0] * (P[0] - peye0) + N[1] * (P[1] - peye1)
                  + N[2] * (P[2] - peye2) <= 0) continue; // faces the camera
                float off[3] = { 0, 0, 0 }, t2[16], Hf[16];
                off[a] = s * d[a];
                pm_mat_translate(t2, off[0], off[1], off[2]);
                mat4_multiply(Hf, H, t2);
                *(*bp)++ = (struct pmvert){
                        .r0 = { Hf[0], Hf[4], Hf[8],  Hf[12] },
                        .r1 = { Hf[1], Hf[5], Hf[9],  Hf[13] },
                        .r2 = { Hf[2], Hf[6], Hf[10], Hf[14] },
                        .dims = { d[0], d[1], d[2] }, .orient = opp[fc],
                        .tex = layer, .illum = 0.f, .glow = glow,
                };
                n++;
        }
        return n;
}

// build the preview's instances into pmbuf: unselected pieces, then the white
// outline hull, then the selected piece. In paint/TYPE modes each group draws
// onto freshly cleared depth, so the selection always reads in full, in
// front; JOINT/SOCKET/PARENT share one honest depth buffer instead (piece
// overlaps read true and the gizmo pierces every surface exactly). Also
// refreshes the pick matrices and eases the center/zoom toward the selection.
struct pmvert *pmedit_emit(struct pmvert *b)
{
        struct pmodel *mo = &pm_models[my_player];
        float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16], root[16];

        // standing pose in the model frame: hitbox center at the origin,
        // facing the camera; NULL context = no animation. ANIMATE mode feeds
        // a synthetic walking-in-place context (full speed, phase advancing)
        // so the WALK style previews too, not just the FLAIL clock.
        static float anim_t;
        struct pm_anim an;
        if (pmedit_animate)
        {
                anim_t += 0.05f;
                an = (struct pm_anim){
                        .walk_phase = anim_t * 4.f,
                        .speed = 1,
                        // gentle look sweep so HEAD tracking shows off
                        .look_yaw = sinf(anim_t * 0.5f) * 0.8f,
                        .look_pitch = sinf(anim_t * 0.3f) * 0.35f,
                        // canned wobble so JIGGLE pieces preview too
                        .bounce = sinf(anim_t * 2.5f) * 0.3f,
                        .tail_phase = anim_t * 2.f,
                        .t = anim_t,
                        .style = mo->style,
                };
        }
        // the test resting pose shows ONLY here (RESTING ANGLE mode and the
        // ANIMATE preview) - the flag drops right back so the in-world
        // resolves never pose with it
        pm_rest_apply = pmedit_restang || pmedit_animate;
        pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                        -camplayer.yaw, pmedit_animate ? &an : NULL,
                        space, geom, root);
        pmedit_nr = mo->nr_pieces;
        if (pmedit_sel >= pmedit_nr) pmedit_sel = -1;

        // frame the STANDING pose even while animating, so the zoom-to-fit
        // doesn't pump with the swinging limbs
        float sspace[PM_MAX_PIECES][16], sgeom[PM_MAX_PIECES][16];
        float (*bgeom)[16] = geom;
        if (pmedit_animate)
        {
                pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                                -camplayer.yaw, NULL, sspace, sgeom, NULL);
                bgeom = sgeom;
        }
        pm_rest_apply = 0;

        // center of interest + radius: the selected prism's bounding box
        // (center = its corner average), plus its parent's in SOCKET mode,
        // or the whole model's
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int spar = pmedit_socket && pmedit_sel >= 0 ? mo->piece[pmedit_sel].parent : -1;
        for (int i = 0; i < pmedit_nr; i++)
        {
                // frame what the main view shows: ANIMATE the whole moving
                // model; PARENT the eligible parents; otherwise the
                // selection (plus its parent in SOCKET)
                if (pmedit_sel >= 0 && !pmedit_animate)
                {
                        if (pmedit_parent)
                        {
                                if (pmedit_cycle(i))
                                        continue;
                        }
                        else if (i != pmedit_sel && i != spar)
                                continue;
                }
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, bgeom[i],
                                (k      & 1) * mo->piece[i].dims[0],
                                (k >> 1 & 1) * mo->piece[i].dims[1],
                                (k >> 2 & 1) * mo->piece[i].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo[a]) lo[a] = p[a];
                                if (p[a] > hi[a]) hi[a] = p[a];
                        }
                }
        }
        // PLACE ATTACHMENT POINT: the resting gizmo sits at the point, which
        // nudges can push outside the prism - keep it in frame too
        if (pmedit_joint && pmedit_sel >= 0 && !pmedit_animate)
        {
                unsigned char *o = mo->piece[pmedit_sel].origin;
                float p[4];
                mat4_f3_multiply(p, space[pmedit_sel], o[0], o[1], o[2]);
                for (int a = 0; a < 3; a++)
                {
                        if (p[a] < lo[a]) lo[a] = p[a];
                        if (p[a] > hi[a]) hi[a] = p[a];
                }
        }
        // nothing framable (PARENT with no eligible parents): the whole model
        if (lo[0] > hi[0]) for (int i = 0; i < pmedit_nr; i++)
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, bgeom[i],
                                (k      & 1) * mo->piece[i].dims[0],
                                (k >> 1 & 1) * mo->piece[i].dims[1],
                                (k >> 2 & 1) * mo->piece[i].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo[a]) lo[a] = p[a];
                                if (p[a] > hi[a]) hi[a] = p[a];
                        }
                }
        float cen[3] = { (lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2 };
        float radius = 0.5f * sqrtf((hi[0]-lo[0]) * (hi[0]-lo[0])
                                  + (hi[1]-lo[1]) * (hi[1]-lo[1])
                                  + (hi[2]-lo[2]) * (hi[2]-lo[2]));

        // the editor's own projection: a narrow FOV so the model reads
        // nearly orthographic (built here, before the render and the pick
        // rays both use it, so everything stays consistent)
        float ed_near = 100.f, ed_far = 1000.f * BS;
        float ed_frustw = ed_near * tanf(PMEDIT_FOV * PI / 360.f);
        float ed_frusth = ed_frustw * screenh / screenw;
        float pj[16] = {
                ed_near / ed_frustw,  0, 0, 0,
                0, -ed_near / ed_frusth,  0, 0,
                0, 0,        -ed_far / (ed_far - ed_near), -1,
                0, 0, -(ed_far * ed_near) / (ed_far - ed_near),  0,
        };
        memcpy(pmedit_proj, pj, sizeof pj);

        // proj * the leveled view (what pmedit_render draws with), kept for
        // projecting world points to the screen - the resize drag needs it
        {
                float fv[3], view[16];
                lookit(view, fv, peye0, peye1, peye2, 0, camplayer.yaw);
                translate(view, -peye0, -peye1, -peye2);
                mat4_multiply(pmedit_pv, pmedit_proj, view);
        }

        // zoom to fit: tan(half-fov) straight from the editor frustum
        float tanw = ed_frustw / ed_near;
        float tanh_ = ed_frusth / ed_near;
        float fill = pmedit_sel < 0 || pmedit_parent || pmedit_animate
                        ? 0.9f : 0.6f; // radius / half-extent
        // small pieces don't zoom in crazy far: floor the radius the fit
        // distance is computed from, so a tiny piece just sits smaller and
        // centered (the bbox framing above still uses the true size)
        float fit_radius = radius;
        if (pmedit_sel >= 0 && !pmedit_parent && !pmedit_animate)
        {
                float floor_r = PMEDIT_MIN_RADIUS_PX * PM_SCALE;
                if (fit_radius < floor_r) fit_radius = floor_r;
        }
        float dist = fit_radius / (fill * MIN(tanw, tanh_));
        if (dist < radius + 0.3f * BS) dist = radius + 0.3f * BS; // keep off the near plane

        if (pmedit_snap)
        {
                memcpy(pmedit_cen, cen, sizeof cen);
                pmedit_dist = dist;
                pmedit_snap = 0;
        }
        else
        {
                // fly to a selected piece fast; drift back out gently
                float ease = pmedit_sel < 0 ? 0.2f : 0.6f;
                for (int a = 0; a < 3; a++)
                        pmedit_cen[a] += (cen[a] - pmedit_cen[a]) * ease;
                pmedit_dist += (dist - pmedit_dist) * ease;
        }

        // the editor's own camera: the eye, leveled (pitch 0), so the model
        // always opens dead ahead at eye level and the turntable's +-89 deg
        // means what it says - no matter where the player was looking
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw);

        // whole-preview transform: put the eased center pmedit_dist in front
        // of the eye, turntable-rotated about it (pitch spins on screen-right)
        float R[16], t1[16], t2[16], E[16];
        pm_mat_axis(t1, vm[0], vm[4], vm[8], pmedit_mpitch);
        pm_mat_yaw(t2, pmedit_myaw);
        mat4_multiply(R, t1, t2);
        pm_mat_translate(t1, -pmedit_cen[0], -pmedit_cen[1], -pmedit_cen[2]);
        mat4_multiply(t2, R, t1);
        pm_mat_translate(t1, peye0 + f[0] * pmedit_dist,
                             peye1 + f[1] * pmedit_dist,
                             peye2 + f[2] * pmedit_dist);
        mat4_multiply(E, t1, t2);

        for (int i = 0; i < pmedit_nr; i++)
                mat4_multiply(pmedit_mats[i], E, geom[i]);

        // SELECT PARENT: the active piece moves to its own little view on
        // the left - same turntable spin, zoomed to the lonely piece - while
        // the main view keeps only the pieces that could become its parent.
        // Overwriting its pick matrix means clicks find it where it appears.
        float E2[16];
        if (pmedit_parent && pmedit_sel >= 0)
        {
                float lo2[3] = {1e9f, 1e9f, 1e9f}, hi2[3] = {-1e9f, -1e9f, -1e9f};
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, geom[pmedit_sel],
                                (k      & 1) * mo->piece[pmedit_sel].dims[0],
                                (k >> 1 & 1) * mo->piece[pmedit_sel].dims[1],
                                (k >> 2 & 1) * mo->piece[pmedit_sel].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo2[a]) lo2[a] = p[a];
                                if (p[a] > hi2[a]) hi2[a] = p[a];
                        }
                }
                float cen2[3] = { (lo2[0]+hi2[0])/2, (lo2[1]+hi2[1])/2,
                                  (lo2[2]+hi2[2])/2 };
                float rad2 = 0.5f * sqrtf((hi2[0]-lo2[0]) * (hi2[0]-lo2[0])
                                        + (hi2[1]-lo2[1]) * (hi2[1]-lo2[1])
                                        + (hi2[2]-lo2[2]) * (hi2[2]-lo2[2]));
                float dist2 = rad2 / (0.28f * MIN(tanw, tanh_));
                if (dist2 < rad2 + 0.3f * BS) dist2 = rad2 + 0.3f * BS;
                float offx = -0.62f * tanw * dist2; // ~62% left of center
                // waiting for a parent, the piece hovers: a gentle bob, half
                // a texel of total travel at 1Hz, so it reads as unattached
                float bob = 0.25f * PM_SCALE * sinf(TAU * pmedit_bob_t);
                float q1[16], q2[16];
                pm_mat_translate(q1, -cen2[0], -cen2[1], -cen2[2]);
                mat4_multiply(q2, R, q1);
                pm_mat_translate(q1, peye0 + f[0] * dist2 + vm[0] * offx,
                                     peye1 + f[1] * dist2 + vm[4] * offx + bob,
                                     peye2 + f[2] * dist2 + vm[8] * offx);
                mat4_multiply(E2, q1, q2);
                mat4_multiply(pmedit_mats[pmedit_sel], E2, geom[pmedit_sel]);
        }

        // one prism face instance; every piece draws at full, glow-heavy
        // brightness so paint colors read the same day or night, and
        // unselected pieces stay color-matchable against the selection
        #define PM_EDIT_FACE(M, dx, dy, dz, layer, il, gl) (struct pmvert){ \
                .r0 = { (M)[0], (M)[4], (M)[8],  (M)[12] },                 \
                .r1 = { (M)[1], (M)[5], (M)[9],  (M)[13] },                 \
                .r2 = { (M)[2], (M)[6], (M)[10], (M)[14] },                 \
                .dims = { dx, dy, dz }, .orient = fc + 1,                   \
                .tex = (layer), .illum = (il), .glow = (gl) }

        // buffer order = draw order: unselected pieces, outline hull, selection
        pm_edit_rest_start = b - pmbuf;
        for (int i = 0; i < pmedit_nr; i++)
        {
                if (i == pmedit_sel) continue;
                // the ANIMATE preview blinks its EYES pieces like the game
                // does (the selected piece stays visible - you're editing it)
                if (pmedit_animate && mo->piece[i].type == PM_T_EYES
                                && pm_blinking(my_player, anim_t))
                        continue;
                // HIDDEN pieces and pieces the active gizmo mode ignores draw
                // faint in the ghost group below instead of solid here
                if (pmedit_hidden >> i & 1 || pmedit_mode_faint(mo, i)) continue;
                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(pmedit_mats[i],
                                mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers() + i * PM_FACES + fc,
                                0.9f, 0.8f);
        }
        pm_edit_rest_count = (int)(b - pmbuf) - pm_edit_rest_start;

        // faint pieces, 90% see-through on the same depth as the rest of the
        // model: the HIDDEN set (dead to clicks until UNHIDE) plus whatever the
        // active gizmo mode sets aside - kept as spatial reference, not gone
        pm_edit_hide_start = pm_edit_rest_start + pm_edit_rest_count;
        for (int i = 0; i < pmedit_nr; i++)
        {
                if (i == pmedit_sel) continue;
                if (pmedit_animate && mo->piece[i].type == PM_T_EYES
                                && pm_blinking(my_player, anim_t))
                        continue;
                if (!(pmedit_hidden >> i & 1 || pmedit_mode_faint(mo, i)))
                        continue;
                for (int fc = 0; fc < PM_FACES; fc++)
                {
                        *b = PM_EDIT_FACE(pmedit_mats[i],
                                mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers() + i * PM_FACES + fc,
                                0.9f, 0.8f);
                        (b++)->tint = -0.1f; // ghost pipe reads alpha 0.1
                }
        }
        pm_edit_hide_count = (int)(b - pmbuf) - pm_edit_hide_start;

        // selection outline: the selected prism again, inflated by a pixel
        // membrane, in solid white (the extra layer past every slot's tiles).
        // Paint/TYPE modes draw it onto freshly cleared depth and the piece
        // over it, so the fatter silhouette shows as a rim around the piece;
        // JOINT/SOCKET use the depth-honest inverted hull instead, white on
        // the selection plus pink on the parent in SOCKET mode.
        pm_edit_hull_start = pm_edit_hide_start + pm_edit_hide_count;
        pm_edit_hull_count = 0;
        pm_edit_sel_count = 0;
        // MOVE PART: while the cursor is on the parent, the piece (rim and
        // all) leaves its current spot and shows as a see-through preview at
        // the exact flush spot a click would lay it (emitted with the gizmo
        // group below); off the parent it sits where it is, solid, and the
        // arrow keys nudge it. A root-parented piece has no parent surface,
        // so it always stays put.
        int sock_att[3];
        int sock_hot = pmedit_socket && pmedit_sel >= 0
                && pmedit_sock_aim(&mo->piece[pmedit_sel], sock_att);
        if (pmedit_sel >= 0)
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                if (pmedit_joint || pmedit_socket)
                {
                        if (!sock_hot)
                                pm_edit_hull_count = pmedit_shell(&b,
                                        pmedit_mats[pmedit_sel], pc,
                                        PM_LAYER_WHITE, 3.f); // reads white
                        if (pmedit_socket && pc->parent >= 0)
                                pm_edit_hull_count += pmedit_shell(&b,
                                        pmedit_mats[(int)pc->parent],
                                        &mo->piece[(int)pc->parent],
                                        PM_LAYER_PINK, 1.2f);
                }
                else if (pmedit_parent)
                {
                        // the piece sits alone in the left view wearing the
                        // white outline; pink candidate rims ride the gizmo
                        // group
                        pm_edit_hull_count = pmedit_shell(&b,
                                pmedit_mats[pmedit_sel], pc,
                                PM_LAYER_WHITE, 3.f); // reads white
                }
                else
                {
                        float e = 0.15f; // px of rim on every side - thin, so
                                         // the shell doesn't cover neighbors
                                         // where pieces touch
                        float t1[16], H[16];
                        pm_mat_translate(t1, -e, -e, -e);
                        mat4_multiply(H, pmedit_mats[pmedit_sel], t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(H,
                                        pc->dims[0] + 2*e, pc->dims[1] + 2*e,
                                        pc->dims[2] + 2*e,
                                        PM_LAYER_WHITE, 0.f, 3.f); // reads white
                        pm_edit_hull_count = PM_FACES;
                }

                if (!sock_hot)
                {
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(pmedit_mats[pmedit_sel],
                                        pc->dims[0], pc->dims[1], pc->dims[2],
                                        pmodel_tex_base + my_player * pm_slot_layers()
                                                + pmedit_sel * PM_FACES + fc,
                                        0.9f, 0.8f);
                        pm_edit_sel_count = PM_FACES;
                }
        }
        pm_edit_sel_start = pm_edit_hull_start + pm_edit_hull_count;

        // JOINT gizmo: the rotation origin (in the piece's 16^3 SPACE - it
        // isn't corner-relative like the prism geometry) as a half-texel
        // cube in a cycling hue, with counter-cycling ruled axes running out
        // of its center in all 6 directions. MOVE PART draws no gizmo: the
        // see-through preview piece rides the cursor on the parent instead
        // (it lives in this group so the ghost pipeline can draw it last).
        pm_edit_joint_start = pm_edit_sel_start + pm_edit_sel_count;
        pm_edit_joint_count = 0;
        if (pmedit_sel >= 0)
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                float t1[16], G[16];
                // the arrow-key nudge steers by this frame in every mode,
                // including the plain selected view's MOVE PART-style nudge
                if (pmedit_joint)
                        mat4_multiply(pmedit_gizmo_mat, E, space[pmedit_sel]);
                else if (pc->parent >= 0)
                        mat4_multiply(pmedit_gizmo_mat, E, space[(int)pc->parent]);
                else
                        mat4_multiply(pmedit_gizmo_mat, E, root);

                if (pmedit_joint)
                {
                        // the gizmo (cube + ruled axes) rides the cursor
                        // whenever it's on the piece - showing exactly where
                        // a click would plant the point - and rests at the
                        // point itself otherwise (arrow nudges move it there)
                        int gface, gtu, gtv;
                        float gh[3];
                        int pt[3] = { pc->origin[0], pc->origin[1], pc->origin[2] };
                        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f,
                                        &gface, &gtu, &gtv, gh) >= 0)
                                for (int a = 0; a < 3; a++)
                                        pt[a] = ICLAMP((int)roundf(pc->corner[a] + gh[a]),
                                                        0, PM_TILE);
                        int hue = pframe / 10 % PM_NR_HUES;

                        float c = 0.65f;
                        pm_mat_translate(t1, pt[0] - c/2, pt[1] - c/2, pt[2] - c/2);
                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(G, c, c, c, PM_LAYER_HUES + hue, 0.f, 3.f);
                        pm_edit_joint_count = PM_FACES;

                        // the axes come as 1-texel segments alternating bright
                        // and dim, pinned to the piece space's texel grid, so
                        // distances read at a glance like a ruler
                        for (int a = 0; a < 3; a++)
                                for (int s = -20; s < 20; s++)
                                {
                                        float dims[3] = { 0.1f, 0.1f, 0.1f };
                                        dims[a] = 1.f;
                                        float off[3] = { pt[0] - 0.05f,
                                                pt[1] - 0.05f, pt[2] - 0.05f };
                                        off[a] = pt[a] + s;
                                        pm_mat_translate(t1, off[0], off[1], off[2]);
                                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                                        for (int fc = 0; fc < PM_FACES; fc++)
                                                *b++ = PM_EDIT_FACE(G,
                                                        dims[0], dims[1], dims[2],
                                                        PM_LAYER_HUES + (hue + 3) % PM_NR_HUES,
                                                        0.f, (pt[a] + s) & 1 ? 3.f : 1.4f);
                                        pm_edit_joint_count += PM_FACES;
                                }
                }
                else if (sock_hot)
                {
                        // MOVE PART preview: the piece itself, half see-
                        // through, laid flush against the hovered parent face
                        // exactly where a click would put it (the transforms
                        // are all translations, so they collapse into one)
                        pm_mat_translate(t1,
                                sock_att[0] - pc->origin[0] + pc->corner[0],
                                sock_att[1] - pc->origin[1] + pc->corner[1],
                                sock_att[2] - pc->origin[2] + pc->corner[2]);
                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(G,
                                        pc->dims[0], pc->dims[1], pc->dims[2],
                                        pmodel_tex_base + my_player * pm_slot_layers()
                                                + pmedit_sel * PM_FACES + fc,
                                        0.9f, 0.8f);
                                (b++)->tint = -0.5f; // ghost pipe reads alpha 0.5
                        }
                        pm_edit_joint_count += PM_FACES;
                }
                else if (pmedit_resize && pmedit_face)
                {
                        // RESIZE: the picked face wears a see-through slab in
                        // a cycling hue - the paint reads through it - so you
                        // can see what the arrows will push and pull
                        int fax = pmedit_axis_of[pmedit_face];
                        int hue = pframe / 10 % PM_NR_HUES;
                        float e = 0.15f, th = 0.2f;
                        float off[3] = { -e, -e, -e };
                        float dims[3] = { pc->dims[0] + 2*e, pc->dims[1] + 2*e,
                                          pc->dims[2] + 2*e };
                        dims[fax] = th;
                        off[fax] = pmedit_side_of[pmedit_face] > 0
                                        ? pc->dims[fax] + 0.02f : -th - 0.02f;
                        pm_mat_translate(t1, off[0], off[1], off[2]);
                        mat4_multiply(G, pmedit_mats[pmedit_sel], t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(G, dims[0], dims[1], dims[2],
                                        PM_LAYER_HUES + hue, 0.f, 3.f);
                                (b++)->tint = -0.45f; // ghost pipe: alpha 0.45
                        }
                        pm_edit_joint_count += PM_FACES;
                }
        }

        // PARENT mode: no gizmo, no ghost - just a pink rim on the eligible
        // parent under the cursor; the click re-wires to it and lays the
        // piece flush right there
        if (pmedit_sel >= 0 && pmedit_parent)
        {
                int gface, gtu, gtv;
                float gh[3];
                int hov = pmedit_pick(pmedit_mx, pmedit_my,
                                PMEDIT_PICK_ELIG, 0.f,
                                &gface, &gtu, &gtv, gh);
                if (hov >= 0)
                        pm_edit_joint_count += pmedit_shell(&b,
                                pmedit_mats[hov], &mo->piece[hov],
                                PM_LAYER_PINK, 1.2f);
        }

        // ground reference: one block's top face as a half-transparent dark
        // green quad at the in-game ground level (the feet), so a floating
        // or sunken model reads at a glance. Drawn after the model, blended
        // on its depth: a sunken model shows the floor slicing through it.
        // Only in the views that show the whole model, and only near level -
        // steep pitches would turn it into a big screen-covering sheet. MOVE
        // PART on a root-parented piece has no parent surface to reference, so
        // it borrows the hitbox wireframe (below) as a nudging frame
        pm_edit_floor_start = pm_edit_joint_start + pm_edit_joint_count;
        pm_edit_floor_count = 0;
        int floor_view = !pmedit_joint && !pmedit_socket && !pmedit_parent;
        int nudge_ref = pmedit_socket && pmedit_sel >= 0
                        && mo->piece[pmedit_sel].parent < 0;
        if (floor_view || nudge_ref)
        {
                if (floor_view && fabsf(pmedit_mpitch) <= TAU / 12)
                {
                        float ft[16], FG[16];
                        pm_mat_translate(ft, -BS / 2.f, PLYR_H / 2.f, -BS / 2.f);
                        mat4_multiply(FG, E, ft);
                        int fc = 0; // the UP face only: a single quad
                        *b = PM_EDIT_FACE(FG, BS, 0, BS, PM_LAYER_DKGREEN, 0.f, 3.f);
                        (b++)->tint = -0.5f; // ghost pipe reads alpha 0.5
                        pm_edit_floor_count = 1;
                }

                // hitbox wireframe: 12 thin bars tracing the player's
                // PLYR_W x PLYR_H x PLYR_W collision box in the same
                // see-through dark green as the floor. The box is square in
                // plan, so z reuses the x numbers; the y and z bars are
                // inset one thickness so no two bars share volume (which
                // would double-blend)
                const float T = 24.f;
                const float cx = -PLYR_W / 2.f, cy = -PLYR_H / 2.f;
                const float X = PLYR_W - T, Y = PLYR_H - T;
                const struct { float p[3], d[3]; } bar[12] = {
                        {{ cx,     cy,     cx     }, { PLYR_W, T, T }},
                        {{ cx,     cy,     cx + X }, { PLYR_W, T, T }},
                        {{ cx,     cy + Y, cx     }, { PLYR_W, T, T }},
                        {{ cx,     cy + Y, cx + X }, { PLYR_W, T, T }},
                        {{ cx,     cy + T, cx     }, { T, PLYR_H - 2 * T, T }},
                        {{ cx,     cy + T, cx + X }, { T, PLYR_H - 2 * T, T }},
                        {{ cx + X, cy + T, cx     }, { T, PLYR_H - 2 * T, T }},
                        {{ cx + X, cy + T, cx + X }, { T, PLYR_H - 2 * T, T }},
                        {{ cx,     cy,     cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx,     cy + Y, cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx + X, cy,     cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx + X, cy + Y, cx + T }, { T, T, PLYR_W - 2 * T }},
                };
                for (int e = 0; e < 12; e++)
                {
                        float bt[16], BM[16];
                        pm_mat_translate(bt, bar[e].p[0], bar[e].p[1], bar[e].p[2]);
                        mat4_multiply(BM, E, bt);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(BM,
                                        bar[e].d[0], bar[e].d[1], bar[e].d[2],
                                        PM_LAYER_DKGREEN, 0.f, 3.f);
                                (b++)->tint = -0.5f; // matches the floor
                        }
                }
                pm_edit_floor_count += 12 * PM_FACES;
        }
        #undef PM_EDIT_FACE

        polys += b - pmbuf - pm_edit_rest_start;
        return b;
}

#endif // BLOCKO_PMEDIT_EMIT_C_INCLUDED
