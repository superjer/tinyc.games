#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>

// default 12x6 
#define sX DUNW // overworld screens across (srooms)
#define sY DUNH // overworld screens down
#define oX (sX*2) // subscreen count (each screen is 4 subscreens) (orooms)
#define oY (sY*2)
#define sW 15 // size of each screen
#define sH 11
#define sW2 (sW/2)
#define sH2 (sH/2)
#define pW 3 // size of subscreen openings in preview printout
#define pH 2

#define LOOPINESS 0.04f // how often to accept a random door that makes a loop, 0.0-1.0
#define EW_BIAS 0.65f // how often to choose an east-west door over north-south, 0.0-1.0
#define RECTS 6

#define SWAP(a, b) { int tmp__ = (a); a = b; b = tmp__; }
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct oroom {
        int open_r;
        int open_d;
        int access;
};

struct oroom oroom[oX][oY];

struct rect {
        int x0, y0, x1, y1;
};

struct sroom {
        struct rect rect[RECTS];
};

struct sroom sroom[sX][sY];

void remove_access()
{
        int x, y;
        for (x = 0; x < oX; x++) for (y = 0; y < oY; y++)
                oroom[x][y].access = 0;
}

int in_traverse(int sx, int sy, int dist)
{
        int ttl = 1;

        if (oroom[sx][sy].access && oroom[sx][sy].access <= dist) return 0; // already been here

        if (oroom[sx][sy].access) ttl = 0;

        oroom[sx][sy].access = dist;

        if (sx > 0 && oroom[sx - 1][sy].open_r)
                ttl += in_traverse(sx - 1, sy, dist + 1);

        if (sy > 0 && oroom[sx][sy - 1].open_d)
                ttl += in_traverse(sx, sy - 1, dist + 1);

        if (sx < oX - 1 && oroom[sx][sy].open_r)
                ttl += in_traverse(sx + 1, sy, dist + 1);

        if (sy < oY - 1 && oroom[sx][sy].open_d)
                ttl += in_traverse(sx, sy + 1, dist + 1);

        return ttl;
}

int traverse(int sx, int sy)
{
        remove_access();
        return in_traverse(sx, sy, 1);
}

void print_orooms()
{
        int i, j;
        char out[oY * pH][oX * pW];
        memset(out, '@', (oX * pW * oY * pH));

        for (i = 0; i < oX; i++) for (j = 0; j < oY; j++)
        {
                int m, n;
                for (m = 0; m < pW - 1; m++) for (n = 0; n < pH - 1; n++)
                {
                        int x = i * pW + m;
                        int y = j * pH + n;
                        out[y][x] = oroom[i][j].access ? ' ' : '.';
                }

                char dist_ch = ' ';
                /*
                if (oroom[i][j].access > 10) dist_ch = 'a' - 1 + oroom[i][j].access - 10;
                else if (oroom[i][j].access) dist_ch = '0' - 1 + oroom[i][j].access;
                */
                out[j * pH + pH / 2 - 1][i * pW + pW / 2 - 1] = dist_ch;

                if (oroom[i][j].open_r) for(n = 0; n < pH - 1; n++)
                        out[j * pH + n][i * pW + pW - 1] = ' ';

                if (oroom[i][j].open_d) for(m = 0; m < pW - 1; m++)
                        out[j * pH + pH - 1][i * pW + m] = ' ';
        }

        // patch (real room) centers
        for (i = 0; i < oX; i++) for (j = 0; j < oY; j++)
        {
                if (i == 0 || j == 0) continue;
                if (i % 2 == 0 && j % 2 == 0) continue; 
                if (out[j * pH - 0][i * pW - 1] == '@') continue;
                if (out[j * pH - 2][i * pW - 1] == '@') continue;
                if (out[j * pH - 1][i * pW - 0] == '@') continue;
                if (out[j * pH - 1][i * pW - 2] == '@') continue;
                out[j * pH - 1][i * pW - 1] = ' ';
        }

        printf("@%.*s\n", oX * pW, out[oY * pH - 1]);
        for (i = 0; i < oY * pH; i++)
                printf("@%.*s\n", oX * pW, out[i]);
}

void open_random_door(float loop_chance)
{
        int x, y;
        int loop_ok = (float)rand() / RAND_MAX < loop_chance;
        retry:
        x = rand() % oX;
        y = rand() % oY;

        traverse(x, y);

        if ((float)rand() / RAND_MAX < EW_BIAS)
        {
                if (x == oX - 1) goto retry;

                int can_get_there = oroom[x + 1][y].access;
                int already_open = oroom[x][y].open_r;
                if (already_open || (can_get_there && !loop_ok)) goto retry;

                oroom[x][y].open_r = 1;
        }
        else
        {
                if (y == oY - 1) goto retry;

                int can_get_there = oroom[x][y + 1].access;
                int already_open = oroom[x][y].open_d;
                if (already_open || (can_get_there && !loop_ok)) goto retry;

                oroom[x][y].open_d = 1;
        }
}

