#include "tet.c"
#ifndef TET_MUSIC_C_INCLUDED
#define TET_MUSIC_C_INCLUDED

enum CHORDS {
        _None,
        _M,
        _11,
        _13,
        _13b9,
        _13v,
        _6,
        _7,
        _7b9,
        _7sus,
        _9,
        _9b5,
        _9s5,
        _M7,
        _M9,
        _add2,
        _aug,
        _dim,
        _dim7,
        _m,
        _m6,
        _m7,
        _m7b5,
        _m9,
        _sus,
        _o3,
        _o5,
        _dimob3,
        _4o1,
        _5o1,
        _5o2,
        CHORD_COUNT
};

struct chord {
        char name[8];
        union {
                struct {
                        int _1, _2, _b3, _3, _s3, _b5, _5, _s5, _6, _b7, _7, _8, _b9, _9, _b11, _11, _13;
                };
                int n[17];
        };
} chord[] = {
#define _ 1
        { .name="None" },
        { .name="",       ._1=_, ._3=_, ._5=_ },
        { .name="11",     ._b7=_, ._9=_, ._11=_ },
        { .name="13",     ._b7=_, ._8=_, ._b11=_, ._13=_ },
        { .name="13b9",   ._b7=_, ._b9=_, ._b11=_, ._13=_ },
        { .name="13x",    ._b7=_, ._9=_, ._11=_, ._13=_ },
        { .name="6",      ._1=_, ._3=_, ._5=_, ._6=_ },
        { .name="7",      ._1=_, ._3=_, ._5=_, ._b7=_ },
        { .name="7b9",    ._3=_, ._5=_, ._b7=_, ._b9=_ },
        { .name="7sus",   ._1=_, ._s3=_, ._5=_, ._b7=_ },
        { .name="9",      ._3=_, ._5=_, ._b7=_, ._9=_ },
        { .name="9b5",    ._3=_, ._b5=_, ._b7=_, ._9=_ },
        { .name="9s5",    ._3=_, ._s5=_, ._b7=_, ._9=_ },
        { .name="M7",     ._1=_, ._3=_, ._5=_, ._7=_ },
        { .name="M9",     ._3=_, ._5=_, ._7=_, ._9=_ },
        { .name="add2",   ._1=_, ._2=_, ._3=_, ._5=_ },
        { .name="aug",    ._1=_, ._3=_, ._s5=_ },
        { .name="dim",    ._1=_, ._b3=_, ._b5=_ },
        { .name="dim7",   ._1=_, ._b3=_, ._b5=_, ._6=_ },
        { .name="m",      ._1=_, ._b3=_, ._5=_ },
        { .name="m6",     ._1=_, ._b3=_, ._5=_, ._6=_ },
        { .name="m7",     ._1=_, ._b3=_, ._5=_, ._b7=_ },
        { .name="m7b5",   ._1=_, ._b3=_, ._b5=_, ._b7=_ },
        { .name="m9",     ._b3=_, ._5=_, ._b7=_, ._9=_ },
        { .name="sus",    ._1=_, ._s3=_, ._5=_ },
        { .name="/3",     ._3=_, ._5=_, ._8=_ },
        { .name="/5",     ._5=_, ._8=_, ._b11=_ },
        { .name="dim/b3", ._b3=_, ._b5=_, ._8=_ },
        { .name="4/1",    ._1=_, ._s3=_, ._6=_, ._8=_ },
        { .name="5/1",    ._1=_, ._2=_, ._5=_, ._7=_ },
        { .name="5/2",    ._2=_, ._5=_, ._7=_, ._9=_ },
#undef _
};

struct chord major_scale = {
        ._1=0, ._2=2, ._b3=3, ._3=4, ._s3=5, ._b5=6, ._5=7, ._s5=8, ._6=9, ._b7=10, ._7=11, ._8=12, ._b9=13, ._9=14, ._b11=16, ._11=17, ._13=21
};
struct chord mixolydian_scale = {
        ._1=0, ._2=2, ._b3=3, ._3=4, ._s3=5, ._b5=6, ._5=7, ._s5=8, ._6=9, ._b7=9, ._7=10, ._8=12, ._b9=13, ._9=14, ._b11=16, ._11=17, ._13=21
};

