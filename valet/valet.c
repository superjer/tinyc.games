// Valet -- http://tinyc.games -- (c) 2016 Jer Wilson

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define W 480
#define H 600
#define NCARS 20

enum gamestates {READY, ALIVE, GAMEOVER} gamestate = READY;

float speed;
float angle[NCARS];
float car_x[NCARS], car_y[NCARS];
int brake, turn_left, turn_right;
int curcar;

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
        curcar = 0;
        new_car();
}

void new_car()
{
        speed = 60.0f + rand() % 40;
        angle[curcar] = M_PI;
        car_x[curcar] = 4*W/5;
        car_y[curcar] = H;
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

        if(speed > 0.1f)
                angle[curcar] += ((turn_left - turn_right) * speed)/(M_PI * 150.0f);

        car_x[curcar] += sinf(angle[curcar]) * speed * 0.1f;
        car_y[curcar] += cosf(angle[curcar]) * speed * 0.1f;

        float speed_diff = brake ? 0.5f : 0.1f;
        if(speed >= speed_diff)
                speed -= speed_diff;
        else
                speed = 0.0f;

        if(speed == 0.0f || car_x[curcar] < 0 || car_x[curcar] > W || car_y[curcar] < 0 || car_y[curcar] > H)
        {
                curcar++;
                if(curcar < NCARS)
                        new_car();
                else
                        new_game();
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
