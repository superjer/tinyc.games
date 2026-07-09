#include "blocko.c"
#ifndef BLOCKO_FORMATIONS_C_INCLUDED
#define BLOCKO_FORMATIONS_C_INCLUDED

// formations.c - overhanging rock via "carve the scaffold"
//
// 3D density noise alone makes overhangs but also floating islands. Instead
// each large 2D cell hashes to at most one formation built as a small voxel
// blob, in four stages:
//
//   1. scaffold  - a connected random-walk of spheres, fused into a smooth
//                  metaball field (no bead-necklace look), planted so its base
//                  sinks below the terrain at an anchor column.
//   2. carve     - subtract 3D noise: blob-bites gouge concave bays, ridged
//                  veins cut cracks/tunnels/arches. This is what turns a smooth
//                  lump into eroded rock.
//   3. detail    - add a thin fbm shell for multi-scale surface grit.
//   4. flood     - flood-fill from the buried anchor and keep ONLY that
//                  component, so carving can never leave a floater and the
//                  whole blob is grounded by construction.
//
// The finished blob is stored as per-column vertical spans (world Y), and the
// chunker asks each column for its solid spans (form_spans) and stamps stone,
// capping exposed tops with the surrounding soil bands.
//
// Everything derives from noise_hash(cell, world_seed): deterministic across
// platforms and threads, same guarantees as taylor noise.

#define FORM_CELL       64      // one formation max per cell of this size
#define FORM_CELL_BITS  6

// local voxel box for one formation (1 voxel = 1 block). The footprint must
// stay < FORM_CELL so a 3x3-cell query from any covered column finds it.
#define FBW  52                 // footprint width  (x)
#define FBD  52                 // footprint depth  (z)
#define FBH  64                 // vertical voxels  (y, local up = world up)
#define FB_GY 10                // local row of the anchor's ground plane
#define FB_VOL (FBW*FBH*FBD)
#define FVIDX(i,j,k) (((i)*FBH+(j))*FBD+(k))

#define FORM_MAX_SPHERES 96     // scaffold spheres (also used by `form near`)
#define FORM_SPANMAX     8000   // per-formation column-span arena

int   form_enable = 1;
float form_region = .56f;       // region mask threshold (lower = more regions)
float form_chance = .50f;       // per-cell spawn chance inside a region
int   form_steps  = 12;         // max steps in the scaffold trunk walk
float form_rmin   = 3.f;
float form_rmax   = 15.f;
int   form_config_gen;          // bump to invalidate memos after knob changes
int   form_detail = 0;          // add the fine fbm grit shell? off: ~2x cheaper
                                // gen for a barely-visible difference at 1 vox/block

// carve/detail tuning (craggy defaults; see the smooth<->craggy sweep)
static float FORM_MBALL_T   = 0.32f; // metaball isosurface threshold
static float FORM_BITE_FREQ = 0.045f, FORM_BITE_T  = 0.53f; // concave bays
static float FORM_CRACK_FREQ= 0.075f, FORM_CRACK_T = 0.60f; // cracks/arches
static float FORM_WARP_AMP  = 7.0f,   FORM_WARP_FREQ = 0.018f;
static float FORM_DTL_FREQ  = 0.15f,  FORM_DTL_T   = 0.47f; // surface grit
static int   FORM_CARVE_OCT = 2, FORM_DTL_OCT = 3;

struct formation {
        int ci, cj, gen, state;
        int bx, bz, ay;         // world col of local (0,0,0); world Y of FB_GY plane
        int n;                  // scaffold sphere count (local coords, for debug)
        float x[FORM_MAX_SPHERES], y[FORM_MAX_SPHERES], z[FORM_MAX_SPHERES],
              r[FORM_MAX_SPHERES], sq[FORM_MAX_SPHERES];
        // finished blob as per-column world-Y spans
        unsigned short coloff[FBW*FBD];
        unsigned char  colcnt[FBW*FBD];
        short slo[FORM_SPANMAX], shi[FORM_SPANMAX];
        int nspan;
};
// 2D-indexed by [ci&7][cj&7] so the 3x3 (really up to 4x4) block of cells a
// chunk touches never self-collides — three consecutive cells are always
// distinct mod 8 — which avoids the per-column rebuild thrash a hash-indexed
// pool would suffer. Evicts (real rebuild) only when the world moves >8 cells.
static _Thread_local struct formation form_memos[8][8];