// every corner must be accessible from 1 of the two doors that connect it
void cook_corners()
{
        if (rand() % 2) oroom[0][0].open_r = 1;
        else            oroom[0][0].open_d = 1;

        if (rand() % 2) oroom[oX - 2][0].open_r = 1;
        else            oroom[oX - 1][0].open_d = 1;

        if (rand() % 2) oroom[0][oY - 1].open_r = 1;
        else            oroom[0][oY - 2].open_d = 1;

        if (rand() % 2) oroom[oX - 2][oY - 1].open_r = 1;
        else            oroom[oX - 1][oY - 2].open_d = 1;
}

// every aligned patch of 4 orooms is one room, and it *usually* all open
void cook_squares()
{
        int i, j;
        for (i = 0; i < oX; i++) for (j = 0; j < oY; j++)
        {
                if (i % 2 == 0 && rand() % 7 > 1)
                        oroom[i][j].open_r = 1;
                if (j % 2 == 0 && rand() % 7 > 1)
                        oroom[i][j].open_d = 1;
        }
}

void cook_squares_again()
{
        int i, j;
        // if 3+ doors are open in a patch, open them all
        for (i = 0; i < oX; i += 2) for (j = 0; j < oY; j += 2)
        {
                if (oroom[i][j].open_r + oroom[i][j].open_d + oroom[i][j + 1].open_r + oroom[i + 1][j].open_d >= 3)
                        oroom[i][j].open_r = oroom[i][j].open_d = oroom[i][j + 1].open_r = oroom[i + 1][j].open_d = 1;
                        
        }

        // open doors on screen edges that are not an obstacle
        for (i = 1; i < oX; i += 2) for (j = 1; j < oY; j += 2)
        {
                if (oroom[i - 1][j].open_r && oroom[i - 1][j + 1].open_r && (oroom[i - 1][j].open_d || oroom[i][j].open_d))
                        oroom[i - 1][j].open_d = oroom[i][j].open_d = 1;
                        
                if (oroom[i][j - 1].open_d && oroom[i + 1][j - 1].open_d && (oroom[i][j - 1].open_r || oroom[i][j].open_r))
                        oroom[i][j - 1].open_r = oroom[i][j].open_r = 1;
        }
}

int open_a_useful_door(int min_to_open)
{
        int offset = rand() % (oX * oY);

        for (int k = 0; k < oX * oY; k++)
        {
                int koff = (k + offset) % (oX * oY);
                int i = koff % oX;
                int j = koff / oX;
                traverse(i, j);

                if (i < oX - 1 && oroom[i + 1][j].access > min_to_open)
                {
                        oroom[i][j].open_r = 1;
                        return 1;
                }

                if (j < oY - 1 && oroom[i][j + 1].access > min_to_open)
                {
                        oroom[i][j].open_d = 1;
                        return 1;
                }
        }

        return 0;
}

int pct(int chance)
{
        return rand() % 100 < chance;
}

void place_initial_rects()
{
        int i, j;
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++)
        {
                //   |   : q | s : u |
                // --+-------+-------+-
                //   |   :   |   :   |
                // ..|.......|.......|.
                // r |   :   |   :   |
                // --+-------+=======+-
                // t |   :   |ij :   |
                // ..|.......|.......|.
                // v |   :   |   :   |
                // --+-------+=======+-
                //   |   :   |   :   |
                int s = i * 2;
                int t = j * 2;
                int q = s - 1;
                int r = t - 1;
                int u = s + 1;
                int v = t + 1;

                if (i && pct(35) && oroom[q][t].open_r && oroom[q][v].open_r)
                {
                        // copy from left
                        sroom[i][j].rect[0].y0 = sroom[i - 1][j].rect[0].y0;
                        sroom[i][j].rect[0].y1 = sroom[i - 1][j].rect[0].y1;
                }
                else
                {
                        int a = 1 + rand() % (sH - 4);
                        int b = 1 + rand() % (sH - 4);
                        if (a > b) SWAP(a, b);
                        a = MIN(a, sroom[i - 1][j].rect[0].y1);
                        b = MAX(b, sroom[i - 1][j].rect[0].y0);
                        sroom[i][j].rect[0].y0 = pct(50) ?      2 : a;
                        sroom[i][j].rect[0].y1 = pct(50) ? sH - 3 : b;
                }


                if (j && pct(15) && oroom[s][r].open_d && oroom[u][r].open_d)
                {
                        // copy from up
                        sroom[i][j].rect[0].x0 = sroom[i][j - 1].rect[0].x0;
                        sroom[i][j].rect[0].x1 = sroom[i][j - 1].rect[0].x1;
                }
                else
                {
                        int a = 1 + rand() % (sW - 3);
                        int b = 1 + rand() % (sW - 3);
                        if (a > b) SWAP(a, b);
                        a = MIN(a, sroom[i][j - 1].rect[0].x1);
                        b = MAX(b, sroom[i][j - 1].rect[0].x0);
                        sroom[i][j].rect[0].x0 = pct(50) ?      1 : a;
                        sroom[i][j].rect[0].x1 = pct(50) ? sW - 2 : b;
                }
        }
}

