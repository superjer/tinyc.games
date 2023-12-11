// Simple implementation of "Taylor Noise" copyright Jer Wilson 2023
// for TinyC.Games

#ifndef TINY_C_GAMES_TAYLOR_NOISE_
#define TINY_C_GAMES_TAYLOR_NOISE_

#include <string.h>
#include <math.h>
#include "utils.c"

int noise_hit, noise_miss;

#define NOISE_GRADSZ 10000
enum { NOISE_LINEAR, NOISE_SQUARE, NOISE_SUBLIN, NOISE_SMSTEP };
float noise_gradient[4][NOISE_GRADSZ + 1] = {{0.f}};

void noise_setup()
{
        // pre-compute weights in within radii
        for (int i = 0; i <= NOISE_GRADSZ; i++)
        {
                float t = (float)i / (float)NOISE_GRADSZ;
                noise_gradient[NOISE_SQUARE][i] = 1.f - t;
                noise_gradient[NOISE_LINEAR][i] = 1.f - sqrtf(t);
                noise_gradient[NOISE_SUBLIN][i] = 1.f - powf(t, .25f);
                noise_gradient[NOISE_SMSTEP][i] = 1.f - lerp(t, t, pow(t, .25f));
        }
}

unsigned long long xorshift(unsigned long long n)
{
	n ^= n << 13;
	n ^= n >> 7;
	n ^= n << 17;
	return n;
}

//#define noise memo_noise
#define noise plain_noise

float memo_noise(int x, int y, int sz, unsigned long long seed, int samples)
{
        // no negative numbers!
        sz &= 0x00ffffff;
        sz /= 2;
        x += 0x10000000;
        y += 0x01000000;
        if (x <= sz) x = sz + 1;
        if (y <= sz) y = sz + 1;

        struct memo {
                int i, j, u[16], v[16], n;
                unsigned long long seed;
                int sz;
                float limit_sq_inv;
                union {
                        float f;
                        unsigned u;
                } strength[16];
        };
        static struct memo memos[17][17][17];

        float limit_sq = sz * sz;
        int xx = (x / sz) * sz;
        int yy = (y / sz) * sz;
        float sum_strengths = .5f;
        float sum_weights = 1.f;
        for (int i = xx-sz; i <= xx+sz; i+=sz) for (int j = yy-sz; j <= yy+sz; j+=sz)
        {
                struct memo *m = &memos[i % 17][j % 17][sz % 17];
                int is_memod = (m->i == i && m->j == j && m->sz == sz && m->seed == seed);
                if (!is_memod)
                {
                        m->i = i;
                        m->j = j;
                        m->sz = sz;
                        m->seed = seed;
                        m->limit_sq_inv = 1.f / limit_sq;
                        memset(m->strength, 0, sizeof m->strength);
                        seed = xorshift(seed ^ i ^ (j * 128));
fprintf(stderr, "Memo Seed=%llu Samples=%d\n", seed, samples);
                        m->n = samples;
                        noise_miss++;
                }
                else
                        noise_hit++;

                for (int n = 0; n < m->n; n++)
                {
fprintf(stderr, "Loop N=%d\n", n);
                        if (!is_memod)
                        {
                                m->u[n] = i + (seed = xorshift(seed)) % sz;
                                m->v[n] = j + (seed = xorshift(seed)) % sz;
                                m->strength[n].u = (0x3f800000 | (0x007fffff & (xorshift(seed))));
                                m->strength[n].f -= 1.f;
                        }
fprintf(stderr, "Strength=%f\n", m->strength[n].f);
                        float dist_sq = (x-m->u[n]) * (x-m->u[n]) + (y-m->v[n]) * (y-m->v[n]);
fprintf(stderr, "Dist_Sq=%f Limit_Sq=%f\n", dist_sq, limit_sq);
                        if (dist_sq > limit_sq)
                                continue;
fprintf(stderr, "Hi\n");
                        float weight = noise_gradient[NOISE_SQUARE][
                                (int)floorf(NOISE_GRADSZ * dist_sq * m->limit_sq_inv)
                        ];
                        sum_strengths += m->strength[n].f * weight;
                        sum_weights += weight;
fprintf(stderr, "Sum Strengths=%f\n", sum_strengths);
exit(0);
                }
        }
        return sum_strengths / sum_weights;
}

float plain_noise(int x, int y, int sz, unsigned long long seed, int samples)
{
        // no negative numbers!
        sz &= 0x00ffffff;
        sz /= 2;
        x += 0x10000000;
        y += 0x01000000;
        if (x <= sz) x = sz + 1;
        if (y <= sz) y = sz + 1;

        float limit_sq = sz * sz;
        float limit_sq_inv = 1.f / limit_sq;
        int xx = (x / sz) * sz;
        int yy = (y / sz) * sz;
        float sum_strengths = .5f;
        float sum_weights = 1.f;
        for (int i = xx-sz; i <= xx+sz; i+=sz) for (int j = yy-sz; j <= yy+sz; j+=sz)
        {
                seed = xorshift(seed ^ i ^ (j * 128));
fprintf(stderr, "Plain Seed=%llu Samples=%d\n", seed, samples);

                for (int n = 0; n < samples; n++)
                {
fprintf(stderr, "Loop N=%d\n", n);
                        union { unsigned u; float f; } strength;
                        int u = i + (seed = xorshift(seed)) % sz;
                        int v = j + (seed = xorshift(seed)) % sz;
                        strength.u = (0x3f800000 | (0x007fffff & (xorshift(seed))));
                        strength.f -= 1.f;
fprintf(stderr, "Strength=%f\n", strength.f);
                        float dist_sq = (x-u) * (x-u) + (y-v) * (y-v);
fprintf(stderr, "Dist_Sq=%f Limit_Sq=%f\n", dist_sq, limit_sq);
                        if (dist_sq > limit_sq)
                                continue;
fprintf(stderr, "Hi\n");
                        float weight = noise_gradient[NOISE_SQUARE][
                                (int)floorf(NOISE_GRADSZ * dist_sq * limit_sq_inv)
                        ];
                        sum_strengths += strength.f * weight;
                        sum_weights += weight;
fprintf(stderr, "Sum Strengths=%f\n", sum_strengths);
exit(0);
                }
        }
        return sum_strengths / sum_weights;
}

#endif
