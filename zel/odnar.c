#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>

#ifndef DUNW
#define DUNW 3
#define DUNH 3
#endif

// default 12x6 
#define sX DUNW // overworld screens across (srooms)
#define sY DUNH // overworld screens down
#define oX (sX*2) // subscreen count (each screen is 4 subscreens) (orooms)
#define oY (sY*2)
#define sW 15 // size of each screen
#define sH 11
#define sW2 (sW/2)
#define sH2 (sH/2)
#define sW4 (sW/4)
#define sH4 (sH/4)
#define pW 3 // size of subscreen openings in preview printout
#define pH 2

#define LOOPINESS 0.04f // how often to accept a random door that makes a loop, 0.0-1.0
#define EW_BIAS 0.65f // how often to choose an east-west door over north-south, 0.0-1.0
#define RECTS 6
#define NUM_KEY_POINTS (sX * sY)
#define OBSTACLE_ATTEMPTS (sX * sY * 4)
#define OPENINGS_MOD 9

#define false 0
#define true 1

#define SWAP(a, b) { int tmp__ = (a); a = b; b = tmp__; }
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct oroom {
        int open_r;
        int open_d;
        int access;
        char base_char;
};

struct oroom oroom[oX][oY];

struct rect {
        int x0, y0, x1, y1;
};

char charout[sY * sH][sX * sW];
char flood_buf[sY * sH][sX * sW];

struct point {
        int x, y;
};

struct point key_points[NUM_KEY_POINTS];

int pct(int chance)
{
        return rand() % 100 < chance;
}

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

void convert_orooms_to_charout()
{
        int i, j, m, n;
        memset(charout, ' ', (sX * sW * sY * sH));
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++)
        {
                char screen_char = 'R';
                switch (rand() % 4)
                {
                        case 0: screen_char = 'R'; break;
                        case 1: screen_char = 'S'; break;
                        case 2: screen_char = 'T'; break;
                        case 3: screen_char = 'W'; break;
                }

                for (m = 0; m < sW; m++) for (n = 0; n < sH; n++)
                {
                        int x = i * sW + m;
                        int y = j * sH + n;
                        int top    = (n <= 1);
                        int bottom = (n >= sH - 2);
                        int left   = (m <= 1);
                        int right  = (m >= sW - 2);
                        int oi = i * 2 + (m > sW2);
                        int oj = j * 2 + (n > sH2);

                        if (top)
                        {
                                if (!left && !right && oj > 0 && oroom[oi][oj - 1].open_d)
                                        charout[y][x] = ' ';
                                else
                                        charout[y][x] = screen_char;
                        }
                        else if (bottom)
                        {
                                if (!left && !right && oj < oY - 1 && oroom[oi][oj].open_d)
                                        charout[y][x] = ' ';
                                else
                                        charout[y][x] = screen_char;
                        }
                        else if (left  )
                        {
                                if (!top && !bottom && oi > 0 && oroom[oi - 1][oj].open_r)
                                        charout[y][x] = ' ';
                                else
                                        charout[y][x] = screen_char;
                        }
                        else if (right )
                        {
                                if (!top && !bottom && oi < oX - 1 && oroom[oi][oj].open_r)
                                        charout[y][x] = ' ';
                                else
                                        charout[y][x] = screen_char;
                        }

                        if (charout[y][x] == ' ')
                        {
                                if (n == sH2 && !oroom[oi][oj].open_d)
                                        charout[y][x] = screen_char;
                                if (m == sW2 && !oroom[oi][oj].open_r)
                                        charout[y][x] = screen_char;
                        }
                }
        }
}

// make sure solids bleed all the way across screen edges
void bleed_edges()
{
        int i, j, m, n;
        // i,j identify the room
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++)
        {
                // m,n identify the tile in the room
                for (m = 0; m < sW; m++) for (n = 0; n < sH; n++)
                {
                        // x and y are the global tile position
                        int x = i * sW + m;
                        int y = j * sH + n;
                        if (i > 0 && m == 0 && charout[y][x] == ' ')
                                charout[y][x] = charout[y][x - 1];
                        if (i < sX - 1 && m == sW - 1 && charout[y][x] == ' ')
                                charout[y][x] = charout[y][x + 1];
                        if (j > 0 && n == 0 && charout[y][x] == ' ')
                                charout[y][x] = charout[y - 1][x];
                        if (j < sY - 1 && n == sH - 1 && charout[y][x] == ' ')
                                charout[y][x] = charout[y + 1][x];
                }
        }
}

// move a point randomly from a starting position with a screen, obeying solids
void wander(struct point * p, int steps)
{
        for (; steps > 0; steps--)
        {
                struct point p2 = *p;
                switch (rand()%4) {
                        case 0: p2.x--; break;
                        case 1: p2.y--; break;
                        case 2: p2.x++; break;
                        case 3: p2.y++; break;
                }

                // o-o-b?
                if (p2.x < 0 || p2.x > sX * sW - 1 || p2.y < 0 || p2.y > sY * sH - 1)
                        continue;

                // too close to screen edge?
                int modx = p2.x % sW;
                int mody = p2.y % sH;
                if (modx < 2 || modx > sW - 3 || mody < 2 || mody > sH -3)
                        continue;

                // blocked?
                if (charout[p2.y][p2.x] != ' ')
                        continue;

                *p = p2;
        }
}

// pick some points on each screen for testing global connectivity
void set_key_points()
{
        int i, j;
        int k = 0;
        // i,j identify the room
        for (i = 0; i < sX; i++) for (j = 0; j < sY; j++)
        {
                // randomly pick one of the 4 quadrant centers
                struct point p;
                p.x = i * sW + sW4 + (sW2 * (rand() % 2));
                p.y = j * sH + sH4 + (sH2 * (rand() % 2));

                wander(&p, 10);
                key_points[k++] = p;
                charout[p.y][p.x] = '.';
        }
}

