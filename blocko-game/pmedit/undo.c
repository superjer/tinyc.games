#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_UNDO_C_INCLUDED
#define BLOCKO_PMEDIT_UNDO_C_INCLUDED

// pmedit/undo.c - session snapshots on disk + the in-memory undo/redo ring

// in-memory undo/redo: a ring of whole-struct snapshots (~15.8KB each, the
// wire size). 128 deep is ~2MiB and far more history than a session needs.
// Commit-on-settle (pmedit_undo_track): a snapshot lands when the model
// stops changing, so a paint drag is one entry, not one per texel. Undo/redo
// restore LOCALLY only - peers see the model once, on editor exit.
#define PMEDIT_UNDO_CAP 128              // power of two
#define PMEDIT_UNDO_MASK (PMEDIT_UNDO_CAP - 1)
static struct pmodel pmedit_undo_ring[PMEDIT_UNDO_CAP];
static int pmedit_undo_next;             // one past the newest snapshot index
static int pmedit_undo_cur;              // index of the state on screen
static int pmedit_undo_oldest;           // oldest index still in the ring
static unsigned pmedit_undo_sum;         // checksum of ring[pmedit_undo_cur]
static unsigned pmedit_undo_prev;        // checksum last frame (settle detector)
static float pmedit_undo_cool;           // debounce: one NEW snapshot per window;
                                         // faster edits fold into the current one
#define PMEDIT_UNDO_DEBOUNCE 0.5f        // seconds between fresh snapshots

// history autosave: while the editor is open, any change to the model
// (checksummed, so paint, geometry, TYPE, STYLE, rest angles all count)
// lands in this session's numbered snapshot, debounced to one write a
// second; closing writes the tail regardless of the clock. The first
// change claims the next free number, later writes overwrite it, and the
// number is dropped at close - a closed session's file is never touched
static void pmedit_hist_save(int final)
{
        if (!final && pmedit_hist_cool > 0) return;
        unsigned sum = pm_checksum(&pm_models[my_player]);
        if (sum == pmedit_hist_sum) return;
        if (!pmedit_hist_n)
        {
                pmedit_hist_n = pm_hist_newest() + 1;
                fprintf(stderr, "pmodel: session snapshot %s/%05d.model\n",
                        pm_hist_dir, pmedit_hist_n);
        }
        pm_hist_write(pmedit_hist_n);
        pmedit_hist_sum = sum;
        pmedit_hist_cool = 1;
}

// start the undo timeline over with the current model as snapshot 0 (on open)
static void pmedit_undo_reset(void)
{
        pmedit_undo_ring[0] = pm_models[my_player];
        pmedit_undo_next = 1;
        pmedit_undo_cur = 0;
        pmedit_undo_oldest = 0;
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
        pmedit_undo_cool = 0;
}

// fold the live model into a new snapshot after pmedit_undo_cur, dropping any
// redo tail and aging out the oldest entry once the ring is full
static void pmedit_undo_commit(void)
{
        int i = pmedit_undo_cur + 1;
        pmedit_undo_ring[i & PMEDIT_UNDO_MASK] = pm_models[my_player];
        pmedit_undo_cur = i;
        pmedit_undo_next = i + 1;
        if (pmedit_undo_next - pmedit_undo_oldest > PMEDIT_UNDO_CAP)
                pmedit_undo_oldest = pmedit_undo_next - PMEDIT_UNDO_CAP;
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
}

// restore ring[cur] into the live model and refresh its texture layers. LOCAL
// ONLY: no pmodel_send_mine here - peers learn the final model on editor exit
static void pmedit_undo_restore(void)
{
        pm_models[my_player] = pmedit_undo_ring[pmedit_undo_cur & PMEDIT_UNDO_MASK];
        pmodel_upload(my_player);
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
        pmedit_undo_cool = 0; // the next edit starts a fresh snapshot, not a fold
}

static void pmedit_undo(void)
{
        if (pmedit_undo_cur <= pmedit_undo_oldest) return;
        pmedit_undo_cur--;
        pmedit_undo_restore();
}

static void pmedit_redo(void)
{
        if (pmedit_undo_cur + 1 >= pmedit_undo_next) return;
        pmedit_undo_cur++;
        pmedit_undo_restore();
}

// every frame: once the model changes and then holds still for a frame, record
// it. A fresh snapshot only every PMEDIT_UNDO_DEBOUNCE seconds; edits faster
// than that (a stream of nudges) fold into the current snapshot in place, so a
// rapid burst is one undo step, not one per nudge - and the ring can't fill in
// a second. The first edit after opening or an undo commits immediately (cool
// is 0), keeping undo responsive
static void pmedit_undo_track(float dt)
{
        if (pmedit_undo_cool > 0) pmedit_undo_cool -= dt;
        unsigned sum = pm_checksum(&pm_models[my_player]);
        if (sum == pmedit_undo_sum) { pmedit_undo_prev = sum; return; } // clean
        if (sum != pmedit_undo_prev) { pmedit_undo_prev = sum; return; } // moving
        if (pmedit_undo_cool > 0)
        {
                // within the window: overwrite the current snapshot with the
                // latest state so the whole burst collapses to one undo step
                pmedit_undo_ring[pmedit_undo_cur & PMEDIT_UNDO_MASK] =
                        pm_models[my_player];
                pmedit_undo_sum = sum;
        }
        else
        {
                pmedit_undo_commit();
                pmedit_undo_cool = PMEDIT_UNDO_DEBOUNCE;
        }
}

#endif // BLOCKO_PMEDIT_UNDO_C_INCLUDED
