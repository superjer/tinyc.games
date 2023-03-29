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

float noise(int x, int y, int sz, int seed, int samples)
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
                int seed;
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
                        srand(i ^ (j * 128) ^ seed);
                        m->n = samples; //9 + rand() % 8;
                        noise_miss++;
                }
                else
                        noise_hit++;

                for (int n = 0; n < m->n; n++)
                {
                        if (!is_memod)
                        {
                                m->u[n] = i + rand() % sz;
                                m->v[n] = j + rand() % sz;
                                m->strength[n].u = (0x3f800000 | (0x007fffff & rand()));
                                m->strength[n].f -= 1.f;
                        }
                        float dist_sq = (x-m->u[n]) * (x-m->u[n]) + (y-m->v[n]) * (y-m->v[n]);
                        if (dist_sq > limit_sq)
                                continue;
                        float weight = noise_gradient[NOISE_SQUARE][
                                (int)floorf(NOISE_GRADSZ * dist_sq * m->limit_sq_inv)
                        ];
                        sum_strengths += m->strength[n].f * weight;
                        sum_weights += weight;
                }
        }
        return sum_strengths / sum_weights;
}

#endif