enum romans {
        Z_None,
        Z_I,
        Z_V,
        Z_iim,
        Z_IV,
        Z_vim,
        Z_iiim,
        Z_IVm7,
        Z_bII7,
        Z_Io5,
        Z_Io3,
        Z_II,
        Z_VI,
        Z_Ix,
        Z_III,
        Z_VII,
        Z_Vo2,
        Z_Im6,
        Z_Idimob3,
        Z_IIIm7b5,
        Z_Vm,
        Z_VIIm7b5,
        Z_sIVm7b5,
        Z_bVI7,
        Z_bVII9,
        Z_Idim7,
        Z_Vdim7,
        Z_IIdim7,
        Z_bVI,
        Z_bVII,
        Z_IVo1,
        Z_Vo1,
        Z_COUNT
};

struct chordnode {
        int roman;
        char name[10];
        int distance;
        int flavor[10];
        int resolv[10];
} chordnode[] =
{
        {Z_None,    "None",     0, {0},                    {0}},
        {Z_I,       "I",        0, {_M, _add2, _6, _M7, _M9, _sus}, {0}},
        {Z_V,       "V",        7, {_M, _7, _9, _11, _13, _sus}, {Z_I}},
        {Z_iim,     "iim",      2, {_m, _m7, _m9},         {Z_V, Z_Io5, Z_IVm7, Z_bII7, Z_Io3}},
        {Z_IV,      "IV",       5, {_M, _6, _M7, _m, _m6}, {Z_I, Z_V, Z_Io5, Z_iim, Z_Io3}},
        {Z_vim,     "vim",      9, {_m, _m7, _m9},         {Z_IV, Z_iim}},
        {Z_iiim,    "iiim",     4, {_m, _m7},              {Z_I, Z_IV, Z_vim}},
        {Z_IVm7,    "IVm7",     5, {_m7},                  {Z_I}},
        {Z_bII7,    "bII7",     1, {_7},                   {Z_I}},
        {Z_Io5,     "I/5",      0, {_o5},                  {Z_V}},
        {Z_Io3,     "I/3",      0, {_o3},                  {Z_IV, Z_iim}},
        {Z_II,      "II",       2, {_M, _7, _9, _7b9},     {Z_V}},
        {Z_VI,      "VI",       9, {_M, _7, _9, _7b9},     {Z_iim}},
        {Z_Ix,      "Ix",       0, {_7, _9, _7b9},         {Z_IV}},
        {Z_III,     "III",      4, {_M, _7, _9, _7b9},     {Z_vim}},
        {Z_VII,     "VII",     11, {_M, _7, _9, _7b9},     {Z_iiim}},
        {Z_Vo2,     "V/2",      0, {_5o2},                 {Z_II}},
        {Z_Im6,     "Im6",      0, {_m6},                  {Z_II}},
        {Z_Idimob3, "Idim/b3",  0, {_dimob3},              {Z_iim}},
        {Z_IIIm7b5, "IIIm7b5",  4, {_m7b5},                {Z_VI, Z_IV}},
        {Z_Vm,      "Vm",       7, {_m, _m7},              {Z_Ix}},
        {Z_VIIm7b5, "VIIm7b5", 11, {_m7b5},                {Z_III}},
        {Z_sIVm7b5, "#IVm7b5",  6, {_m7b5},                {Z_VII, Z_Io5, Z_V}},
        {Z_bVI7,    "bVI7",     8, {_7},                   {Z_Io5}},
        {Z_bVII9,   "bVII9",   10, {_9},                   {Z_Io5}},
        {Z_Idim7,   "Idim7",    0, {_dim7},                {Z_iim}},
        {Z_Vdim7,   "Vdim7",    7, {_dim7},                {Z_vim}},
        {Z_IIdim7,  "IIdim7",   2, {_dim7},                {Z_iiim}},
        {Z_bVI,     "bVI",      8, {_M},                   {Z_bVII, Z_bVII9}},
        {Z_bVII,    "bVII",    10, {_M},                   {Z_I}},
        {Z_IVo1,    "IV/1",     0, {_4o1},                 {Z_I}},
        {Z_Vo1,     "V/1",      0, {_5o1},                 {Z_I}},
};

