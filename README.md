# tinyc.games
Tiny C games you can compile and run RIGHT NOW.

Tools and libraries are available for making cross-platform C games, but simple examples can be hard to find.

This project is an example of how you can get started making a game in C that will work on Windows, Mac and Linux.

## Screens

![Tet](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/tet-tiny.png)
![Flappy](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/flappy-tiny.png)
![Blocko](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/shadow-blocko.png)

## Games

### Tet
A Tetris clone which is getting way too large. Cool animations, sound, controller support, multiplayer etc.
This is too much for a "tiny" game so I'll be paring it down and moving the fully featured version to its own branch or repo.

### Flappy
The original tiny game. It's just Flappy Bird in only around 200 [normal] lines of code. So no dumb tricks, I hope :)

### Blocko
A Minecraft (Infiniminer?) clone with too many features once again. Dynamic lighting and overly complex procedural generation.
Also needs to be simplified for this repo.

### Zel
A top-down adventure game with an overworld and a dungeon. Very minimal, no ending. Too much procedural generation!

### Maker
A quick hack to make Zel into more of a 2D platformer. Very rough around the edges. The intention is to add a simple level editor.

## How do I do it?

Use CMake

### Linux
1. Create a build directory here
    ```
    mkdir build
    cd build
    ```

2. Run CMake to fetch libraries and create build scripts
    ```
    cmake ..
    ```

3. Build!
    ```
    cmake --build .                   # build all games
    cmake --build . --target=blocko   # build only blocko
    ```

4. Run, for example, blocko
    ```
    ./blocko
    ```

### Mac, Windows
CMake is supposed to be cross-platform and should work on Mac and Windows. Specific instructions coming soon.

## Next?

After you're done playing an exhilirating demo game, open the .c file in your favorite text editor. You're now looking at the actual code you just ran. So feel free to tinker and run it with your changes. Or use it as a starting point for your own game!