void place_connecting_rects()
{
        int i, j;
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++)
        {
                //   |   : q | s : u |
                // --+-------+-------+-
                //   |   :   |   :   |
                // ..|.......|.......|.
                // r |   :   |   :   |
                // --+-------+=======+-
                // t |   :   |ij :   |
                // ..|.......|.......|.
                // v |   :   |   :   |
                // --+-------+=======+-
                //   |   :   |   :   |
                int s = i * 2;
                int t = j * 2;
                int q = s - 1;
                int r = t - 1;
                int u = s + 1;
                int v = t + 1;

                #define FUDGE(p) if (pct(p) && b > a) {    \
                        int fuj = rand() % (b - a);        \
                        int afuj = fuj ? rand() % fuj : 0; \
                        a += afuj;                         \
                        b -= fuj - afuj;                   \
                }

                // left rect
                if (i > 0 && (oroom[q][t].open_r || oroom[q][v].open_r))
                {
                        int b = MAX(sroom[i][j].rect[0].y0, sroom[i - 1][j].rect[2].y0);
                        int a = MIN(sroom[i][j].rect[0].y1, sroom[i - 1][j].rect[2].y1);
                        int c = sroom[i][j].rect[0].x0 - 1;
                        if (a > b) SWAP(a, b);
                        sroom[i][j].rect[1] = (struct rect){0, a, c, b};
                }

                // right rect
                if (i < sX - 1 && (oroom[u][t].open_r || oroom[u][v].open_r))
                {
                        int a = MAX(sroom[i][j].rect[0].y0, sroom[i + 1][j].rect[0].y0);
                        int b = MIN(sroom[i][j].rect[0].y1, sroom[i + 1][j].rect[0].y1);
                        int c = sroom[i][j].rect[0].x1 + 1;
                        //if (a > b) SWAP(a, b);
                        /*
                        if (a > b) {
                                a = b;
                                c = sroom[i][j].rect[0].x0 + rand() % (sroom[i][j].rect[0].x1 - sroom[i][j].rect[0].x0);
                                sroom[i][j].rect[5] = (struct rect){c, b, sroom[i][j].rect[0].x1, sroom[i][j].rect[0].y1};
                        }
                        */
                        FUDGE(33);
                        printf("a:%d b:%d\n", a, b);
                        sroom[i][j].rect[2] = (struct rect){c, a, sW - 1, b};
                }

                // top rect
                if (j > 0 && (oroom[s][r].open_d || oroom[u][r].open_d))
                {
                        int a = MAX(sroom[i][j].rect[0].x0, sroom[i][j - 1].rect[4].x0);
                        int b = MIN(sroom[i][j].rect[0].x1, sroom[i][j - 1].rect[4].x1);
                        int c = sroom[i][j].rect[0].y0 - 1;
                        if (a > b) SWAP(a, b);
                        sroom[i][j].rect[3] = (struct rect){a, 0, b, c};
                }

                // bottom rect
                if (j < sY - 1 && (oroom[s][v].open_d || oroom[u][v].open_d))
                {
                        int a = MAX(sroom[i][j].rect[0].x0, sroom[i][j + 1].rect[0].x0);
                        int b = MIN(sroom[i][j].rect[0].x1, sroom[i][j + 1].rect[0].x1);
                        int c = sroom[i][j].rect[0].y1 + 1;
                        if (a > b) SWAP(a, b);
                        FUDGE(85);
                        sroom[i][j].rect[4] = (struct rect){a, c, b, sH - 1};
                }
        }
}

void print_srooms()
{
        int i, j, r;
        char out[sY * sH][sX * sW];
        memset(out, '@', (sX * sW * sY * sH));
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++) for(r = 0; r < RECTS; r++)
        {
                if (sroom[i][j].rect[r].x1 == 0 && sroom[i][j].rect[r].y1 == 0)
                        continue;

                for (int u = sroom[i][j].rect[r].x0; u <= sroom[i][j].rect[r].x1; u++)
                        for (int v = sroom[i][j].rect[r].y0; v <= sroom[i][j].rect[r].y1; v++)
                {
                        int edge = (v == 0 || u == 0 || v == sH - 1 || u == sW - 1);
                        out[j * sH + v][i * sW + u] = edge ? '.' : ' ';
                }
        }

        for (i = 0; i < sY * sH; i++)
                printf("%.*s\n", sX * sW, out[i]);
}

void odnar()
{
        int traverse_count = 0;

        srand(time(NULL));

        cook_corners();
        cook_squares();

        print_orooms();

        while (traverse_count < oX * oY)
        {
                open_random_door(LOOPINESS);
                traverse_count = traverse(oX / 2, oY - 1);
        }

        while (open_a_useful_door(38))
                traverse(oX / 2, oY - 1);

        cook_squares_again();
        print_orooms();

        place_initial_rects();
        place_connecting_rects();
        print_srooms();
}
