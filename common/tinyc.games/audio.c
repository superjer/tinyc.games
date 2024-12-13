// tinyc.games audio - copyright 2023 Jer Wilson
//
// 1. Add SDL_INIT_AUDIO to your SDL_Init()
// 2. Call audioinit() before playing sound
// 3. Play tones with audio_tone()

#include <math.h>
#include <SDL3/SDL_audio.h>
#include "audio.h"

void audio_tone(int shape, int note_lo, int note_hi,
                double attack, double decay, double sustain, double release)
{
        int note = note_lo + rand() % (note_hi - note_lo + 1);
        if (note < A0 || note > C6) return;

        envs[rand() % NUM_ENVS] = (struct envelope){
                .shape      = shape,
                .start_freq = music_notes[note].frequency,
                .volume     = 1.0 + 0.08 * music_notes[note].wavelength,
                .attack     = (attack                            ) / 1000.f,
                .decay      = (attack + decay                    ) / 1000.f,
                .sustain    = (attack + decay + sustain          ) / 1000.f,
                .release    = (attack + decay + sustain + release) / 1000.f,
        };
}

static void SDLCALL mix_audio(void *unused, unsigned char *stream, int len)
{
        short *out = (short*)stream;

        for (int j = 0; j < len/2; j++)
        {
                int samp = 0;

                for (int n = 0; n < NUM_ENVS; n++)
                {
                        struct envelope *e = envs + n;
                        if (e->pos >= e->release)
                                continue;

                        double freq = e->start_freq;
                        double wl   = 1.0 / freq;
                        double wl2  = wl / 2.0;
                        double frac = fmod(e->pos, wl);
                        double t = e->pos;
                        e->pos += 1.0 / 44100;

                        double veloc = e->volume * 2200;
                        if      (t <= e->attack ) veloc *= t / e->attack * 1.5;
                        else if (t <= e->decay  ) veloc *= (e->decay - t) / (e->decay - e->attack) * .5 + 1.0;
                        else if (t <= e->sustain) ;
                        else if (t <= e->release) veloc *= (e->release - t) / (e->release - e->sustain);

                        switch (e->shape)
                        {
                                case SINE:     samp += veloc * sin(frac * freq * 6.28318531);                      break;
                                case SQUARE:   samp += frac > wl2 ? veloc : -veloc;                                break;
                                case TRIANGLE: samp += (4 * freq * (frac > wl2 ? (wl - frac) : frac) - 1) * veloc; break;
                        }
                }

                out[j] = samp >  32767 ?  32768 :
                         samp < -32767 ? -32767 :
                         samp;
        }
}

void SDLCALL mix_audio_wrapper(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
        if (additional_amount > 0) {
                Uint8 *data = SDL_stack_alloc(Uint8, additional_amount);
                if (data) {
                        mix_audio(userdata, data, additional_amount);
                        SDL_PutAudioStreamData(stream, data, additional_amount);
                        SDL_stack_free(data);
                }
        }
}

void audio_init()
{
        const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
        SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mix_audio_wrapper, NULL);
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
}
