# tinyc.games
Tiny C games you can compile and run RIGHT NOW.

Tools and libraries are available for making cross-platform C games, but simple examples can be hard to find.

This project is an example of how you can get started making a game in C that will work on Windows, Mac and Linux.

## Screenshots

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

## How do I run this?

Anything that supports CMake should work. But here are some suggestions for various platforms:

### Windows

1. Install Visual Studio (you can use the free Community Edition)
   Make sure to check the box for "C++ CMake Tools for Windows." This may be under "Desktop development with C++"

2. Open the tinyc.games folder in Visual Studio. It should detect the CMake config and set everything up

3. Use the play button to build & run, e.g. `blocko.exe`

### Linux

1. Create a build directory inside tinyc.games
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

### MacOS

1. Install CMake. For example if you have Homebrew, run `brew install cmake`

2. In the tinyc.games directory, run `cmake . -G Xcode` to generate an Xcode project file

3. Open the project file with Xcode

## Next?

After you're done playing an exhilirating demo game, open a .c file in your favorite text editor. You're now looking at the actual code you just ran. So feel free to tinker and run it with your changes. Or use it as a starting point for your own game!
