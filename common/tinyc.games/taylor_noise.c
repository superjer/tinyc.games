// Simple implementation of "Taylor Noise" copyright Jer Wilson 2023
// for TinyC.Games
//
// Feature points are hashed from (cell, seed), so results are deterministic
// across platforms, threads, and runs — no libc rand.

#ifndef TINYCGAMES_TAYLOR_NOISE_C_INCLUDED
#define TINYCGAMES_TAYLOR_NOISE_C_INCLUDED

#include <math.h>

_Thread_local int noise_hit, noise_miss;

// runtime knobs - bump noise_config_gen after changing any so memos refill
int noise_kernel_sq = 0;       // squared falloff: smoother blobs, no crease at edges
float noise_base_weight = 1.f; // weight of the phantom 0.5 feature; lower = more contrast
float noise_aniso = 0.f;       // 0=round blobs, ->1 stretches each into an oriented ridge
int noise_nvary = 0;           // vary feature count per cell (samples..2*samples)
int noise_config_gen;

void noise_setup() { }

static inline unsigned noise_hash(unsigned a, unsigned b, unsigned c)
{
        unsigned h = a * 2654435761u ^ b * 2246822519u ^ c * 3266489917u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return h;
}

static inline unsigned noise_rng(unsigned *s)
{
        unsigned x = *s;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return *s = x;
}

float noise(int x, int y, int sz, int seed, int samples)
{
        // no negative numbers!
        sz &= 0x00ffffff;
        sz /= 5;
        x += 0x10000000;
        y += 0x01000000;
        if (x <= sz) x = sz + 1;
        if (y <= sz) y = sz + 1;
        if (samples > 16) samples = 16;

        struct memo {
                int i, j, sz, seed, req, n, cfg;
                int u[16], v[16];
                float strength[16];
                float ca[16], sa[16]; // feature orientation, for aniso
        };
        static _Thread_local struct memo memos[4096];

        // across-axis shrink: blobs stay within radius sz along their axis
        float aniso_k = 1.f + 6.f * noise_aniso;

        float limit_sq = (float)sz * (float)sz;
        float limit_sq_inv = 1.f / limit_sq;
        int xx = (x / sz) * sz;
        int yy = (y / sz) * sz;
        float sum_strengths = .5f * noise_base_weight;
        float sum_weights = noise_base_weight;
        for (int i = xx-sz; i <= xx+sz; i+=sz) for (int j = yy-sz; j <= yy+sz; j+=sz)
        {
                struct memo *m = &memos[noise_hash(i, j, seed ^ sz) & 4095];
                if (m->i != i || m->j != j || m->sz != sz || m->seed != seed
                        || m->req != samples || m->cfg != noise_config_gen)
                {
                        m->i = i;
                        m->j = j;
                        m->sz = sz;
                        m->seed = seed;
                        m->req = samples;
                        m->cfg = noise_config_gen;
                        unsigned s = noise_hash(i, j * 128, seed);
                        if (!s) s = 1;
                        int n = samples;
                        if (noise_nvary)
                                n = samples + noise_rng(&s) % (samples + 1);
                        if (n > 16) n = 16;
                        m->n = n;
                        for (int k = 0; k < n; k++)
                        {
                                m->u[k] = i + (noise_rng(&s) >> 8) % sz;
                                m->v[k] = j + (noise_rng(&s) >> 8) % sz;
                                m->strength[k] = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                                float ang = noise_rng(&s) * 1.46291808e-9f; // 2pi/2^32
                                m->ca[k] = cosf(ang);
                                m->sa[k] = sinf(ang);
                        }
                        noise_miss++;
                }
                else
                        noise_hit++;

                for (int n = 0; n < m->n; n++)
                {
                        float dx = x - m->u[n];
                        float dy = y - m->v[n];
                        float dist_sq;
                        if (aniso_k > 1.f)
                        {
                                float along  =  dx * m->ca[n] + dy * m->sa[n];
                                float across = (-dx * m->sa[n] + dy * m->ca[n]) * aniso_k;
                                dist_sq = along * along + across * across;
                        }
                        else
                                dist_sq = dx * dx + dy * dy;
                        if (dist_sq > limit_sq)
                                continue;
                        float weight = 1.f - dist_sq * limit_sq_inv;
                        if (noise_kernel_sq)
                                weight *= weight;
                        sum_strengths += m->strength[n] * weight;
                        sum_weights += weight;
                }
        }
        if (sum_weights < 1e-6f) // possible when base weight is turned down
                return .5f;
        return sum_strengths / sum_weights;
}

#endif // TINYCGAMES_TAYLOR_NOISE_C_INCLUDED
