#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_PIECE_C_INCLUDED
#define BLOCKO_PMEDIT_PIECE_C_INCLUDED

// pmedit/piece.c - piece operations: parenting, flush placement, copy, delete

static const char *pmedit_type_name[PM_T_COUNT] = {
        [PM_T_FIXED] = "FIXED",   [PM_T_TORSO] = "TORSO",
        [PM_T_HEAD]  = "HEAD",    [PM_T_ARM_L] = "LEFT ARM",
        [PM_T_ARM_R] = "RIGHT ARM", [PM_T_LEG1] = "LEG 1",
        [PM_T_LEG2]  = "LEG 2",   [PM_T_TAIL]  = "TAIL",
        [PM_T_JIGGLE] = "JIGGLE", [PM_T_EYES]  = "EYES",
};

// would re-parenting the selection to piece j loop the chain?
static int pmedit_cycle(int j)
{
        int hops = 0;
        while (j >= 0 && hops++ <= PM_MAX_PIECES)
        {
                if (j == pmedit_sel) return 1;
                j = pm_models[my_player].piece[j].parent;
        }
        return 0;
}

// piece i plays no part in the active gizmo mode: shown 90% see-through as a
// spatial reference rather than vanishing. PLACE ATTACHMENT POINT (joint)
// isolates the selected piece; MOVE PART (socket) keeps its parent solid too;
// SELECT PARENT (parent) keeps the eligible parents solid. The selected piece
// and mode-off views are never faint (return 0)
static int pmedit_mode_faint(struct pmodel *mo, int i)
{
        if (pmedit_sel < 0 || i == pmedit_sel) return 0;
        if (pmedit_joint)  return 1;
        if (pmedit_socket) return i != mo->piece[pmedit_sel].parent;
        if (pmedit_parent) return pmedit_cycle(i);
        return 0;
}

// face orient code (1..6) -> its normal's axis and sign
static const signed char pmedit_axis_of[7] = { 0, 1, 0, 2, 0, 2, 1 };
static const signed char pmedit_side_of[7] = { 0, -1, 1, 1, -1, -1, 1 };

// NEW PART: snap the joint to the middle of the piece's side FACING the
// parent (the opposite side from the parent face the socket is going on),
// so the new piece hangs outward from wherever it gets attached
static void pmedit_newpart_snap(struct pm_piece *pc, int face)
{
        int a = pmedit_axis_of[face], s = pmedit_side_of[face];
        for (int q = 0; q < 3; q++)
                pc->origin[q] = pc->corner[q] + pc->dims[q] / 2;
        pc->origin[a] = s > 0 ? pc->corner[a]
                              : pc->corner[a] + pc->dims[a];
}

// the attach point that puts pc flush against piece P's hovered face at the
// picked point h (P-local px, corner-relative) - the attach moves, the joint
// never does. The flush shift can push the attach past the parent's 16^3
// tile; that's fine, it's just a translation.
static void pmedit_flush_att(struct pm_piece *pc, int P, int face,
                float *h, int att[3])
{
        unsigned char *corner = pm_models[my_player].piece[P].corner;
        for (int q = 0; q < 3; q++)
                att[q] = ICLAMP((int)roundf(corner[q] + h[q]), 0, PM_TILE);
        int a = pmedit_axis_of[face];
        att[a] += pc->origin[a] - pc->corner[a]
                - (pmedit_side_of[face] < 0 ? pc->dims[a] : 0);
        att[a] = ICLAMP(att[a], 0, 2 * PM_TILE);
}

// MOVE PART: where would a click lay the piece? Picks the parent surface
// under the cursor and, on a hit, computes the flush attach point there.
// Returns whether the cursor is on the parent at all.
static int pmedit_sock_aim(struct pm_piece *pc, int att[3])
{
        int face, tu, tv;
        float h[3];
        if (pc->parent < 0 || pmedit_pick(pmedit_mx, pmedit_my,
                        (int)pc->parent, 0.f, &face, &tu, &tv, h) < 0)
                return 0;
        pmedit_flush_att(pc, (int)pc->parent, face, h, att);
        return 1;
}

// backing out of a NEW PART placement cancels it: the unplaced piece goes
// away with the mode (it was never attached to anything)
static void pmedit_newpart_cancel()
{
        if (!pmedit_newpart) return;
        pmedit_newpart = 0;
        if (pmedit_sel >= 0 &&
                        pm_piece_delete(&pm_models[my_player], pmedit_sel))
                pmodel_upload(my_player);
}

// MAKE COPY (the button, or ctrl-C): a full clone of the selection - prism,
// joint, paint, type - then straight into the parent-picking flow to hang it
// somewhere, exactly like NEW PART (backing out cancels the copy). Unlike
// NEW PART the joint stays the original's, so it swings the same
static void pmedit_copy()
{
        struct pmodel *mo = &pm_models[my_player];
        // no copying a piece that isn't placed yet
        if (pmedit_sel < 0 || pmedit_newpart
                        || mo->nr_pieces >= PM_MAX_PIECES)
                return;
        int i = mo->nr_pieces++;
        mo->piece[i] = mo->piece[pmedit_sel];
        mo->piece[i].parent = -1;
        memcpy(mo->texel[i], mo->texel[pmedit_sel], sizeof mo->texel[i]);
        pmodel_upload(my_player);
        pmedit_sel = i;
        pmedit_parent = 1;
        pmedit_newpart = 2;
        pmedit_joint = pmedit_socket = 0;
        pmedit_resize = pmedit_face = pmedit_restang = 0;
        pmedit_hide = 0;
}

// DELETE (the button, or the Del/Backspace keys): the piece and its whole
// subtree go away (no undo yet); the survivors' indices compact down
static void pmedit_delete()
{
        if (pmedit_sel < 0
                        || !pm_piece_delete(&pm_models[my_player], pmedit_sel))
                return;
        pmedit_sel = -1;
        pmedit_joint = pmedit_socket = pmedit_parent = 0;
        pmedit_resize = pmedit_face = pmedit_newpart = pmedit_restang = 0;
        pmedit_hidden = 0; // survivors' indices shifted
        pmodel_upload(my_player);
}

#endif // BLOCKO_PMEDIT_PIECE_C_INCLUDED
