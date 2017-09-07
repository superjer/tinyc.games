// Valet -- http://tinyc.games -- (c) 2016 Jer Wilson

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define W 480
#define H 600
#define NCARS 20
#define PI2 (M_PI/2)

enum gamestates {READY, ALIVE, GAMEOVER} gamestate = READY;

float speed[NCARS], lateral[NCARS], angle[NCARS];
float car_x[NCARS], car_y[NCARS];
int brake, turn_left, turn_right;
int cur;

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *background;
SDL_Texture *car[4];
TTF_Font *font;

void setup();
void new_game();
void new_car();
void update_stuff();
void draw_stuff();
void text(char *fstr, int value, int height);

//the entry point and main game loop
int main()
{
        setup();
        int press;

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT:
                                exit(0);
                        case SDL_KEYDOWN:
                                if(event.key.keysym.sym == SDLK_ESCAPE) exit(0);

                                if(gamestate == READY)
                                {
                                        new_game();
                                        break;
                                }

                                //fall thru
                        case SDL_KEYUP:
                                press = (event.type == SDL_KEYDOWN ? 1 : 0);

                                switch(event.key.keysym.sym)
                                {
                                        case SDLK_SPACE:
                                                brake = press;
                                                break;
                                        case SDLK_LEFT:
                                                turn_left = press;
                                                break;
                                        case SDLK_RIGHT:
                                                turn_right = press;
                                                break;
                                }
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(1000 / 60);
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *win = SDL_CreateWindow("Valet",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
        if(!renderer) renderer = SDL_CreateRenderer(win, -1, 0);
        if(!renderer) exit(fprintf(stderr, "Could not create SDL renderer\n"));

        surf = SDL_LoadBMP("res/background.bmp");
        background = SDL_CreateTextureFromSurface(renderer, surf);

        for(int i = 0; i < 4; i++)
        {
                char file[80];
                sprintf(file, "res/car-%d.bmp", i);
                surf = SDL_LoadBMP(file);
                SDL_SetColorKey(surf, 1, 0xffff00);
                car[i] = SDL_CreateTextureFromSurface(renderer, surf);
        }

        TTF_Init();
        font = TTF_OpenFont("res/LiberationMono-Regular.ttf", 42);
}

//start a new game
void new_game()
{
        int i;
        for(i = 0; i < NCARS; i++) car_x[i] = W*2;
        gamestate = ALIVE;
        cur = 0;
        new_car();
}

void new_car()
{
        speed[cur] = 60.0f + rand() % 40;
        lateral[cur] = 0.0f;
        angle[cur] = M_PI;
        car_x[cur] = 4*W/5;
        car_y[cur] = H;
}

//when we hit something
void game_over()
{
        gamestate = GAMEOVER;
}

//update everything that needs to update on its own, without input
void update_stuff()
{
        if(gamestate != ALIVE) return;

        // movements
        for(int i = 0; i < NCARS; i++)
        {
                if(cur == i && speed[i] > 0.1f)
                        angle[i] += ((turn_left - turn_right) * speed[i])/(M_PI * 150.0f);

                car_x[i] += sinf(angle[i]) * speed[i] * 0.1f;
                car_y[i] += cosf(angle[i]) * speed[i] * 0.1f;

                car_x[i] += sinf(angle[i] + PI2) * lateral[i] * 0.1f;
                car_y[i] += cosf(angle[i] + PI2) * lateral[i] * 0.1f;

                // slow down or brake
                float delta = (brake || cur != i) ? 0.5f : 0.1f;
                float latdelta = fabsf(lateral[i]) * 0.01f;
                delta += latdelta;
                if(     speed[i] >=  delta) speed[i] -= delta;
                else if(speed[i] <= -delta) speed[i] += delta;
                else                        speed[i]  = 0.0f;

                // always brake laterally
                float latang = speed[i] > 0 ? latdelta : -latdelta;
                if(     lateral[i] >=  3.0f) { lateral[i] -= 3.0f; speed[i] += 2.0f; angle[i] += latang * 0.1f; }
                else if(lateral[i] <= -3.0f) { lateral[i] += 3.0f; speed[i] -= 2.0f; angle[i] -= latang * 0.1f; }
                else                         { lateral[i]  = 0.0f; }
        }

        // collisions
        for(int i = 0; i < NCARS; i++) for(int j = i+1; j < NCARS; j++)
        {
                if(fabsf(car_x[i] - car_x[j]) < 20.0f &&
                   fabsf(car_y[i] - car_y[j]) < 20.0f)
                {
                        float si = speed[i];
                        float sj = speed[j];
                        float li = lateral[i];
                        float lj = lateral[j];

                        speed[i]   *= 0.05f;
                        speed[j]   *= 0.05f;
                        lateral[i] *= 0.05f;
                        lateral[j] *= 0.05f;

                        float ai = angle[i];
                        float aj = angle[j];
                        float ix = sinf(ai) * si + sinf(ai + PI2) * li;
                        float iy = cosf(ai) * si + cosf(ai + PI2) * li;
                        float jx = sinf(aj) * sj + sinf(aj + PI2) * lj;
                        float jy = cosf(aj) * sj + cosf(aj + PI2) * lj;

                        speed[i]   += (sinf(ai      )*jx + cosf(ai      )*jy) * 0.95f;
                        speed[j]   += (sinf(aj      )*ix + cosf(aj      )*iy) * 0.95f;
                        lateral[i] += (sinf(ai + PI2)*jx + cosf(ai + PI2)*jy) * 0.95f;
                        lateral[j] += (sinf(aj + PI2)*ix + cosf(aj + PI2)*iy) * 0.95f;
                }
        }

        // has our car stopped or gone oob?
        if(speed[cur] == 0.0f || car_x[cur] < 0 || car_x[cur] > W || car_y[cur] < 0 || car_y[cur] > H)
        {
                if(++cur < NCARS) new_car(); else new_game();
        }
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect dest = {0, 0, W, H};
        SDL_RenderCopy(renderer, background, NULL, &dest);

        //draw cars
        int i;
        for(i = 0; i < NCARS; i++)
        {
                SDL_RenderCopyEx(renderer, car[0], NULL,
                        &(SDL_Rect){car_x[i], car_y[i], 40, 40},
                        -angle[i]*180/M_PI, NULL, 0);
        }

        if(gamestate == READY) text("Press any key", 0, 150);

        SDL_RenderPresent(renderer);
}

void text(char *fstr, int value, int height)
{
        if(!font) return;
        int w, h;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){255, 255, 255});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {(W - w)/2, height, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}
