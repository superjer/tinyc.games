@echo off
setlocal

reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && (set ARCH=x86) || (set ARCH=x64)

set CC=..\_win\tcc-%ARCH%\tcc.exe
set SDL=..\_win\SDL2-2.0.5
set TTF=..\_win\SDL2_ttf-2.0.14

echo %CC%

set PATH=%PATH%;%SDL%\lib\%ARCH%
set PATH=%PATH%;%TTF%\lib\%ARCH%

%CC% -DSDL_MAIN_HANDLED -I%SDL%\include -I%TTF%\include -L%SDL%\lib\%ARCH% -L%TTF%\lib\%ARCH% -lSDL2 -lSDL2_ttf -run tet.c
