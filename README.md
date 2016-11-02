# tinyc.games
Tiny C games you can compile and run RIGHT NOW.

Tools and libraries are available for making cross-platform C games very easily, but simple examples were hard to find.

This project is an example of how you can get started making a game in C that will work on Windows, Mac and Linux with mimimal installation and configuration.

For Windows and Linux, we use the Tiny C Compliler (tcc) and for Mac we use gcc or clang.

## Screens

![Tet](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/tet-tiny.png)
![Flappy](https://raw.githubusercontent.com/superjer/tinyc.games/gh-pages/images/flappy-tiny.png)

## Windows
Everything you need to run the C code is already included in this repo, including TCC and SDL. Just run a run-windows.bat!

## Mac
You will need to install gcc or clang, then just run a run-mac.sh!

See this page to get gcc or clang installed by way of installing tcc (which we won't actually use!):

http://macappstore.org/tcc/

If someone can figure out how to run tcc with SDL2 on Mac, please advise!

## Linux
Install tcc and the SDL2 dev libraries and then run a run-linux.sh!

Example to install tcc and SDL2:

    sudo apt install tcc libsdl2-dev libsdl2-ttf-dev
