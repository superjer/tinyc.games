#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define W 480
#define H 600
#define GROUND 80
#define PIPE_W 86
#define PHYS_W (W + PIPE_W + 80)
#define GAP 220
#define GRACE 4

enum gamestates {READY, ALIVE, GAMEOVER};

int gamestate = READY;
float player_x = 80;
float player_y = (H - GROUND)/2;
float player_w = 60;
float player_h = 60;
float player_vel;

int pipe_x[2] = {W, W};
float pipe_y[2];
int score;
int best;

int idle_time;
float frame = 0;

SDL_Event event;
SDL_Renderer *renderer;
SDL_Texture *pillar;
SDL_Texture *background;
SDL_Texture *bird[4];
TTF_Font *font;

void setup();
void new_game();
void key_down();
void update_stuff();
void update_pipe(int i);
int random_pipe_height();
void draw_stuff();
void text(char *msg, int y);

//the entry point and main game loop
int main()
{
        setup();

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT: exit(0);
                        case SDL_KEYDOWN: key_down(); break;
                        case SDL_MOUSEBUTTONDOWN: key_down(); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(1000 / 60);
                idle_time++;
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);

        SDL_Window *win = SDL_CreateWindow("Flappy",
                        SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED,
                        W,
                        H,
                        SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);

        if(!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer for some reason\n");
                exit(-1);
        }

        SDL_Surface *surf;

        surf = SDL_LoadBMP("pillar.bmp");
        SDL_SetColorKey(surf, 1, 0xffff00);
        pillar = SDL_CreateTextureFromSurface(renderer, surf);

        surf = SDL_LoadBMP("background.bmp");
        background = SDL_CreateTextureFromSurface(renderer, surf);

        for(int i = 0; i < 4; i++)
        {
                char file[80];
                sprintf(file, "bird-%d.bmp", i);
                surf = SDL_LoadBMP(file);
                SDL_SetColorKey(surf, 1, 0xffff00);
                bird[i] = SDL_CreateTextureFromSurface(renderer, surf);
        }

        TTF_Init();
        font = TTF_OpenFont("LiberationMono-Regular.ttf", 50);
        printf("%p %s\n", font, SDL_GetError());
}

//start a new game
void new_game()
{
        gamestate = ALIVE;
        player_x = 80;
        player_y = (H - GROUND)/2;
        player_vel = -11.7f;
        score = 0;
        pipe_x[0] = PHYS_W + PHYS_W/2 - PIPE_W;
        pipe_x[1] = PHYS_W - PIPE_W;
        pipe_y[0] = random_pipe_height();
        pipe_y[1] = random_pipe_height();
}

//when we hit something
void game_over()
{
        gamestate = GAMEOVER;
        idle_time = 0;
        if(best < score) best = score;
}

//handle a key press from the player
void key_down()
{
        if(gamestate == ALIVE)
        {
                player_vel = -11.7f;
                frame += 1.0f;
        }
        else if(idle_time > 30)
        {
                new_game();
        }
}

//update everything that needs to update on its own, without input
void update_stuff()
{
        if(gamestate != ALIVE)
                return;

        player_y += player_vel;
        player_vel += 0.61; // gravity

        if(player_vel > 10.0f)
                frame = 0;
        else
                frame -= (player_vel - 10.0f) * 0.03f;

        // hit ground?
        if(player_y > H - GROUND - player_h)
                game_over();

        // pipes
        for(int i = 0; i < 2; i++)
                update_pipe(i);
}

void update_pipe(int i)
{
        // collide player
        if(player_x + player_w >= pipe_x[i] + GRACE && player_x <= pipe_x[i] + PIPE_W - GRACE &&
                (player_y <= pipe_y[i] - GRACE || player_y + player_h >= pipe_y[i] + GAP + GRACE))
        {
                game_over();
                return;
        }

        // move pipes
        pipe_x[i] -= 5;

        if(pipe_x[i] + PIPE_W < player_x && pipe_x[i] + PIPE_W > player_x - 5)
                score++;

        if(pipe_x[i] <= -PIPE_W)
        {
                pipe_x[i] = PHYS_W - PIPE_W;
                pipe_y[i] = random_pipe_height();
        }
}

int random_pipe_height()
{
        return rand() % (H - GROUND - GAP - 120) + 60;
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect dest = {0, 0, W, H};
        SDL_RenderCopy(renderer, background, NULL, &dest);

        //draw pipes
        for(int i = 0; i < 2; i++)
        {
                int lower = pipe_y[i] + GAP;
                SDL_RenderCopy(renderer, pillar, NULL, &(SDL_Rect){pipe_x[i], pipe_y[i] - H, PIPE_W, H});
                SDL_Rect src = {0, 0, 86, H - lower - GROUND};
                SDL_RenderCopy(renderer, pillar, &src, &(SDL_Rect){pipe_x[i], lower, PIPE_W, src.h});
        }

        //draw player
        SDL_RenderCopy(renderer, bird[(int)frame % 4], NULL, &(SDL_Rect){player_x, player_y, player_w, player_h});

        //draw text
        char msg[80];
        if(gamestate != READY)
        {
                snprintf(msg, 80, "%d", score);
                text(msg, 10);
        }

        if(gamestate != ALIVE)
        {
                text(gamestate == GAMEOVER ? "Game Over" : "Press any key", 150);
                if(gamestate == GAMEOVER)
                {
                        snprintf(msg, 80, "Best: %d", best);
                        text(msg, 250);
                }
        }

        SDL_RenderPresent(renderer);
}

void text(char *msg, int y)
{
        if(!font) return;

        int w, h;
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){255, 255, 255});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {(W - w)/2, y, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}
