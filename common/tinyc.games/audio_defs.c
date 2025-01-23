#pragma once

#define NUM_ENVS 20
#define NUM_BEAT 32
#define NUM_MEAS 4
#define NUM_CHAN 8

#define NOTE2FREQ(note) (music_notes[note].frequency)
#define NOTE2VOL(note) (1.0 + 0.08 * music_notes[note].wavelength)

enum shape {NOSHAPE, SINE, SQUARE, TRIANGLE, NOISE};

struct envelope {
        enum shape shape;
        double start_freq;
        double volume;
        double attack, decay, sustain, release; // envelope timings
        double pos;                             // current play position
	double noiseval;
	int    noisectr;
	int    noisesign;
} envs[NUM_ENVS], music[NUM_MEAS][NUM_BEAT][NUM_CHAN];

enum notes {
        C0, Cs0, D0, Ds0, E0, F0, Fs0, G0, Gs0, A0, As0, B0,
        C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1,
        C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2,
        C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3,
        C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
        C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5,
};

struct {
        char name[3];
        int octave;
        double frequency;
        double wavelength;
} music_notes[] = {
        {"C",  0,   16.35 , 20.810},
        {"C#", 0,   17.32 , 19.642},
        {"D",  0,   18.35 , 18.540},
        {"D#", 0,   19.45 , 17.500},
        {"E",  0,   20.60 , 16.518},
        {"F",  0,   21.83 , 15.590},
        {"F#", 0,   23.12 , 14.716},
        {"G",  0,   24.50 , 13.890},
        {"G#", 0,   25.96 , 13.110},
        {"A" , 0,   27.500, 12.374}, // Piano low
        {"A#", 0,   29.135, 11.680},
        {"B" , 0,   30.868, 11.024},
        {"C" , 1,   32.703, 10.405},
        {"C#", 1,   34.648,  9.821},
        {"D" , 1,   36.708,  9.270},
        {"D#", 1,   38.891,  8.750},
        {"E" , 1,   41.203,  8.259},
        {"F" , 1,   43.654,  7.795},
        {"F#", 1,   46.249,  7.358},
        {"G" , 1,   48.999,  6.945},
        {"G#", 1,   51.913,  6.555},
        {"A" , 1,   55.000,  6.187},
        {"A#", 1,   58.270,  5.840},
        {"B" , 1,   61.735,  5.512},
        {"C" , 2,   65.406,  5.203},
        {"C#", 2,   69.296,  4.911},
        {"D" , 2,   73.416,  4.635},
        {"D#", 2,   77.782,  4.375},
        {"E" , 2,   82.407,  4.129},
        {"F" , 2,   87.307,  3.898},
        {"F#", 2,   92.499,  3.679},
        {"G" , 2,   97.999,  3.472},
        {"G#", 2,  103.826,  3.278},
        {"A" , 2,  110.000,  3.094},
        {"A#", 2,  116.541,  2.920},
        {"B" , 2,  123.471,  2.756},
        {"C" , 3,  130.813,  2.601},
        {"C#", 3,  138.591,  2.455},
        {"D" , 3,  146.832,  2.318},
        {"D#", 3,  155.563,  2.187},
        {"E" , 3,  164.814,  2.065},
        {"F" , 3,  174.614,  1.949},
        {"F#", 3,  184.997,  1.839},
        {"G" , 3,  195.998,  1.736},
        {"G#", 3,  207.652,  1.639},
        {"A" , 3,  220.000,  1.547},
        {"A#", 3,  233.082,  1.460},
        {"B" , 3,  246.942,  1.378},
        {"C" , 4,  261.626,  1.301}, // middle C
        {"C#", 4,  277.183,  1.228},
        {"D" , 4,  293.665,  1.159},
        {"D#", 4,  311.127,  1.094},
        {"E" , 4,  329.628,  1.032},
        {"F" , 4,  349.228,  0.974},
        {"F#", 4,  369.994,  0.920},
        {"G" , 4,  391.995,  0.868},
        {"G#", 4,  415.305,  0.819},
        {"A" , 4,  440.000,  0.773}, // A 440
        {"A#", 4,  466.164,  0.730},
        {"B" , 4,  493.883,  0.689},
        {"C" , 5,  523.251,  0.650},
        {"C#", 5,  554.365,  0.614},
        {"D" , 5,  587.330,  0.579},
        {"D#", 5,  622.254,  0.547},
        {"E" , 5,  659.255,  0.516},
        {"F" , 5,  698.456,  0.487},
        {"F#", 5,  739.989,  0.460},
        {"G" , 5,  783.991,  0.434},
        {"G#", 5,  830.609,  0.410},
        {"A" , 5,  880.000,  0.387},
        {"A#", 5,  932.328,  0.365},
        {"B" , 5,  987.767,  0.345},
};

int next_env = 0;
bool music_on = true;

void audio_init();
void audio_tone(int shape, int note_lo, int note_hi,
                double attack, double decay, double sustain, double release);
int music_toggle();