char notenames[][8] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

void play_chord(bool actually_play, int *ch, int *dist)
{
        static int prev_node = Z_I;
        int n;

        // find next chord based on previous
        int nres = 0;
        for (int i = 0; i < 10; i++)
                if (chordnode[prev_node].resolv[i])
                        nres++;
        if (nres)
                n = chordnode[prev_node].resolv[rand() % nres];
        else
                n = rand() % (Z_COUNT-1) + 1;
        prev_node = n;

        // choose a flavor of the chord
        int nflav = 0;
        for (int i = 0; i < 10; i++)
                if (chordnode[n].flavor[i])
                        nflav++;
        int flav = rand() % nflav;

        *ch = chordnode[n].flavor[flav];
        *dist = chordnode[n].distance;

        //fprintf(stderr, "Playing a %s%s chord...\n",
        //        notenames[*dist], chord[*ch].name);

        if (actually_play)
        {
                int first = 1;
                #define MAYBE_PLAY(n) \
                        if (chord[*ch].n) { \
                                audio_tone(SQUARE, C3 + *dist + major_scale.n, C3 + major_scale.n, 10, 50, 350, 600); \
                                if (first) \
                                        audio_tone(SQUARE, C1 + *dist + major_scale.n, C1 + major_scale.n, 10, 50, 350, 600); \
                                first = 0; \
                        }
                MAYBE_PLAY(_1);
                MAYBE_PLAY(_2);
                MAYBE_PLAY(_b3);
                MAYBE_PLAY(_3);
                MAYBE_PLAY(_s3);
                MAYBE_PLAY(_b5);
                MAYBE_PLAY(_5);
                MAYBE_PLAY(_s5);
                MAYBE_PLAY(_6);
                MAYBE_PLAY(_b7);
                MAYBE_PLAY(_7);
                MAYBE_PLAY(_8);
                MAYBE_PLAY(_b9);
                MAYBE_PLAY(_9);
                MAYBE_PLAY(_b11);
                MAYBE_PLAY(_11);
                MAYBE_PLAY(_13);
                #undef MAYBE_PLAY
        }
}