// flood-fill used by are_key_points_connected
int flood(int x, int y)
{
        int ttl = 0;

        if (x < 0 || x > sX * sW - 1 || y < 0 || y > sY * sH - 1) // out of bounds
                return 0;
        else if (flood_buf[y][x]) // already visited
                return 0;
        else if (charout[y][x] == ' ') // open
                ;
        else if (charout[y][x] == '.') // key point
                ttl++;
        else
                return 0; // blocked

        flood_buf[y][x] = 1;
        return ttl
                + flood(x - 1, y)
                + flood(x + 1, y)
                + flood(x, y - 1)
                + flood(x, y + 1);
}

// returns true if all key points are accessible from each other
int are_key_points_connected()
{
        memset(flood_buf, 0, sizeof flood_buf);
        int num_found = flood(key_points[0].x, key_points[0].y);
        return num_found == NUM_KEY_POINTS;
}

// just print charout to the screen
void print_charout()
{
        int i;
        for (i = 0; i < sY * sH; i++)
                printf("%.*s\n", sX * sW, charout[i]);
}

// finds the most common obstacle in a recatangle
int find_common_obstacle(int x0, int x1, int y0, int y1)
{
        int histo[256] = {};
        int x, y, i;
        int best = 0;
        int best_idx = ' ';

        for (x = x0; x < x1; x++) for (y = y0; y < y1; y++)
                histo[charout[y][x]]++;

        for (i = 0; i < 256; i++) if (histo[i] > best)
        {
                best = histo[i];
                best_idx = i;
        }

        return best_idx;
}

// randomly place things w/o breaking the connectivity of key points
void add_random_obstacles_and_openings()
{
        int n;
        for (n = 0; n < OBSTACLE_ATTEMPTS; n++) {
                int w = 2 + rand() % 7;
                int h = 2 + rand() % 7;
                int x0 = 2 + rand() % (sX * sW - w - 4);
                int y0 = 2 + rand() % (sY * sH - h - 4);
                int x1 = x0 + w;
                int y1 = y0 + h;
                int x, y;
                int new_char = ' ';

                int opening = (rand() % OPENINGS_MOD == 0);

                // no edges super close to screen borders
                if (x0 % sW == sW - 2) x0 -= 0 + opening;
                if (x0 % sW == sW - 1) x0 -= 1 + opening;
                if (x0 % sW == 0     ) x0 -= 2 + opening;
                if (x0 % sW == 1     ) x0 -= 3 + opening;
                if (x0 % sW == 2     ) x0 -= 4 + opening;
                if (x0 < 0) x0 = 0;
                if (y0 % sH == sH - 2) y0 -= 0 + opening;
                if (y0 % sH == sH - 1) y0 -= 1 + opening;
                if (y0 % sH == 0     ) y0 -= 2 + opening;
                if (y0 % sH == 1     ) y0 -= 3 + opening;
                if (y0 % sH == 2     ) y0 -= 4 + opening;
                if (y0 < 0) y0 = 0;
                if (x1 % sW == sW - 2) x1 += 4 + opening;
                if (x1 % sW == sW - 1) x1 += 3 + opening;
                if (x1 % sW == 0     ) x1 += 2 + opening;
                if (x1 % sW == 1     ) x1 += 1 + opening;
                if (x1 % sW == 2     ) x1 += 0 + opening;
                if (x1 > sX * sW - 1 ) x1 = sX * sW - 1;
                if (y1 % sH == sH - 2) y1 += 4 + opening;
                if (y1 % sH == sH - 1) y1 += 3 + opening;
                if (y1 % sH == 0     ) y1 += 2 + opening;
                if (y1 % sH == 1     ) y1 += 1 + opening;
                if (y1 % sH == 2     ) y1 += 0 + opening;
                if (y1 > sY * sH - 1 ) y1 = sY * sH - 1;

                char common = find_common_obstacle(x0, x1, y0, y1);

                // block it
                for (x = x0; x < x1; x++) for (y = y0; y < y1; y++)
                {
                        if (charout[y][x] == ' ')
                                charout[y][x] = '?';
                }

                if (are_key_points_connected())
                {
                        if (common != ' ' && common != '.')
                        {
                                new_char = common;
                        }
                        else switch (3 * n / OBSTACLE_ATTEMPTS)
                        {
                                case 0: new_char = 'W'; break;
                                case 1: new_char = 'S'; break;
                                case 2: new_char = 'T'; break;
                        }
                }

                int no_edge = (x0 > 1 && y0 > 1 && x1 < sX * sW - 2 && y1 < sY * sH - 2);
                int no_corner = (((x0 / sW) == (x1 / sW)) || ((y0 / sH) == (y1 / sH)));
                opening = opening && no_edge && no_corner;

                // confirm or undo blockage
                for (x = x0; x < x1; x++) for (y = y0; y < y1; y++) {
                        if (opening)
                        {
                                if (charout[y][x] != '.')
                                        charout[y][x] = ' ';
                        }
                        else
                        {
                                if (charout[y][x] == '?')
                                        charout[y][x] = new_char;
                        }
                }
        }
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

        convert_orooms_to_charout();

        bleed_edges();
        set_key_points();
        if (!are_key_points_connected())
        {
                fprintf(stderr, "failure: key points initially unconnected\n");
                exit(1);
        }
        add_random_obstacles_and_openings();

        print_charout();
}
