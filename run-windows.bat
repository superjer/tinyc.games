@echo off
setlocal

set CC=win\tcc\tcc.exe
set SDL=win\SDL2-2.0.5
set TTF=win\SDL2_ttf-2.0.14\x86_64-w64-mingw32

set PATH=%PATH%;%SDL%\lib\x64
set PATH=%PAtH%;%TTF%\lib
set PATH=%PAtH%;%TTF%\bin

%CC% -DSDL_MAIN_HANDLED -I%SDL%\include -I%TTF%/include/SDL2 -L%SDL%\bin\x64 -L%TTF%/bin -lSDL2 -lSDL2_ttf -run flappy.c
