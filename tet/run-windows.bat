@echo off
setlocal

set CC=..\_win\tcc\tcc.exe
set SDL=..\_win\SDL2-2.0.5
set TTF=..\_win\SDL2_ttf-2.0.14\x86_64-w64-mingw32

set PATH=%PATH%;%SDL%\lib\x64
set PATH=%PATH%;%TTF%\lib
set PATH=%PATH%;%TTF%\bin

%CC% -DSDL_MAIN_HANDLED -I%SDL%\include -I%TTF%/include/SDL2 -L%SDL%\bin\x64 -L%TTF%/bin -lSDL2 -lSDL2_ttf -run tet.c
