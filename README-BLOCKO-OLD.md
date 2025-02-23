# tinyc.games: Blocko

Blocko uses OpenGL for hardware-accelerated 3D graphics and may require installing additional software.


### Windows
1. Install MSYS2 https://www.msys2.org/

    Please install to the default location of C:\msys64\

2. Optionally, install gcc according to MSYS2's instructions
3. Optionally, install make similarly:

    pacman -S make

4. Run run-windows.bat   (this will try to install gcc & make if you skipped them)


### Mac
1. Install Xcode, minimally by running `xcode-select --install`
2. Install LLVM and libomp. I recommend installing Homebrew https://brew.sh/ and running:

    brew install llvm
    brew install libomp

3. Run run-mac.sh


### Linux
1. Install gcc, libSDL2, and libGLEW. E.g. for Debian/Ubuntu/Mint:

    sudo apt install gcc libsdl2-dev libglew-dev

2. Run run-linux.sh
