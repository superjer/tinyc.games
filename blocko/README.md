# tinyc.games: Blocko

Blocko uses OpenGL for hardware-accelerated 3D graphics and may require installing additional software.

### Windows
1. Install Visual Studio 2019 or later
2. I recommend installing Community edition and selecting "Game development with C++" in the installer
3. Open the solution file vs2019-blocko\vs2019-blocko.sln
4. Click the Play button to build and run

### Mac
1. Install Xcode, minimally by running xcode-select --install
2. Install LLVM and libomp. I recommend installing Homebrew https://brew.sh/ and running:

    brew install llvm
    brew install libomp

3. Run run-mac.sh

### Linux
1. Install gcc, libSDL2, and libGLEW. E.g. for Debian/Ubuntu/Mint:

    sudo apt install gcc libsdl2-dev libglew-dev

2. Run run-linux.sh, or you can use make and run ./blocko
