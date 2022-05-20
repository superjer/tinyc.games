#include <math.h>
#include <SDL_audio.h>

#define NUM_ENVS 20

static SDL_AudioSpec audiospec;
static SDL_AudioDeviceID audiodevid;

enum WVSHAPE { SINE, SQUARE, TRIANGLE, SAWTOOTH, NOISE, NUM_SHAPES };

typedef struct {
        enum WVSHAPE shape;
        double start_freq;
        double volume;
        double attack, decay, sustain, release, attack_ratio;
        double pos;
	double noiseval;
	int    noisectr;
	int    noisesign;
} ENVELOPE;

ENVELOPE envs[NUM_ENVS];

enum notes {
                                                A0, As0, B0,
        C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1,
        C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2,
        C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3,
        C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
        C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5,
        C6, Cs6, D6, Ds6, E6, F6, Fs6, G6, Gs6, A6, As6, B6,
        C7, Cs7, D7, Ds7, E7, F7, Fs7, G7, Gs7, A7, As7, B7,
        C8,
};

struct {
        char name[3];
        int octave;
        double frequency;
        double wavelength;
} music_notes[] = {
        {"A" , 0, 27.5     , 12.374}, // Piano low
        {"A#", 0, 29.135   , 11.680},
        {"B" , 0, 30.868   , 11.024},
        {"C" , 1, 32.703   , 10.405},
        {"C#", 1, 34.648   ,  9.821},
        {"D" , 1, 36.708   ,  9.270},
        {"D#", 1, 38.891   ,  8.750},
        {"E" , 1, 41.203   ,  8.259},
        {"F" , 1, 43.654   ,  7.795},
        {"F#", 1, 46.249   ,  7.358},
        {"G" , 1, 48.999   ,  6.945},
        {"G#", 1, 51.913   ,  6.555},
        {"A" , 1, 55       ,  6.187},
        {"A#", 1, 58.27    ,  5.840},
        {"B" , 1, 61.735   ,  5.512},
        {"C" , 2, 65.406   ,  5.203},
        {"C#", 2, 69.296   ,  4.911},
        {"D" , 2, 73.416   ,  4.635},
        {"D#", 2, 77.782   ,  4.375},
        {"E" , 2, 82.407   ,  4.129},
        {"F" , 2, 87.307   ,  3.898},
        {"F#", 2, 92.499   ,  3.679},
        {"G" , 2, 97.999   ,  3.472},
        {"G#", 2, 103.826  ,  3.278},
        {"A" , 2, 110      ,  3.094},
        {"A#", 2, 116.541  ,  2.920},
        {"B" , 2, 123.471  ,  2.756},
        {"C" , 3, 130.813  ,  2.601},
        {"C#", 3, 138.591  ,  2.455},
        {"D" , 3, 146.832  ,  2.318},
        {"D#", 3, 155.563  ,  2.187},
        {"E" , 3, 164.814  ,  2.065},
        {"F" , 3, 174.614  ,  1.949},
        {"F#", 3, 184.997  ,  1.839},
        {"G" , 3, 195.998  ,  1.736},
        {"G#", 3, 207.652  ,  1.639},
        {"A" , 3, 220      ,  1.547},
        {"A#", 3, 233.082  ,  1.460},
        {"B" , 3, 246.942  ,  1.378},
        {"C" , 4, 261.626  ,  1.301}, // middle C
        {"C#", 4, 277.183  ,  1.228},
        {"D" , 4, 293.665  ,  1.159},
        {"D#", 4, 311.127  ,  1.094},
        {"E" , 4, 329.628  ,  1.032},
        {"F" , 4, 349.228  ,  0.974},
        {"F#", 4, 369.994  ,  0.920},
        {"G" , 4, 391.995  ,  0.868},
        {"G#", 4, 415.305  ,  0.819},
        {"A" , 4, 440      ,  0.773}, // A 440
        {"A#", 4, 466.164  ,  0.730},
        {"B" , 4, 493.883  ,  0.689},
        {"C" , 5, 523.251  ,  0.650},
        {"C#", 5, 554.365  ,  0.614},
        {"D" , 5, 587.33   ,  0.579},
        {"D#", 5, 622.254  ,  0.547},
        {"E" , 5, 659.255  ,  0.516},
        {"F" , 5, 698.456  ,  0.487},
        {"F#", 5, 739.989  ,  0.460},
        {"G" , 5, 783.991  ,  0.434},
        {"G#", 5, 830.609  ,  0.410},
        {"A" , 5, 880      ,  0.387},
        {"A#", 5, 932.328  ,  0.365},
        {"B" , 5, 987.767  ,  0.345},
        {"C" , 6, 1046.502 ,  0.325},
        {"C#", 6, 1108.731 ,  0.307},
        {"D" , 6, 1174.659 ,  0.290},
        {"D#", 6, 1244.508 ,  0.273},
        {"E" , 6, 1318.51  ,  0.258},
        {"F" , 6, 1396.913 ,  0.244},
        {"F#", 6, 1479.978 ,  0.230},
        {"G" , 6, 1567.982 ,  0.217},
        {"G#", 6, 1661.219 ,  0.205},
        {"A" , 6, 1760     ,  0.193},
        {"A#", 6, 1864.655 ,  0.182},
        {"B" , 6, 1975.533 ,  0.172},
        {"C" , 7, 2093.005 ,  0.163},
        {"C#", 7, 2217.461 ,  0.153},
        {"D" , 7, 2349.318 ,  0.145},
        {"D#", 7, 2489.016 ,  0.137},
        {"E" , 7, 2637.021 ,  0.129},
        {"F" , 7, 2793.826 ,  0.122},
        {"F#", 7, 2959.955 ,  0.115},
        {"G" , 7, 3135.964 ,  0.109},
        {"G#", 7, 3322.438 ,  0.102},
        {"A" , 7, 3520     ,  0.097},
        {"A#", 7, 3729.31  ,  0.091},
        {"B" , 7, 3951.066 ,  0.086},
        {"C" , 8, 4186.009 ,  0.081}, // Piano high
};
#define NUM_MUSIC_NOTES (sizeof music_notes / sizeof *music_notes)