void music_beat0_chord(int m, int b)
{
        int ch;
        int dist;
        play_chord(false, &ch, &dist);
        int c = 1;
        for (int i = 0; i < 17; i++)
        {
                if (chord[ch].n[i]) {
                        music[m][b+c*4][c] = (struct envelope){
                                .shape      = SQUARE,
                                .start_freq = NOTE2FREQ(C3 + dist + major_scale.n[i]),
                                .volume     = NOTE2VOL(C3 + dist + major_scale.n[i]) * 0.4,
                                .attack     =  100 / 1000.f,
                                .decay      =  120 / 1000.f,
                                .sustain    =  600 / 1000.f,
                                .release    =  800 / 1000.f,
                        };
                        if (c == 1)
                                music[m][b][0] = (struct envelope){
                                .shape      = SQUARE,
                                .start_freq = NOTE2FREQ(C2 + dist + major_scale.n[i]),
                                .volume     = NOTE2VOL(C2 + dist + major_scale.n[i]) * 1.0,
                                .attack     =  100 / 1000.f,
                                .decay      =  120 / 1000.f,
                                .sustain    =  600 / 1000.f,
                                .release    =  800 / 1000.f,
                        };
                        c++;
                }
        }

        int j = 0;
        if (b == 0) for (; b < 32; b += 4)
        {
                int r;
                if (b % 16 != 0)
                {
                        r = rand() % 5;
                        if (r <= 1)
                                continue;
                        if (r <= 2 && b % 8 == 4)
                                continue;
                }

                r = rand() % 5;
                if (r <= 1)
                        ;
                else if (r == 2)
                        j += 1;
                else if (r == 3)
                        j += 2;
                else if (r == 4)
                        j = rand() % 10;

                int syncho = rand() % 20;
                if (syncho == 0)
                        syncho = -4;
                else if (syncho == 1)
                        syncho = 4;
                else
                        syncho = 0;
                for (int i = 0; i < 10; i++)
                {
                        int bb = b + syncho;
                        int mm = m;
                        if (bb < 0)
                        {
                                bb += NUM_BEAT;
                                mm--;
                                if (mm < 0) mm = NUM_MEAS-1;
                        }
                        if (chord[ch].n[j]) {
                                music[mm][bb][5] = (struct envelope){
                                        .shape      = SQUARE,
                                        .start_freq = NOTE2FREQ(C3 + dist + major_scale.n[j]),
                                        .volume     = NOTE2VOL(C3 + dist + major_scale.n[j]),
                                        .attack     =   50 / 1000.f,
                                        .decay      =  100 / 1000.f,
                                        .sustain    =  300 / 1000.f,
                                        .release    =  400 / 1000.f,
                                };
                                if (rand()%2 == 0)
                                                music[mm][bb+1][5] = music[mm][bb][5];
                                break;
                        }
                        j = (j + 1) % 10;
                }
        }
        /*
        #define ISN(b, x) \
                music[m][b][5] = (struct envelope){ \
                        .shape      = SQUARE, \
                        .start_freq = NOTE2FREQ(C3 + dist + major_scale.x), \
                        .volume     = NOTE2VOL(C3 + dist + major_scale.x), \
                        .attack     =   50 / 1000.f, \
                        .decay      =  100 / 1000.f, \
                        .sustain    =  300 / 1000.f, \
                        .release    =  400 / 1000.f, \
                };
        ISN(8, _5)
        ISN(16, _3)
        ISN(24, _1)
        #undef ISN
        */
} 

void music_setup()
{
        for (int m = 0; m < NUM_MEAS; m++)
        {
                for (int b = 0; b < NUM_BEAT; b++)
                {
                        if (b == 0)
                                music_beat0_chord(m, b);

                        if (b % 4 == 0)
                                music[m][b][6] = (struct envelope){
                                        .shape      = NOISE,
                                        .start_freq = NOTE2FREQ(A5) * 5,
                                        .volume     = 0.6,
                                        .attack     =   1 / 1000.f,
                                        .decay      =   2 / 1000.f,
                                        .sustain    =  40 / 1000.f,
                                        .release    =  50 / 1000.f,
                                };

                        if (b % 16 == 0)
                                music[m][b][7] = (struct envelope){
                                        .shape      = NOISE,
                                        .start_freq = NOTE2FREQ(A2),
                                        .volume     = 1.2,
                                        .attack     =   1 / 1000.f,
                                        .decay      =  50 / 1000.f,
                                        .sustain    =  50 / 1000.f,
                                        .release    = 600 / 1000.f,
                                };

                        if (b % 16 == 8)
                                music[m][b][7] = (struct envelope){
                                        .shape      = NOISE,
                                        .start_freq = NOTE2FREQ(A5) * 3,
                                        .volume     = 0.6,
                                        .attack     =  10 / 1000.f,
                                        .decay      =  50 / 1000.f,
                                        .sustain    =  60 / 1000.f,
                                        .release    = 600 / 1000.f,
                                };
                }
        }
}

void update_music()
{
        /*
        for (int i = 0; i < NUM_MUS; i++)
        {
                if (music[i].shape == SQUARE)
                        music[i].start_freq *= 1.001;
        }
        */
}

#endif // TET_MUSIC_C_INCLUDED