// build scratch, reused across formations (too big for the C stack)
static _Thread_local unsigned char fvox[FB_VOL]; // 0 empty, 1 solid, 2 kept
static _Thread_local float          fden[FB_VOL]; // metaball accum / detail marks
static _Thread_local int            fstk[FB_VOL]; // flood-fill stack
// tight bbox of the scaffold (padded), so every stage skips the empty box
static _Thread_local int bb_i0, bb_i1, bb_j0, bb_j1, bb_k0, bb_k1;

#define FRAND01(s) ((noise_rng(&(s)) >> 8) * (1.f / 0x1000000))

// ---- deterministic 3D value / fbm / ridged noise (hash family of taylor) ----
static inline unsigned fhash3(unsigned a, unsigned b, unsigned c, unsigned s)
{
        unsigned h = a*2654435761u ^ b*2246822519u ^ c*3266489917u ^ s*668265263u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
        return h;
}
static inline float fh01(unsigned a, unsigned b, unsigned c, unsigned s)
{
        return (fhash3(a,b,c,s) >> 8) * (1.f / 0x1000000);
}
static inline float fsmooth(float t){ return t*t*(3.f-2.f*t); }

static float vnoise3(float x, float y, float z, unsigned seed)
{
        int ix=(int)floorf(x), iy=(int)floorf(y), iz=(int)floorf(z);
        float fx=x-ix, fy=y-iy, fz=z-iz;
        float u=fsmooth(fx), v=fsmooth(fy), w=fsmooth(fz);
        unsigned bx=ix+4096, by=iy+4096, bz=iz+4096; // keep hash inputs positive
        float c000=fh01(bx,by,bz,seed),     c100=fh01(bx+1,by,bz,seed);
        float c010=fh01(bx,by+1,bz,seed),   c110=fh01(bx+1,by+1,bz,seed);
        float c001=fh01(bx,by,bz+1,seed),   c101=fh01(bx+1,by,bz+1,seed);
        float c011=fh01(bx,by+1,bz+1,seed), c111=fh01(bx+1,by+1,bz+1,seed);
        float x00=c000+(c100-c000)*u, x10=c010+(c110-c010)*u;
        float x01=c001+(c101-c001)*u, x11=c011+(c111-c011)*u;
        float y0=x00+(x10-x00)*v, y1=x01+(x11-x01)*v;
        return y0+(y1-y0)*w;
}
static float fbm3(float x, float y, float z, unsigned seed, int oct, float gain)
{
        float sum=0, amp=1, tot=0, f=1;
        for(int o=0;o<oct;o++){ sum+=amp*vnoise3(x*f,y*f,z*f,seed+o*131u);
                tot+=amp; amp*=gain; f*=2.f; }
        return sum/tot;
}
static float ridged3(float x, float y, float z, unsigned seed, int oct, float gain)
{
        float sum=0, amp=1, tot=0, f=1;
        for(int o=0;o<oct;o++){ float n=vnoise3(x*f,y*f,z*f,seed+o*911u);
                n=1.f-fabsf(2.f*n-1.f); n*=n; sum+=amp*n; tot+=amp; amp*=gain; f*=2.f; }
        return sum/tot;
}

// ---- stage 1: scaffold ----
static void add_sphere(struct formation *f, float x, float y, float z, float r, float sq)
{
        if (f->n >= FORM_MAX_SPHERES) return;
        f->x[f->n]=x; f->y[f->n]=y; f->z[f->n]=z; f->r[f->n]=r; f->sq[f->n]=sq; f->n++;
}