static void silly_noise(int shape, int note_lo, int note_hi,
                double attack, double decay, double sustain, double release)
{
        ENVELOPE *e = envs + rand()%NUM_ENVS;
        memset(e, 0, sizeof *e);

        int note = note_lo + rand() % (note_hi - note_lo + 1);

        e->shape      = shape;
        e->start_freq = music_notes[note].frequency;
        e->volume     = 1.0 + 0.08 * music_notes[note].wavelength;
        if (shape == NOISE)
	{
		e->noiseval = 0.0;
		e->noisesign = 1;
		e->noisectr = 0;
		e->volume *= 0.2;
	}

        e->attack_ratio = 1.5;
        e->attack  = attack  / 1000.f;
        e->decay   = decay   / 1000.f + e->attack;
        e->sustain = sustain / 1000.f + e->decay;
        e->release = release / 1000.f + e->sustain;
}

void mixaudio(void *unused, unsigned char *stream, int len)
{
        int j;
        int buf[len];
        short *out = (short*)stream;

        memset(buf, 0, sizeof *buf * len); // twice as big as it needs to be, if 16bit

        for( int j=0; j<len/2; j++ ) for( int n=0; n<NUM_ENVS; n++ )
	{
		ENVELOPE *e = envs + n;

		if( e->pos >= e->release )
			continue;

		double freq = e->start_freq;
		double wl   = 1.0 / freq;
		double wl2  = wl / 2.0;
		double frac = fmod(e->pos, wl);

		if (e->shape == NOISE)
		{
			if (e->noisectr-- <= 0)
			{
				double r = (double)rand() / (double)RAND_MAX;
				e->noisectr  = wl * r * 100000 + 1;
				e->noisesign = e->noisesign < 0 ? 1 : -1;
				e->noiseval  = 1.0;
			}
			e->noiseval *= 0.995;
		}

		double t = e->pos;
		e->pos += 1.0 / 44100;

		double veloc = e->volume * 2200;
		if      (t <= e->attack ) veloc *= t / e->attack * e->attack_ratio;
		else if (t <= e->decay  ) veloc *= (e->decay - t) / (e->decay - e->attack) * (e->attack_ratio - 1.0) + 1.0;
		else if (t <= e->sustain) ;
		else if (t <= e->release) veloc *= (e->release - t) / (e->release - e->sustain);

		switch (e->shape)
		{
			case SINE:     buf[j] += veloc * sin(frac*freq*6.28318531);                  break;
			case SQUARE:   buf[j] += frac > wl2 ? veloc : -veloc;                        break;
			case TRIANGLE: buf[j] += (4*freq*(frac>wl2 ? (wl-frac) : frac) - 1) * veloc; break;
			case SAWTOOTH: buf[j] += (2*freq*frac - 1) * veloc;                          break;
			case NOISE:    buf[j] += (e->noisesign * e->noiseval) * veloc;               break;
			default: break;
		}
	}

        for( j=0; j<len/2; j++ )
        {
                if(      buf[j] >  32767 ) out[j] = (short) 32767;
                else if( buf[j] < -32768 ) out[j] = (short)-32768;
                else                       out[j] = (short)buf[j];
        }
}

void audioinit()
{
        size_t i;
        SDL_AudioSpec desired;
        desired.freq = 44100;
        desired.format = AUDIO_S16LSB;
        desired.channels = 1;
        desired.samples = 512;
        desired.callback = mixaudio;
        desired.userdata = NULL;
        audiodevid = SDL_OpenAudioDevice(NULL, 0, &desired, &audiospec, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (!audiodevid)
                fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        else
                SDL_PauseAudioDevice(audiodevid, 0);
}
