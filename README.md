# tinyc.games
Tiny C games you can compile and run RIGHT NOW.

Tools and libraries are available for making cross-platform C games, but simple examples can be hard to find.

This project is an example of how you can get started making a game in C that will work on Windows, Mac and Linux.

## Screens

![Tet](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/tet-tiny.png)
![Flappy](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/flappy-tiny.png)
![Blocko](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/blocko3-tiny.png)

## How do I do it?

For Blocko, see blocko/README.md. For everything else:

### Windows
1. TCC, SDL2, and GLEW are already included!
2. Open a game folder and run run-windows.bat

### Mac
1. Install clang by typing "clang" in Terminal and clicking the Install button.
2. Open a game folder and run run-mac.sh

### Linux
1. Install tcc, libSDL2 with TTF support, and libGLEW. E.g. for Debian/Ubuntu/Mint:

    sudo apt install tcc libsdl2-dev libsdl2-ttf-dev libglew-dev

2. Open a game folder and run run-linux.sh

## Next?

After you've run a game, open the .c file in your favorite text editor. You're now looking at the actual code you just ran. You can edit it and run it again with your changes.
