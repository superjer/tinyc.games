#pragma once

#include "tet.h"
#include "../common/tinyc.games/audio.h"

int roll[NUM_MUS] = {
C2, 0, 0, 0, 0, 0, 0, 0,
C2, 0, 0, 0, 0, 0, 0, 0,
C2, 0, 0, 0, 0, 0, 0, 0,
F2, 0, 0, 0, 0, 0, 0, 0,
D2, 0, 0, 0, 0, 0, 0, 0,
D2, 0, 0, 0, 0, 0, 0, 0,
D2, 0, 0, 0, 0, 0, 0, 0,
A2, 0, 0, 0, 0, 0, 0, 0,
F2, 0, 0, 0, 0, 0, 0, 0,
F2, 0, 0, 0, 0, 0, 0, 0,
F2, 0, 0, 0, 0, 0, 0, 0,
A2, 0, 0, 0, 0, 0, 0, 0,
A1, 0, 0, 0, 0, 0, 0, 0,
A1, 0, 0, 0, 0, 0, 0, 0,
A1, 0, 0, 0, 0, 0, 0, 0,
B1, 0, 0, 0, 0, 0, 0, 0,
};

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
        int bits;
               //                     1   1
} chord[] = {  //    1   3  5   7v 9  1   3
        { "None",  0b0000000000000000000000 },
        { "",      0b1000100100000000000000 },
        { "11",    0b0000000000100010010000 },
        { "13",    0b0000000000101000100001 },
        { "13b9",  0b0000000000100100100001 },
        { "13x",   0b0000000000100010010001 },
        { "6",     0b1000100101000000000000 },
        { "7",     0b1000100100100000000000 },
        { "7b9",   0b0000100100100100000000 },
        { "7sus",  0b1000010100100000000000 },
        { "9",     0b0000100100100010000000 },
        { "9b5",   0b0000101000100010000000 },
        { "9s5",   0b0000100010100010000000 },
        { "M7",    0b1000100100010000000000 },
        { "M9",    0b0000100100010010000000 },
        { "add2",  0b1010100100000000000000 },
        { "aug",   0b1000100010000000000000 },
        { "dim",   0b1001001000000000000000 },
        { "dim7",  0b1001001001000000000000 },
        { "m",     0b1001000100000000000000 },
        { "m6",    0b1001000101000000000000 },
        { "m7",    0b1001000100100000000000 },
        { "m7b5",  0b1001001000100000000000 },
        { "m9",    0b0001000100100010000000 },
        { "sus",   0b1000010100000000000000 },
        { "/3",    0b0000100100001000000000 },
        { "/5",    0b0000000100001000100000 },
        { "dim/b3",0b0001001000001000000000 },
        { "4/1",   0b1000010001001000000000 },
        { "5/1",   0b1010000100010000000000 },
        { "5/2",   0b0010000100010010000000 },
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

void play_chord()
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

        int ch = chordnode[n].flavor[flav];
        int dist = chordnode[n].distance;

        fprintf(stderr, "Playing a %s%s chord...\n",
                notenames[dist], chord[ch].name);

        int low_disti = 0;
        for (int i = 0; i < 22; i++)
        {
                if (chord[ch].bits & (1 << (22-i)))
                {
                        if (!low_disti)
                                low_disti = dist + i;
                        int note = C2 + dist + i - (low_disti > 8 ? 12 : 0);
                        audio_tone(SQUARE, note, note, 10, 50, 150, 400);
                }
        }
}

void music_setup()
{
        /*
        for (int i = 0; i < NUM_MUS / 2; i++)
        {
                if (roll[i])
                        music[i*2] = (struct envelope){
                                .shape      = SQUARE,
                                .start_freq = NOTE2FREQ(roll[i]),
                                .volume     = NOTE2VOL(roll[i]),
                                .attack     =  40 / 1000.f,
                                .decay      =  80 / 1000.f,
                                .sustain    = 200 / 1000.f,
                                .release    = 500 / 1000.f,
                        };

                if (i % 16 == 0)
                        music[i*2+1] = (struct envelope){
                                .shape      = NOISE,
                                .start_freq = NOTE2FREQ(A1),
                                .volume     = 0.6,
                                .attack     =   1 / 1000.f,
                                .decay      =  50 / 1000.f,
                                .sustain    =  50 / 1000.f,
                                .release    = 600 / 1000.f,
                        };

                if (i % 16 == 8)
                        music[i*2+1] = (struct envelope){
                                .shape      = NOISE,
                                .start_freq = NOTE2FREQ(A5) * 2,
                                .volume     = 0.6,
                                .attack     =  10 / 1000.f,
                                .decay      =  50 / 1000.f,
                                .sustain    =  60 / 1000.f,
                                .release    = 600 / 1000.f,
                        };
        }
        */
}

void update_music()
{
        for (int i = 0; i < NUM_MUS; i++)
        {
                if (music[i].shape == SQUARE)
                        music[i].start_freq *= 1.001;
        }
}