static void build_scaffold(struct formation *f, unsigned *s)
{
        f->n = 0;
        float px=FBW/2.f, pz=FBD/2.f, py=FB_GY-2.f; // start buried at the anchor
        float ang=FRAND01(*s)*6.2831853f, dirx=cosf(ang), dirz=sinf(ang);
        float r=form_rmin+FRAND01(*s)*(form_rmax-form_rmin);
        int steps=6 + (form_steps>6 ? noise_rng(s)%(form_steps-5) : 0);
        for (int step=0; step<steps; step++)
        {
                float sq=0.7f+0.5f*FRAND01(*s);
                add_sphere(f, px, py, pz, r, sq);

                // fork a shorter, thinner limb off to the side
                if (step>0 && FRAND01(*s)<0.35f)
                {
                        float ba=(FRAND01(*s)<.5f?-1.f:1.f)*(0.8f+FRAND01(*s));
                        float ca=cosf(ba), sa=sinf(ba);
                        float lx=px, lz=pz, ly=py, lr=r*0.7f;
                        float ldx=dirx*ca-dirz*sa, ldz=dirx*sa+dirz*ca;
                        for (int t=0; t<steps-step; t++)
                        {
                                add_sphere(f, lx, ly, lz, lr, 0.8f+0.4f*FRAND01(*s));
                                float hz=lr*(0.3f+0.2f*FRAND01(*s));
                                lx+=ldx*hz; lz+=ldz*hz; ly+=(FRAND01(*s)-0.2f)*lr*0.6f;
                                lx=CLAMP(lx,6.f,FBW-6.f); lz=CLAMP(lz,6.f,FBD-6.f);
                                lr*=0.8f+0.15f*FRAND01(*s); if(lr<3.f) break;
                        }
                }

                float turn=(FRAND01(*s)-0.5f)*1.4f, ca=cosf(turn), sa=sinf(turn);
                float ndx=dirx*ca-dirz*sa, ndz=dirx*sa+dirz*ca; dirx=ndx; dirz=ndz;
                float nr=r*(0.72f+0.4f*FRAND01(*s));
                if(nr>form_rmax) nr=form_rmax; if(nr<3.f) nr=3.f;
                // mostly climb, drift a little -> leaning spires & overhangs;
                // short steps so metaballs fuse into smooth tapered limbs
                float horiz=(r+nr)*(0.14f+0.13f*FRAND01(*s));
                float climb=(r+nr)*(0.3f +0.22f*FRAND01(*s));
                px+=dirx*horiz; pz+=dirz*horiz; py+=climb;
                px=CLAMP(px,6.f,FBW-6.f); pz=CLAMP(pz,6.f,FBD-6.f);
                if(py>FBH-4) break;
                r=nr;
        }
}

// ---- stage 1b: fuse spheres into a smooth metaball field, threshold ----
static void rasterize_metaballs(struct formation *f)
{
        bb_i0=FBW; bb_i1=-1; bb_j0=FBH; bb_j1=-1; bb_k0=FBD; bb_k1=-1;
        for (int b=0; b<f->n; b++)
        {
                float cx=f->x[b], cy=f->y[b], cz=f->z[b], r=f->r[b]*1.6f, sq=f->sq[b];
                int i0=(int)(cx-r), i1=(int)(cx+r);
                int j0=(int)(cy-r*sq-1), j1=(int)(cy+r*sq+1);
                int k0=(int)(cz-r), k1=(int)(cz+r);
                if(i0<bb_i0)bb_i0=i0; if(i1>bb_i1)bb_i1=i1;
                if(j0<bb_j0)bb_j0=j0; if(j1>bb_j1)bb_j1=j1;
                if(k0<bb_k0)bb_k0=k0; if(k1>bb_k1)bb_k1=k1;
        }
        // pad by 1 for the detail shell, clamp to the box
        bb_i0=ICLAMP(bb_i0-1,0,FBW-1); bb_i1=ICLAMP(bb_i1+1,0,FBW-1);
        bb_j0=ICLAMP(bb_j0-1,0,FBH-1); bb_j1=ICLAMP(bb_j1+1,0,FBH-1);
        bb_k0=ICLAMP(bb_k0-1,0,FBD-1); bb_k1=ICLAMP(bb_k1+1,0,FBD-1);
        if(bb_i1<bb_i0) return; // no spheres

        // clear + accumulate + threshold, all within the bbox only
        for (int i=bb_i0;i<=bb_i1;i++) for (int j=bb_j0;j<=bb_j1;j++) for (int k=bb_k0;k<=bb_k1;k++)
                { fden[FVIDX(i,j,k)]=0.f; fvox[FVIDX(i,j,k)]=0; }
        for (int b=0; b<f->n; b++)
        {
                float cx=f->x[b], cy=f->y[b], cz=f->z[b], r=f->r[b]*1.6f, sq=f->sq[b];
                int i0=ICLAMP((int)(cx-r),0,FBW-1), i1=ICLAMP((int)(cx+r),0,FBW-1);
                int j0=ICLAMP((int)(cy-r*sq-1),0,FBH-1), j1=ICLAMP((int)(cy+r*sq+1),0,FBH-1);
                int k0=ICLAMP((int)(cz-r),0,FBD-1), k1=ICLAMP((int)(cz+r),0,FBD-1);
                float rr=r*r;
                for (int i=i0;i<=i1;i++) for (int j=j0;j<=j1;j++) for (int k=k0;k<=k1;k++)
                {
                        float dx=i-cx, dy=(j-cy)/sq, dz=k-cz;
                        float q=(dx*dx+dy*dy+dz*dz)/rr;
                        if(q<1.f){ float tt=1.f-q; fden[FVIDX(i,j,k)]+=tt*tt; }
                }
        }
        for (int i=bb_i0;i<=bb_i1;i++) for (int j=bb_j0;j<=bb_j1;j++) for (int k=bb_k0;k<=bb_k1;k++)
                fvox[FVIDX(i,j,k)] = fden[FVIDX(i,j,k)]>FORM_MBALL_T;
}

