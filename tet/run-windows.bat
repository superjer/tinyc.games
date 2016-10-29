@echo off
setlocal
set PATH=%PATH%;..\SDL2-2.0.5\lib\x64
set PATH=%PAtH%;..\SDL2_ttf-2.0.14\x86_64-w64-mingw32\lib
set PATH=%PAtH%;..\SDL2_ttf-2.0.14\x86_64-w64-mingw32\bin

..\tcc\tcc.exe -DSDL_MAIN_HANDLED -I..\SDL2-2.0.5\include -L..\SDL2-2.0.5\lib\x64 -lSDL2 -run tet.c

