@echo off
setlocal

reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && (set ARCH=x86) || (set ARCH=x64)
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && (set ARCHW=Win32) || (set ARCHW=x64)

set CC=..\_win\tcc-%ARCH%\tcc.exe
set GLEW=..\_win\glew-2.1.0
set SDL=..\_win\SDL2-2.0.5
set WINSDK="C:\Program Files (x86)\Windows Kits"

for /r %WINSDK% %%a in (*) do if "%%~nxa"=="Windows.h" set WININC=%%~dpa
if defined WININC (
	echo Found Windows.h at %WININC%
) else (
	echo Hi! This is TinyC.Games,
	echo To compile and run this game, I need some files from the Windows SDK
	echo:
	echo I couldn't find Windows.h anywhere in %WINSDK%
	echo Please set WINSDK to the path of the Windows SDK in this script
	echo:
	echo You may need to install the Windows SDK from Microsoft
	echo Here is the Windows 10 SDK for example:
	echo https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk
	echo Here are some earlier versions:
	echo https://developer.microsoft.com/en-us/windows/downloads/sdk-archive
	echo:
	echo Press any key to close this message . . .
	pause >nul
	exit /B
)

set PATH=%PATH%;%GLEW%\bin\Release\%ARCHW%
set PATH=%PATH%;%SDL%\lib\%ARCH%

%CC% -DSDL_MAIN_HANDLED -I%WININC% -I%WININC%\..\shared -I%GLEW%\include -I%SDL%\include -L%GLEW%\bin\Release\%ARCHW% -L%SDL%\lib\%ARCH% -lmsvcrt -lglew32 -lOpenGL32 -lSDL2 -run blocko.c