// ---- stage 2: carve concave bays + cracks (never below the buried base) ----
// The carve field is smooth at the block scale, so we evaluate it once per
// 2x2x2 cell and apply the decision to the whole cell — 8x fewer noise calls
// for a negligible look change (rock is chunky anyway).
static void carve(unsigned seed)
{
        int j0 = bb_j0<FB_GY ? FB_GY : bb_j0;
        for (int i=bb_i0;i<=bb_i1;i+=2) for (int j=j0;j<=bb_j1;j+=2) for (int k=bb_k0;k<=bb_k1;k+=2)
        {
                float wx=i+FORM_WARP_AMP*(vnoise3(i*FORM_WARP_FREQ,j*FORM_WARP_FREQ,k*FORM_WARP_FREQ,seed^0xA1)-0.5f)*2.f;
                float wy=j+FORM_WARP_AMP*(vnoise3(i*FORM_WARP_FREQ,j*FORM_WARP_FREQ,k*FORM_WARP_FREQ,seed^0xB2)-0.5f)*2.f;
                float wz=k+FORM_WARP_AMP*(vnoise3(i*FORM_WARP_FREQ,j*FORM_WARP_FREQ,k*FORM_WARP_FREQ,seed^0xC3)-0.5f)*2.f;
                float bite =fbm3   (wx*FORM_BITE_FREQ, wy*FORM_BITE_FREQ, wz*FORM_BITE_FREQ, seed^0xB1, FORM_CARVE_OCT,0.5f);
                float crack=ridged3(wx*FORM_CRACK_FREQ,wy*FORM_CRACK_FREQ,wz*FORM_CRACK_FREQ,seed^0xCA, FORM_CARVE_OCT,0.55f);
                if(!(bite>FORM_BITE_T || crack>FORM_CRACK_T)) continue;
                int i1=i+1<=bb_i1?i+1:i, j1=j+1<=bb_j1?j+1:j, k1=k+1<=bb_k1?k+1:k;
                for(int a=i;a<=i1;a++) for(int b=j;b<=j1;b++) for(int c=k;c<=k1;c++)
                        fvox[FVIDX(a,b,c)]=0;
        }
}

