#include "tet.h"

// render a line of text optionally with a %d value in it
void text(char *fstr, int value)
{
        if (!font || !fstr || !fstr[0]) return;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        SDL_Color color = (SDL_Color){180, 190, 185};
        if (fstr[0] == ' ' && tick / 3 % 2) color = (SDL_Color){255, 255, 255};
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, color);
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {text_x, text_y, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
        text_y += bs * 125 / 100 + (fstr[strlen(fstr) - 1] == ' ' ? bs : 0);
}

void draw_menu()
{
        if (state != MAIN_MENU && state != NUMBER_MENU) return;

        menu_pos = MAX(menu_pos, 0);
        menu_pos = MIN(menu_pos, state == NUMBER_MENU ? 3 : 2);
        p = play; // just grab first player :)

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){
                        p->held.x,
                        p->held.y + p->box_w + bs2 + line_height * (menu_pos + 1),
                        p->board_w,
                        line_height });
        text_x = p->held.x;
        text_y = p->held.y + p->box_w + bs2;
        if (state == MAIN_MENU)
        {
                text("Main Menu"        , 0);
                text("Endless"          , 0);
                text("Garbage Race"     , 0);
                text("Quit"             , 0);
        }
        else if (state == NUMBER_MENU)
        {
                text("How many players?", 0);
                text("1"                , 0);
                text("2"                , 0);
                text("3"                , 0);
                text("4"                , 0);
        }
}

// set the current draw color to the color assoc. with a shape
void set_color_from_shape(int shape, int shade)
{
        int r = MAX((colors[shape] >> 16 & 0xFF) + shade, 0);
        int g = MAX((colors[shape] >>  8 & 0xFF) + shade, 0);
        int b = MAX((colors[shape] >>  0 & 0xFF) + shade, 0);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

// draw a single mino (square) of a shape
void draw_mino(int x, int y, int shape, int outline, int part)
{
        if (!part) return;
        int bw = MAX(1, outline ? bs / 10 : bs / 6);
        set_color_from_shape(shape, -40);
        SDL_RenderFillRect(renderer, &(SDL_Rect){x, y, bs, bs});
        set_color_from_shape(shape, outline ? -255 : 0);
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // horizontal band
                        x + (part & 8 ? 0 : bw),
                        y + bw,
                        bs - (part & 8 ? 0 : bw) - (part & 2 ? 0 : bw),
                        bs - bw - bw });
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // vertical band
                        x + (part & 32 ? 0 : bw),
                        y + (part & 1 ? 0 : bw),
                        bs - (part & 32 ? 0 : bw) - (part & 16 ? 0 : bw),
                        bs - (part & 1 ? 0 : bw) - (part & 4 ? 0 : bw) });
}

#define CENTER 1
#define OUTLINE 2

void draw_shape(int x, int y, int color, int rot, int flags)
{
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_mino(
                        x + bs * i + ((flags & CENTER) ? bs2 + bs2 * center[2 * color    ] : 0),
                        y + bs * j + ((flags & CENTER) ? bs  + bs2 * center[2 * color + 1] : 0),
                        color,
                        flags & OUTLINE,
                        is_solid_part(color, rot, i, j)
                );
}

// draw everything in the game on the screen for current player
void draw_player()
{
        if (state != PLAY && state != ASSIGN) return;

        // draw background, black boxes
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){p->held.x, p->held.y, p->box_w, p->box_w});
        SDL_RenderFillRect(renderer, &(SDL_Rect){p->board_x, p->board_y, p->board_w, bs * VHEIGHT});

        // find ghost piece position
        int ghost_y = p->it.y;
        while (ghost_y < BHEIGHT && !collide(p->it.x, ghost_y + 1, p->it.rot))
                ghost_y++;

        // draw shadow
        if (p->it.color)
        {
                struct shadow shadow = shadows[p->it.rot][p->it.color];
                int top = MAX(0, p->it.y + shadow.y - 5);
                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        p->board_x + bs * (p->it.x + shadow.x),
                        p->board_y + bs * top,
                        bs * shadow.w,
                        MAX(0, bs * (ghost_y - top + shadow.y - 5)) });
        }

        // draw hard drop beam
        float loss = .1f * (tick - p->beam_tick);
        if (loss < 1.f && p->beam.color)
        {
                struct shadow shadow = shadows[p->beam.rot][p->beam.color];
                int rw = bs * shadow.w;
                int rh = bs * (p->beam.y + shadow.y - 5);
                int lossw = (1.f - ((1.f - loss) * (1.f - loss))) * rw;
                int lossh = loss < .5f ? 0.f : (1.f - ((1.f - loss) * (1.f - loss))) * rh;
                SDL_SetRenderDrawColor(renderer, 33, 37, 43, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){ (p->board_x + bs * (p->beam.x + shadow.x)) + lossw / 2,
                                p->board_y + lossh, rw - lossw, rh - lossh});
        }

        // draw pieces on board
        for (int i = 0; i < BWIDTH; i++) for (int j = 0; j < BHEIGHT; j++)
                draw_mino(p->board_x + bs * i, p->board_y + bs * (j-5) - p->line_offset[j],
                                p->board[j][i].color, 0, p->board[j][i].part);

        // draw falling piece & ghost
        draw_shape(p->board_x + bs * p->it.x, p->board_y + bs * (ghost_y - 5), p->it.color, p->it.rot, OUTLINE);
        draw_shape(p->board_x + bs * p->it.x, p->board_y + bs * (p->it.y - 5), p->it.color, p->it.rot, 0);

        // draw next pieces
        for (int n = 0; n < 5; n++)
                draw_shape(p->preview_x, p->preview_y + 3 * bs * n, p->next[n], 0, CENTER);

        // draw held piece
        draw_shape(p->held.x, p->held.y, p->held.color, 0, CENTER);

        // draw scores etc
        text_x = p->held.x;
        text_y = p->held.y + p->box_w + bs2;
        text("%d pts "   , p->score    );
        text("%d lines " , p->lines    );
        text(p->dev_name , 0           );

        if (p->reward)
        {
                text_x = p->reward_x - bs;
                text_y = p->reward_y--;
                text("%d", p->reward);
        }

        text_x = p->board_x + bs2;
        text_y = p->board_y + bs2 * 19;
        if (p->countdown_time > 0)
                text(countdown_msg[p->countdown_time / CTDN_TICKS], 0);

        if (state == ASSIGN)
                text(p >= play + assign_me ? "Press button to join" : p->dev_name, 0 );
}

// recalculate sizes and positions on resize
void resize(int x, int y)
{
        win_x = x;
        win_y = y;
        bs = MIN(win_x / (nplay * 22), win_y / 24);
        bs2 = bs / 2;
        line_height = bs * 125 / 100;
        int n = 0;
        for (p = play; p < play + nplay; p++, n++)
        {
                p->board_x = (x / (nplay * 2)) * (2 * n + 1) - bs2 * BWIDTH;
                p->board_y = (y / 2) - bs2 * VHEIGHT;
                p->board_w = bs * 10;
                p->box_w = bs * 5;
                p->held.x = p->board_x - p->box_w - bs2;
                p->held.y = p->board_y;
                p->preview_x = p->board_x + p->board_w + bs2;
                p->preview_y = p->board_y;
        }
        if (font) TTF_CloseFont(font);
        font = TTF_OpenFont("../common/res/LiberationSans-Regular.ttf", bs);
}
