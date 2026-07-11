#include "zel.c"
#ifndef ZEL_OW_GEN_C
#define ZEL_OW_GEN_C

#define sX DUNW // overworld screens across (srooms)
#define sY DUNH // overworld screens down
#define oX (sX*2) // subscreen count (each screen is 4 subscreens) (orooms)
#define oY (sY*2)
#define sW TILESW // size of each screen
#define sH TILESH
#define sW2 (sW/2)
#define sH2 (sH/2)
#define sW4 (sW/4)
#define sH4 (sH/4)
#define pW 3 // size of subscreen openings in preview printout
#define pH 2

#define LOOPINESS 0.66f // how often to accept a random door that makes a loop, 0.0-1.0
#define EW_BIAS 0.65f // how often to choose an east-west door over north-south, 0.0-1.0
#define RECTS 6
#define NUM_KEY_POINTS (sX * sY)
#define OBSTACLE_ATTEMPTS (sX * sY * 4)
#define GRID_ATTEMPTS (sX * sY * 15)
#define RIVER_ATTEMPTS (sX * sY * 9)
#define OPENINGS_MOD 2

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

unsigned char charout[sY * sH][sX * sW];
unsigned char flood_buf[sY * sH][sX * sW];

struct point {
        int x, y;
};

struct point key_points[NUM_KEY_POINTS];

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
                /*
                    +===+===+===+===+===+===+
                    #   |i  #i+1|   #   |   #
                    #   |j-1#j-1|   #   |   #
                    +---+---+---+---+---+---+
                    #   |i  #   |   #   |   #
                    #   |j  #   |   #   |   #
                    +===+===+===+===+===+===+
                    #   |   #   |   #   |   #
                    #   |   #   |   #   |   #
                    +---+---+---+---+---+---+
                    #   |   #   |   #i-1|i  #
                    #   |   #   |   #j  |j  #
                    +===+===+===+===+===+===+
                                     i-1
                                     j+1
                */
                if (j < oY - 1
                                && oroom[i - 1][j].open_r && oroom[i - 1][j + 1].open_r
                                && (oroom[i - 1][j].open_d || oroom[i][j].open_d))
                        oroom[i - 1][j].open_d = oroom[i][j].open_d = true;

                if (i < oX - 1
                                && oroom[i][j - 1].open_d && oroom[i + 1][j - 1].open_d
                                && (oroom[i][j - 1].open_r || oroom[i][j].open_r))
                        oroom[i][j - 1].open_r = oroom[i][j].open_r = true;
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
                        case 3: screen_char = rand() % 4 ? 'R' : 'W'; break;
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
                switch (rand()%4)
                {
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

int is_open(char c)
{
        return (c == ' ' || c == '.' || c == 'B' || c == 'A');
}

// flood-fill used by are_key_points_connected()
int flood(int x, int y)
{
        int ttl = 0;

        if (x < 0 || x > sX * sW - 1 || y < 0 || y > sY * sH - 1) // out of bounds
                return 0;
        else if (flood_buf[y][x]) // already visited
                return 0;
        else if (charout[y][x] == '.') // key point
                ttl++;
        else if (is_open(charout[y][x])) // open
                ;
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
        for (int i = 0; i < sY * sH; i++)
                printf("%.*s\n", sX * sW, charout[i]);
}

int impassable(char c)
{
        return (c != ' ' && c != '.' && c != 'B' && c != 'A');
}

void print_utf8out()
{
        char *boxes = " \0\0\0▘\0▝\0▀\0▖\0▌\0▞\0▛\0▗\0▚\0▐\0▜\0▄\0▙\0▟\0█";
        for (int i = 0; i < sY * sH - 1; i += 2)
        {
                for (int j = 0; j < sX * sW - 1; j += 2)
                {
                        int pos =
                                1 * impassable(charout[i  ][j  ]) +
                                2 * impassable(charout[i  ][j+1]) +
                                4 * impassable(charout[i+1][j  ]) +
                                8 * impassable(charout[i+1][j+1]);
                        printf("%s", boxes + (pos * 4));
                }
                printf("\n");
        }
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
        for (int n = 0; n < OBSTACLE_ATTEMPTS; n++)
        {
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
                for (x = x0; x < x1; x++) for (y = y0; y < y1; y++)
                {
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

int can_flow_into(char c)
{
        return c != ' ' && c != '.' && c != 'B' && c != 'V';
}

// a bridge tile must land on open ground to its east and west
int bridgeable(int y, int x)
{
        return x > 0 && x < sX * sW - 1
                && is_open(charout[y][x - 1])
                && is_open(charout[y][x + 1]);
}

void river_flow()
{
        for (int n = 0; n < RIVER_ATTEMPTS; n++)
        {
                int x = rand() % (sX * sW);
                int y = rand() % (sY * sH);
                int east_flow = rand() % 2;

                // must start in a body of water or far north
                if (y != 0 && charout[y][x] != 'W') continue;

                while (charout[y][x] == 'W' && y < sY * sH - 1)
                        y++;

                if (can_flow_into(charout[y][x])) while (1)
                {
                        if (charout[y][x] == 'W' || charout[y][x] == 'V') break;

                        charout[y][x] = 'V';
                        int ew_ok = (y > 1); // no east/west along the top border
                        if (y >= sY * sH - 1)
                        {
                                break;
                        }
                        else if (can_flow_into(charout[y + 1][x]))
                        {
                                y++;
                        }
                        else if (ew_ok && east_flow && x < sX * sW - 3 && can_flow_into(charout[y][x + 1]))
                        {
                                x++;
                        }
                        else if (ew_ok && x > 2 && can_flow_into(charout[y][x - 1]))
                        {
                                x--;
                        }
                        else if (ew_ok && x < sX * sW - 3 && can_flow_into(charout[y][x + 1]))
                        {
                                x++;
                        }
                        else if (charout[y + 1][x] == ' ') // flow over land
                        {
                                y++;
                                int ystart = y;
                                while (y < sY * sH - 1 && charout[y][x] == ' ')
                                {
                                        charout[y][x] = '?';
                                        y++;
                                }

                                // add bridge on a row with open ground both sides
                                int randy = -1;
                                int ncand = 0;
                                for (int yy = ystart; yy < y; yy++)
                                        if (bridgeable(yy, x))
                                                ncand++;
                                if (ncand)
                                {
                                        int pick = rand() % ncand;
                                        for (int yy = ystart; yy < y; yy++)
                                                if (bridgeable(yy, x) && pick-- == 0)
                                                        randy = yy;
                                }

                                if (randy >= 0) charout[randy][x] = ' ';

                                int bridge_char = 'B';
                                int river_char = 'X';

                                // if this breaks connectivity, undo the river
                                if (!are_key_points_connected())
                                        bridge_char = river_char = ' ';

                                // confirm or undo river
                                if (randy >= 0) charout[randy][x] = bridge_char;
                                for (int yy = ystart; yy < y; yy++)
                                        if (charout[yy][x] == '?')
                                                charout[yy][x] = river_char;

                                if (y >= sY * sH - 1 || !can_flow_into(charout[y][x]))
                                        break;
                        }
                        else
                        {
                                break;
                        }
                }
        }
}

void add_random_grids()
{
        for (int n = 0; n < GRID_ATTEMPTS; n++)
        {
                int w = 3 + rand() % 9;
                int h = 3 + rand() % 7;
                int x0 = 2 + rand() % (sX * sW - w - 4);
                int y0 = 2 + rand() % (sY * sH - h - 4);
                int x1 = x0 + w;
                int y1 = y0 + h;
                int x, y;
                int grid_char = rand() % 2 ? 'T' : 'S';
                int sum = 0;

                if (x0 % sW == sW - 1) x0 -= 1;
                if (x0 % sW == 0)      x0 -= 2;
                if (y0 % sH == sH - 1) y0 -= 1;
                if (y0 % sH == 0)      y0 -= 2;
                if (x1 % sW == 0)      x1 += 2;
                if (x1 % sW == 1)      x1 += 1;
                if (y1 % sH == 0)      y1 += 2;
                if (y1 % sH == 1)      y1 += 1;

                for (x = x0; x < x1; x++) for (y = y0; y < y1; y++)
                        sum += charout[y][x] != ' ' && charout[y][x] != '.';

                if (sum) continue;

                for (x = x0 + 1; x < x1 - 1; x++) for (y = y0 + 1; y < y1 - 1; y++)
                {
                        if (charout[y][x] == '.') continue;

                        int xedge = (x + 1) % sW < 2;
                        int yedge = (y + 1) % sH < 2;
                        if (((x - x0) % 2 || xedge) && ((y - y0) % 2 || yedge))
                                charout[y][x] = grid_char;
                }
        }
}

int is_smoothable(char c)
{
        return (c == 'T' || c == 'R' || c == 'S');
}

int is_smoothing(char c)
{
        return (c == 'T' || c == 'R' || c == 'S' || c == 'W');
}

// get rid of noise within solid areas
void smoothen()
{
        for (int i = 1; i < sY * sH - 1; i++) for (int j = 1; j < sX * sW - 1; j++)
        {
                char a = charout[i + 1][j];
                char b = charout[i - 1][j];
                char c = charout[i][j + 1];
                char d = charout[i][j - 1];

                if (!is_smoothable(charout[i][j]))
                        continue;

                if (is_smoothing(a) && a == b)
                {
                        if (charout[i][j] != a)
                                charout[i][j] = a;
                }
                else if (is_smoothing(c) && c == d)
                {
                        if (charout[i][j] != c)
                                charout[i][j] = c;
                }
        }

}

int is_solid(char c)
{
        return (c == 'T' || c == 'R' || c == 'S' || c == 'W' || c == 'V' || c == 'X' || c == 'P' || c == '?');
}

void make_cave(int k)
{
        int x = key_points[k].x;
        int y = key_points[k].y - 1;

        // try to put a cave north of key point
        for (; y % sH >= 1; y--)
        {
                if (!is_solid(charout[y][x])) continue;

                if (is_solid(charout[y - 1][x])
                                && is_solid(charout[y][x - 1])
                                && is_solid(charout[y][x + 1]))
                {
                        charout[y][x] = 'C';
                        return;
                }
                break;
        }

        // try to build a door + frame near the key point:
        // apply the whole change tentatively (frame + cave + displaced key
        // point), check connectivity of the final state, commit or undo
        for (y = key_points[k].y; y % sH > 2; y--)
        {
                struct point f[5] = {{x-1,y-1},{x,y-1},{x+1,y-1},{x-1,y},{x+1,y}};
                unsigned char fsave[5], csave, dsave;
                int at_kp = (y == key_points[k].y);

                csave = charout[y][x];
                dsave = charout[y + 1][x];
                if (at_kp && dsave != ' ') continue; // nowhere safe to move the key point

                for (int i = 0; i < 5; i++)
                {
                        fsave[i] = charout[f[i].y][f[i].x];
                        if (fsave[i] == ' ') charout[f[i].y][f[i].x] = '?';
                }
                charout[y][x] = 'C';
                if (at_kp) charout[y + 1][x] = '.';

                if (are_key_points_connected())
                {
                        for (int i = 0; i < 5; i++)
                                if (is_solid(charout[f[i].y][f[i].x]))
                                        charout[f[i].y][f[i].x] = 'P';
                        if (at_kp) key_points[k].y++;
                        return;
                }

                // undo everything, try one row higher
                for (int i = 0; i < 5; i++) charout[f[i].y][f[i].x] = fsave[i];
                charout[y][x] = csave;
                charout[y + 1][x] = dsave;
        }
}

int walkable(char c)
{
        return (c == ' ' || c == '.' || c == 'B' || c == 'C' || c == 'A');
}

#define MAX_APPLES 64
struct point apples[MAX_APPLES];
int num_apples;

// the outermost tile ring of each screen
int on_screen_edge(int x, int y)
{
        return (x % sW == 0 || x % sW == sW - 1 || y % sH == 0 || y % sH == sH - 1);
}

// a dead end is a walkable part of a screen that can be entered from exactly
// one other screen and holds no cave door. put an apple at its far end.
void place_apples()
{
        // sealed-off pockets can pass the local dead-end test; only reward
        // areas the player can actually reach
        memset(flood_buf, 0, sizeof flood_buf);
        flood(key_points[0].x, key_points[0].y);

        for (int i = 0; i < sX; i++) for (int j = 0; j < sY; j++)
        {
                int comp[sH][sW];
                int cid = 0;
                memset(comp, 0, sizeof comp);

                for (int m = 0; m < sW; m++) for (int n = 0; n < sH; n++)
                {
                        if (comp[n][m] || !walkable(charout[j * sH + n][i * sW + m]))
                                continue;

                        // flood the walkable component containing (m,n)
                        struct point stack[sW * sH];
                        int sp = 0;
                        int entrances = 0;
                        int has_door = 0;

                        cid++;
                        comp[n][m] = cid;
                        stack[sp++] = (struct point){m, n};
                        while (sp)
                        {
                                struct point p = stack[--sp];
                                int gx = i * sW + p.x;
                                int gy = j * sH + p.y;

                                if (charout[gy][gx] == 'C') has_door = 1;
                                if (p.x == 0      && i > 0      && walkable(charout[gy][gx - 1])) entrances |= 1;
                                if (p.x == sW - 1 && i < sX - 1 && walkable(charout[gy][gx + 1])) entrances |= 2;
                                if (p.y == 0      && j > 0      && walkable(charout[gy - 1][gx])) entrances |= 4;
                                if (p.y == sH - 1 && j < sY - 1 && walkable(charout[gy + 1][gx])) entrances |= 8;

                                struct point q[4] = {{p.x-1,p.y}, {p.x+1,p.y}, {p.x,p.y-1}, {p.x,p.y+1}};
                                for (int d = 0; d < 4; d++)
                                {
                                        if (q[d].x < 0 || q[d].x >= sW || q[d].y < 0 || q[d].y >= sH) continue;
                                        if (comp[q[d].y][q[d].x]) continue;
                                        if (!walkable(charout[j * sH + q[d].y][i * sW + q[d].x])) continue;
                                        comp[q[d].y][q[d].x] = cid;
                                        stack[sp++] = (struct point){q[d].x, q[d].y};
                                }
                        }

                        if (has_door) continue;
                        if (entrances == 0) continue;               // sealed pocket
                        if (entrances & (entrances - 1)) continue;  // 2+ other screens

                        // BFS from the entrance edge to find the farthest open tile
                        int dist[sH][sW];
                        struct point queue[sW * sH];
                        int qh = 0, qt = 0;
                        memset(dist, -1, sizeof dist);

                        for (int m2 = 0; m2 < sW; m2++) for (int n2 = 0; n2 < sH; n2++)
                        {
                                if (comp[n2][m2] != cid) continue;
                                if ((entrances == 1 && m2 == 0)
                                                || (entrances == 2 && m2 == sW - 1)
                                                || (entrances == 4 && n2 == 0)
                                                || (entrances == 8 && n2 == sH - 1))
                                {
                                        dist[n2][m2] = 0;
                                        queue[qt++] = (struct point){m2, n2};
                                }
                        }

                        int bx = -1, by = -1, bd = -1;
                        while (qh < qt)
                        {
                                struct point p = queue[qh++];

                                if (charout[j * sH + p.y][i * sW + p.x] == ' '
                                                && !on_screen_edge(p.x, p.y)
                                                && dist[p.y][p.x] > bd)
                                {
                                        bd = dist[p.y][p.x];
                                        bx = i * sW + p.x;
                                        by = j * sH + p.y;
                                }

                                struct point q[4] = {{p.x-1,p.y}, {p.x+1,p.y}, {p.x,p.y-1}, {p.x,p.y+1}};
                                for (int d = 0; d < 4; d++)
                                {
                                        if (q[d].x < 0 || q[d].x >= sW || q[d].y < 0 || q[d].y >= sH) continue;
                                        if (comp[q[d].y][q[d].x] != cid || dist[q[d].y][q[d].x] >= 0) continue;
                                        dist[q[d].y][q[d].x] = dist[p.y][p.x] + 1;
                                        queue[qt++] = (struct point){q[d].x, q[d].y};
                                }
                        }

                        if (bx >= 0 && flood_buf[by][bx] && num_apples < MAX_APPLES)
                        {
                                charout[by][bx] = 'A';
                                apples[num_apples++] = (struct point){bx, by};
                        }
                }
        }
}

// convert bridges to nowhere (fewer than 2 walkable neighbors) into water
int fix_bridges()
{
        int total = 0, changed;
        do {
                changed = 0;
                for (int i = 0; i < sY * sH; i++) for (int j = 0; j < sX * sW; j++)
                {
                        if (charout[i][j] != 'B') continue;

                        // a bridge on a screen border with a walkable partner
                        // is a crossing that must stay (see match_borders)
                        if (j % sW == sW - 1 && j < sX * sW - 1 && walkable(charout[i][j + 1])) continue;
                        if (j % sW == 0      && j > 0           && walkable(charout[i][j - 1])) continue;
                        if (i % sH == sH - 1 && i < sY * sH - 1 && walkable(charout[i + 1][j])) continue;
                        if (i % sH == 0      && i > 0           && walkable(charout[i - 1][j])) continue;

                        int n = 0;
                        n += (i > 0           && walkable(charout[i - 1][j]));
                        n += (i < sY * sH - 1 && walkable(charout[i + 1][j]));
                        n += (j > 0           && walkable(charout[i][j - 1]));
                        n += (j < sX * sW - 1 && walkable(charout[i][j + 1]));
                        if (n >= 2) continue;

                        // become a solid neighbor, preferring the water around
                        unsigned char c = 0;
                        unsigned char nb[4] = {
                                i > 0           ? charout[i - 1][j] : 'W',
                                i < sY * sH - 1 ? charout[i + 1][j] : 'W',
                                j > 0           ? charout[i][j - 1] : 'W',
                                j < sX * sW - 1 ? charout[i][j + 1] : 'W',
                        };
                        for (int k = 0; k < 4; k++)
                                if (is_solid(nb[k])) c = nb[k];
                        for (int k = 0; k < 4; k++)
                                if (nb[k] == 'V' || nb[k] == 'X' || nb[k] == 'W') c = nb[k];

                        charout[i][j] = c ? c : 'W';
                        if (!are_key_points_connected())
                                charout[i][j] = 'B'; // somebody needs this walkway
                        else
                                changed++;
                }
                total += changed;
        } while (changed);
        return total;
}

// rivers etc. can block one side of a shared screen border while the other
// side stays walkable, letting the player walk across into a solid tile.
// force openness/blockedness to match on every abutting pair.
void match_border_pair(int ay, int ax, int by, int bx)
{
        unsigned char *a = &charout[ay][ax];
        unsigned char *b = &charout[by][bx];

        if (is_solid(*a) == is_solid(*b)) return;

        unsigned char *open  = is_solid(*a) ? b : a;
        unsigned char *solid = is_solid(*a) ? a : b;
        unsigned char open_was = *open;

        // prefer extending the blockage across the border
        *open = *solid;
        if (are_key_points_connected()) return;

        // that would trap something - open the blocked side instead
        *open = open_was;
        *solid = (*solid == 'W' || *solid == 'V' || *solid == 'X') ? 'B' : ' ';
}

void match_borders()
{
        int i, k;

        for (i = 1; i < sX; i++) for (k = 0; k < sY * sH; k++)
                match_border_pair(k, i * sW - 1, k, i * sW);

        for (i = 1; i < sY; i++) for (k = 0; k < sX * sW; k++)
                match_border_pair(i * sH - 1, k, i * sH, k);
}

#define MAX_BREAKS (MAX_APPLES * 4)
struct point breakables[MAX_BREAKS];
int num_breaks;

int breakable_type(char c)
{
        return (c == 'T' || c == 'R' || c == 'S');
}

int already_breakable(int x, int y)
{
        for (int k = 0; k < num_breaks; k++)
                if (breakables[k].x == x && breakables[k].y == y)
                        return 1;
        return 0;
}

// flood walkable tiles from (x,y) into buf; stays on one screen if bound
void area_flood(unsigned char (*buf)[sX * sW], int x, int y, int bound)
{
        static struct point stack[sX * sW * sY * sH];
        int si = x / sW, sj = y / sH;
        int sp = 0;

        if (buf[y][x] || !walkable(charout[y][x])) return;
        buf[y][x] = 1;
        stack[sp++] = (struct point){x, y};

        while (sp)
        {
                struct point p = stack[--sp];
                struct point q[4] = {{p.x-1,p.y}, {p.x+1,p.y}, {p.x,p.y-1}, {p.x,p.y+1}};
                for (int d = 0; d < 4; d++)
                {
                        if (q[d].x < 0 || q[d].x >= sX * sW || q[d].y < 0 || q[d].y >= sY * sH) continue;
                        if (bound && (q[d].x / sW != si || q[d].y / sH != sj)) continue;
                        if (buf[q[d].y][q[d].x] || !walkable(charout[q[d].y][q[d].x])) continue;
                        buf[q[d].y][q[d].x] = 1;
                        stack[sp++] = (struct point){q[d].x, q[d].y};
                }
        }
}

// does (x,y) connect to a tile of buf[][] without leaving its screen?
int links_back(int x, int y, unsigned char (*buf)[sX * sW])
{
        static unsigned char seen[sY * sH][sX * sW];
        memset(seen, 0, sizeof seen);
        area_flood(seen, x, y, 1);
        for (int i = 0; i < sY * sH; i++) for (int j = 0; j < sX * sW; j++)
                if (seen[i][j] && buf[i][j])
                        return 1;
        return 0;
}

#define POCKET_MAX 6

int in_pocket(struct point *pock, int np, int x, int y)
{
        for (int i = 0; i < np; i++)
                if (pock[i].x == x && pock[i].y == y)
                        return 1;
        return 0;
}

// can (x,y) join the pocket without leaking? every neighbor must be solid,
// already in the pocket, or the wall itself
int carve_ok(struct point *pock, int np, struct point w, int x, int y)
{
        if (on_screen_edge(x, y)) return 0;
        if (!breakable_type(charout[y][x])) return 0;

        struct point q[4] = {{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}};
        for (int d = 0; d < 4; d++)
        {
                if (q[d].x == w.x && q[d].y == w.y) continue;
                if (in_pocket(pock, np, q[d].x, q[d].y)) continue;
                if (!is_solid(charout[q[d].y][q[d].x])) return 0;
        }
        return 1;
}

// carve a small hidden pocket into the solid mass behind wall w. it touches
// walkable ground only through w. returns the number of tiles carved.
int carve_pocket(struct point w)
{
        struct point pock[POCKET_MAX];
        int np = 0;

        // seed from around the wall, then grow off pocket tiles
        for (int scan = -1; scan < np && np < POCKET_MAX; scan++)
        {
                int cx = scan < 0 ? w.x : pock[scan].x;
                int cy = scan < 0 ? w.y : pock[scan].y;
                struct point q[4] = {{cx-1,cy}, {cx+1,cy}, {cx,cy-1}, {cx,cy+1}};
                for (int d = 0; d < 4 && np < POCKET_MAX; d++)
                {
                        if (q[d].x < 1 || q[d].x > sX * sW - 2
                                        || q[d].y < 1 || q[d].y > sY * sH - 2) continue;
                        if (in_pocket(pock, np, q[d].x, q[d].y)) continue;
                        if (carve_ok(pock, np, w, q[d].x, q[d].y))
                                pock[np++] = q[d];
                }
        }

        if (np < 2) return 0; // a 1-tile pocket is too cramped

        for (int i = 0; i < np; i++)
                charout[pock[i].y][pock[i].x] = ' ';

        return np;
}

int max_secret_depth; // most breakable walls between the world and an apple
int num_shortcuts;    // dead ends that got a breakable shortcut wall

static unsigned char reach[sY * sH][sX * sW];  // all tiles the player can get to
static unsigned char reach0[sY * sH][sX * sW]; // ...before any secrets existed
static unsigned char area[sY * sH][sX * sW];   // the apple's current sub-area
static unsigned char prev[sY * sH][sX * sW];   // every area this chain has visited
static unsigned char wmark[sY * sH][sX * sW];
static struct point cand[3][512]; // walls with: natural secret behind (0),
static int nc[3];                 // shortcut (1), solid mass to carve (2)

// classify the breakable-wall candidates around `area`
void collect_walls()
{
        int dx[4] = {-1, 1, 0, 0};
        int dy[4] = {0, 0, -1, 1};

        nc[0] = nc[1] = nc[2] = 0;
        memset(wmark, 0, sizeof wmark);

        for (int y = 0; y < sY * sH; y++) for (int x = 0; x < sX * sW; x++)
        {
                if (!area[y][x]) continue;
                for (int d = 0; d < 4; d++)
                {
                        int wx = x + dx[d], wy = y + dy[d];
                        if (wx < 1 || wx > sX * sW - 2 || wy < 1 || wy > sY * sH - 2) continue;
                        // edge tiles must match their partner across the
                        // border (see match_borders), so never breakable
                        if (on_screen_edge(wx, wy)) continue;
                        if (wmark[wy][wx]) continue;
                        if (!breakable_type(charout[wy][wx])) continue;
                        if (already_breakable(wx, wy)) continue;

                        // never directly next to a cave mouth
                        if (charout[wy - 1][wx] == 'C' || charout[wy + 1][wx] == 'C'
                                        || charout[wy][wx - 1] == 'C'
                                        || charout[wy][wx + 1] == 'C') continue;

                        // never crack a wall that leads back to an area this
                        // chain already visited - unless getting there
                        // normally would cross other screens
                        int secret = 0, shortcut = 0, reject = 0;
                        int open_beyond = 0, carvable = 0;
                        for (int e = 0; e < 4; e++)
                        {
                                int px = wx + dx[e], py = wy + dy[e];
                                if (area[py][px]) continue;
                                if (walkable(charout[py][px]))
                                {
                                        open_beyond = 1;
                                        if (!reach[py][px])
                                                secret = 1;
                                        else if (!reach0[py][px])
                                                reject = 1; // another chain's secret
                                        else if (links_back(px, py, prev))
                                                reject = 1; // backdoor
                                        else
                                                shortcut = 1;
                                }
                                else if (!on_screen_edge(px, py)
                                                && breakable_type(charout[py][px]))
                                        carvable = 1;
                        }
                        if (reject) continue;

                        int cls = secret ? 0 :
                                  (!open_beyond && carvable) ? 2 :
                                  shortcut ? 1 : -1;
                        if (cls < 0) continue;

                        wmark[wy][wx] = 1;
                        if (nc[cls] < 512)
                                cand[cls][nc[cls]++] = (struct point){wx, wy};
                }
        }
}

// from each apple's dead end, crack a wall that opens something new. if it
// hides an unreachable pocket, move the apple in and repeat, up to 4 deep
void add_secrets()
{
        int dx[4] = {-1, 1, 0, 0};
        int dy[4] = {0, 0, -1, 1};

        memset(flood_buf, 0, sizeof flood_buf);
        flood(key_points[0].x, key_points[0].y);
        memcpy(reach, flood_buf, sizeof reach);
        memcpy(reach0, flood_buf, sizeof reach0);
        max_secret_depth = 0;
        num_shortcuts = 0;

        for (int k = 0; k < num_apples; k++)
        {
                int depth = 0;
                memset(area, 0, sizeof area);
                area_flood(area, apples[k].x, apples[k].y, 1);
                memcpy(prev, area, sizeof prev);

                // every dead end gets one breakable shortcut to the wider
                // world, when a qualifying wall exists
                collect_walls();
                if (nc[1] && num_breaks < MAX_BREAKS)
                {
                        breakables[num_breaks++] = cand[1][rand() % nc[1]];
                        num_shortcuts++;
                }

                for (int iter = 0; iter < 4 && num_breaks < MAX_BREAKS; iter++)
                {
                        collect_walls();

                        // prefer a natural secret pocket; else carve one
                        struct point w;
                        int is_secret = 0;

                        if (nc[0])
                        {
                                w = cand[0][rand() % nc[0]];
                                is_secret = 1;
                        }
                        else while (nc[2])
                        {
                                int r = rand() % nc[2];
                                w = cand[2][r];
                                cand[2][r] = cand[2][--nc[2]];
                                if (carve_pocket(w))
                                {
                                        is_secret = 1;
                                        break;
                                }
                        }

                        if (!is_secret) break;

                        breakables[num_breaks++] = w;

                        // the hidden pocket becomes the new area
                        memset(area, 0, sizeof area);
                        for (int e = 0; e < 4; e++)
                        {
                                int px = w.x + dx[e], py = w.y + dy[e];
                                if (walkable(charout[py][px]) && !reach[py][px])
                                        area_flood(area, px, py, 0);
                        }

                        // move the apple to the pocket's farthest open tile
                        static int dist[sY * sH][sX * sW];
                        static struct point queue[sY * sH * sX * sW];
                        int qh = 0, qt = 0;
                        int bx = -1, by = -1, bd = -1;
                        memset(dist, -1, sizeof dist);
                        for (int e = 0; e < 4; e++)
                        {
                                int px = w.x + dx[e], py = w.y + dy[e];
                                if (!area[py][px] || dist[py][px] >= 0) continue;
                                dist[py][px] = 0;
                                queue[qt++] = (struct point){px, py};
                        }
                        while (qh < qt)
                        {
                                struct point p = queue[qh++];
                                if (charout[p.y][p.x] == ' '
                                                && !on_screen_edge(p.x, p.y)
                                                && dist[p.y][p.x] > bd)
                                {
                                        bd = dist[p.y][p.x];
                                        bx = p.x;
                                        by = p.y;
                                }
                                for (int e = 0; e < 4; e++)
                                {
                                        int px = p.x + dx[e], py = p.y + dy[e];
                                        if (!area[py][px] || dist[py][px] >= 0) continue;
                                        dist[py][px] = dist[p.y][p.x] + 1;
                                        queue[qt++] = (struct point){px, py};
                                }
                        }

                        // pocket is now (secretly) reachable, and part of this chain
                        for (int y = 0; y < sY * sH; y++) for (int x = 0; x < sX * sW; x++)
                                if (area[y][x])
                                        reach[y][x] = prev[y][x] = 1;

                        if (bx < 0) break; // nowhere to put the apple

                        charout[apples[k].y][apples[k].x] = ' ';
                        charout[by][bx] = 'A';
                        apples[k] = (struct point){bx, by};

                        depth++;
                        if (depth > max_secret_depth)
                                max_secret_depth = depth;
                }
        }
}

int ow_gen_once()
{
        int traverse_count = 0;

        memset(oroom, 0, sizeof oroom);
        num_apples = 0;
        num_breaks = 0;

        //cook_corners();
        cook_squares();

        while (traverse_count < oX * oY)
        {
                open_random_door(LOOPINESS);
                traverse_count = traverse(oX / 2, oY - 1);
        }

        while (open_a_useful_door(38))
                traverse(oX / 2, oY - 1);

        cook_squares_again();

        convert_orooms_to_charout();

        bleed_edges();
        set_key_points();
        if (!are_key_points_connected())
                return 0; // basically never - retry

        add_random_obstacles_and_openings();
        river_flow();
        add_random_grids();
        smoothen();

        for (int k = 0; k < NUM_KEY_POINTS; k++)
                make_cave(k);

        fix_bridges();
        match_borders();
        fix_bridges(); // match_borders may orphan a bridge landing

        place_apples();
        add_secrets();

        return 1;
}

void ow_gen()
{
        srand(time(NULL));

        // regenerate until an apple hides 2+ breakable walls deep, up to
        // 100 tries. a valid connected map is required no matter what.
        for (int attempt = 1; ; attempt++)
        {
                if (!ow_gen_once())
                {
                        if (attempt > 1000)
                                exit(fprintf(stderr, "failure: no valid overworld\n"));
                        continue;
                }
                if (max_secret_depth >= 2 || attempt >= 100)
                        break;
        }

        print_orooms();
        //print_charout();
        print_utf8out();
}

#endif // ZEL_OW_GEN_C