// ---- stage 3: add a thin fbm surface shell (multi-scale grit) ----
static void detail(unsigned seed)
{
        int i0=bb_i0<1?1:bb_i0, i1=bb_i1>FBW-2?FBW-2:bb_i1;
        int j0=bb_j0<FB_GY?FB_GY:bb_j0, j1=bb_j1>FBH-2?FBH-2:bb_j1;
        int k0=bb_k0<1?1:bb_k0, k1=bb_k1>FBD-2?FBD-2:bb_k1;
        // half-res like carve: one fbm decision per 2x2x2 cell, added as a shell
        // where the cell is empty but touches solid. Marks go in fden so the
        // scan reads only the pre-existing fvox (no cascading growth); clear the
        // lattice first since rasterize left metaball densities there.
        for (int i=i0;i<=i1;i+=2) for (int j=j0;j<=j1;j+=2) for (int k=k0;k<=k1;k+=2)
                fden[FVIDX(i,j,k)]=0.f;
        for (int i=i0;i<=i1;i+=2) for (int j=j0;j<=j1;j+=2) for (int k=k0;k<=k1;k+=2)
        {
                if(fvox[FVIDX(i,j,k)]) continue;
                if(!(fvox[FVIDX(i-1,j,k)]||fvox[FVIDX(i+1,j,k)]||fvox[FVIDX(i,j-1,k)]||
                     fvox[FVIDX(i,j+1,k)]||fvox[FVIDX(i,j,k-1)]||fvox[FVIDX(i,j,k+1)])) continue;
                float d=fbm3(i*FORM_DTL_FREQ,j*FORM_DTL_FREQ,k*FORM_DTL_FREQ,seed^0xDE,FORM_DTL_OCT,0.5f);
                if(d>FORM_DTL_T) fden[FVIDX(i,j,k)]=1.f;
        }
        for (int i=i0;i<=i1;i+=2) for (int j=j0;j<=j1;j+=2) for (int k=k0;k<=k1;k+=2)
                if(fden[FVIDX(i,j,k)]>0.5f)
                {
                        int a1=i+1<=i1?i+1:i, b1=j+1<=j1?j+1:j, c1=k+1<=k1?k+1:k;
                        for(int a=i;a<=a1;a++) for(int b=j;b<=b1;b++) for(int c=k;c<=c1;c++)
                                if(!fvox[FVIDX(a,b,c)]) fvox[FVIDX(a,b,c)]=1;
                }
}

