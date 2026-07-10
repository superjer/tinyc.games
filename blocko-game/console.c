#include "blocko.c"
#ifndef BLOCKO_CONSOLE_C_INCLUDED
#define BLOCKO_CONSOLE_C_INCLUDED

// console.c - in-game chat, Minecraft-style: T opens the input line, Enter
// sends, Esc cancels. Incoming lines (yours and the network's) show at the
// bottom of the screen for a few seconds, then fade.

int console_open;
int console_opened_frame; // swallow the opening keystroke's own text event
char console_input[256];
size_t console_input_len;

// recent chat, shown even with the input closed, newest at the bottom
#define CHAT_LINES 8
#define CHAT_SHOW_FRAMES 600 // ~10 seconds
char chat_log[CHAT_LINES][300];
int chat_expire[CHAT_LINES];
int chat_head;

// append one line to the chat log (printable ascii only - this also takes
// lines straight off the network, so strip anything that could fake a line)
void chat_add(const char *s)
{
        char *d = chat_log[chat_head], *end = d + sizeof chat_log[0] - 1;
        for (; *s && d < end; s++)
                if (*s >= ' ' && *s <= '~')
                        *d++ = *s;
        *d = '\0';
        if (headless) // the terminal is the chat display
        {
                printf("%s\n", chat_log[chat_head]);
                fflush(stdout); // don't lag when piped to a log
        }
        chat_expire[chat_head] = frame + CHAT_SHOW_FRAMES;
        chat_head = (chat_head + 1) % CHAT_LINES;
}

static void console_show()
{
        console_open = 1;
        console_opened_frame = frame;
        console_input[0] = '\0';
        console_input_len = 0;
        SDL_StartTextInput(vk.window);
        // drop any held movement so the player doesn't run off
        player[my_player].goingf = player[my_player].goingb = 0;
        player[my_player].goingl = player[my_player].goingr = 0;
        player[my_player].running = player[my_player].sneaking = 0;
}

static void console_hide()
{
        console_open = 0;
        SDL_StopTextInput(vk.window);
}

static void console_submit()
{
        char line[300];
        snprintf(line, sizeof line, "<player %d> %s", my_player, console_input);
        chat_add(line);
        net_send_chat(console_input);
        console_input_len = 0;
        console_input[0] = '\0';
}

// key events go here first; returns 1 if the console consumed the key
int console_key(int down)
{
        if (!console_open)
        {
                if (down && !event.key.repeat && event.key.key == SDLK_T)
                        { console_show(); return 1; }
                return 0;
        }
        if (down) switch (event.key.key)
        {
                case SDLK_ESCAPE:
                        console_hide();
                        break;
                case SDLK_BACKSPACE:
                        if (console_input_len)
                                console_input[--console_input_len] = '\0';
                        break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                        if (event.key.repeat)
                                break;
                        if (console_input_len)
                                console_submit();
                        console_hide(); // chat closes on send, like Minecraft
                        break;
        }
        return 1;
}

void console_text(const char *text)
{
        if (!console_open) return;
        if (frame == console_opened_frame)
                return; // the keystroke that opened the box isn't input
        for (; *text; text++)
        {
                if (*text < ' ' || *text > '~')
                        continue; // printable ascii only
                if (console_input_len < sizeof console_input - 1)
                {
                        console_input[console_input_len++] = *text;
                        console_input[console_input_len] = '\0';
                }
        }
}

void console_draw()
{
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1.f) scale = 1.f;
        int lh = FONT_CH_H * scale;
        int inputy = screenh - 3.5f * lh;
        int base = inputy; // chat stacks upward from here

        if (console_open)
        {
                char line[300];
                snprintf(line, sizeof line, "> %s_", console_input);
                font_begin(screenw, screenh);
                font_add_text(line, 20, inputy, 0);
                font_end(1.f, 1.f, .5f);
        }

        // recent chat: always while the input is up, else until it expires
        int rows[CHAT_LINES], shown = 0;
        for (int k = 0; k < CHAT_LINES; k++)
        {
                int i = (chat_head + k) % CHAT_LINES; // oldest first
                if (!chat_log[i][0]) continue;
                if (!console_open && frame > chat_expire[i]) continue;
                rows[shown++] = i;
        }
        if (!shown) return;
        font_begin(screenw, screenh);
        for (int k = 0; k < shown; k++)
                font_add_text(chat_log[rows[k]], 20, base - (shown + 1 - k) * lh, 0);
        font_end(1.f, 1.f, 1.f);
}

#endif // BLOCKO_CONSOLE_C_INCLUDED