// ---- stage 4: flood-fill from the buried anchor; keep only that component ----
static void flood_keep(void)
{
        int si=FBW/2, sk=FBD/2, sj=FB_GY-2;
        if(fvox[FVIDX(si,sj,sk)]!=1)
        {
                int found=0;
                for(int j=0;j<FBH && !found;j++) if(fvox[FVIDX(si,j,sk)]==1){ sj=j; found=1; }
                if(!found) return; // empty formation
        }
        int sp=0;
        fstk[sp++]=FVIDX(si,sj,sk); fvox[FVIDX(si,sj,sk)]=2;
        while(sp)
        {
                int id=fstk[--sp];
                int i=id/(FBH*FBD), j=(id/FBD)%FBH, k=id%FBD;
                if(i>0)     { int n=id-FBH*FBD; if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
                if(i<FBW-1) { int n=id+FBH*FBD; if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
                if(j>0)     { int n=id-FBD;     if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
                if(j<FBH-1) { int n=id+FBD;     if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
                if(k>0)     { int n=id-1;       if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
                if(k<FBD-1) { int n=id+1;       if(fvox[n]==1){fvox[n]=2;fstk[sp++]=n;} }
        }
        // drop floaters (unvisited solids) — only the bbox can hold any
        for (int i=bb_i0;i<=bb_i1;i++) for (int j=bb_j0;j<=bb_j1;j++) for (int k=bb_k0;k<=bb_k1;k++)
                if(fvox[FVIDX(i,j,k)]==1) fvox[FVIDX(i,j,k)]=0;
}

// ---- extract per-column vertical spans, converting local j to world Y ----
static void extract_spans(struct formation *f)
{
        f->nspan = 0;
        for (int i=bb_i0;i<=bb_i1;i++) for (int k=bb_k0;k<=bb_k1;k++)
        {
                int c=i*FBD+k;
                f->coloff[c]=f->nspan;
                int cnt=0, j=bb_j0;
                while(j<=bb_j1)
                {
                        if(fvox[FVIDX(i,j,k)]==2)
                        {
                                int j0=j; while(j<=bb_j1 && fvox[FVIDX(i,j,k)]==2) j++;
                                int j1=j-1;
                                // local up (bigger j) = higher altitude = smaller world Y
                                int lo=f->ay-(j1-FB_GY), hi=f->ay-(j0-FB_GY);
                                if(lo<1) lo=1; if(hi>TILESH-2) hi=TILESH-2;
                                if(lo<=hi && f->nspan<FORM_SPANMAX && cnt<255)
                                {
                                        f->slo[f->nspan]=lo; f->shi[f->nspan]=hi;
                                        f->nspan++; cnt++;
                                }
                        }
                        else j++;
                }
                f->colcnt[c]=cnt;
        }
}

struct formation *get_formation(int ci, int cj)
{
        struct formation *f = &form_memos[ci & 7][cj & 7];
        if (f->state && f->ci==ci && f->cj==cj && f->gen==form_config_gen)
                return f;

        f->ci=ci; f->cj=cj; f->gen=form_config_gen; f->state=1; f->n=0; f->nspan=0;
        memset(f->colcnt, 0, sizeof f->colcnt);
        if (!form_enable) return f;

        unsigned s = noise_hash(ci, cj, world_seed ^ 0xB10B);
        if (!s) s = 1;

        // clustered, not sprinkled: only cells inside a low-frequency mask
        float region = noise(ci*FORM_CELL + FORM_CELL/2, cj*FORM_CELL + FORM_CELL/2,
                        2500, world_seed ^ 0x0F0F0F0F, 2);
        if (region < form_region || FRAND01(s) > form_chance)
                return f;

        // pick an anchor on the surface, preferring the steepest ground so
        // crags grow off cliff brows and mountainsides
        int ax=0, az=0; float ah=0.f, best=-1.f;
        for (int t=0; t<5; t++)
        {
                int tx = ci*FORM_CELL + noise_rng(&s)%FORM_CELL;
                int tz = cj*FORM_CELL + noise_rng(&s)%FORM_CELL;
                float h = get_filtered_height(tx, tz);
                if (h < .46f) continue; // deep ocean: no formations
                float gx=(get_filtered_height(tx+4,tz)-get_filtered_height(tx-4,tz))/8.f;
                float gz=(get_filtered_height(tx,tz+4)-get_filtered_height(tx,tz-4))/8.f;
                float steep=gx*gx+gz*gz;
                if (steep>best){ best=steep; ax=tx; az=tz; ah=h; }
        }
        if (best < 0.f) return f; // whole cell is deep water

        f->bx = ax - FBW/2;
        f->bz = az - FBD/2;
        f->ay = TILESH - (int)(ah * TERRAIN_VSCALE);

        unsigned cseed = noise_hash(ci, cj, world_seed ^ 0x0CA0E);
        unsigned dseed = noise_hash(ci, cj, world_seed ^ 0x0DE7A);

        build_scaffold(f, &s);
        rasterize_metaballs(f);
        carve(cseed);
        if (form_detail) detail(dseed);
        flood_keep();
        extract_spans(f);
        return f;
}

// solid y-intervals formations add to column (ax,az); returns interval count
int form_spans(int ax, int az, int *lo, int *hi, int max)
{
        if (!form_enable) return 0;
        int n=0;
        int ci=ax>>FORM_CELL_BITS, cj=az>>FORM_CELL_BITS;
        for (int i=ci-1;i<=ci+1;i++) for (int j=cj-1;j<=cj+1;j++)
        {
                struct formation *f = get_formation(i, j);
                if (!f->nspan) continue;
                int li=ax-f->bx, lk=az-f->bz;
                if (li<0||li>=FBW||lk<0||lk>=FBD) continue;
                int c=li*FBD+lk;
                int cnt=f->colcnt[c], off=f->coloff[c];
                for (int q=0; q<cnt && n<max; q++){ lo[n]=f->slo[off+q]; hi[n]=f->shi[off+q]; n++; }
        }
        return n;
}

#endif // BLOCKO_FORMATIONS_C_INCLUDED
