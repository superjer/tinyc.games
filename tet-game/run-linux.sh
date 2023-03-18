gcc -std=c2x -Wall -pedantic \
        -I/usr/include/GL/ \
        -I/usr/include/SDL2 \
        -o tet tet.c \
        -lm -lGLEW -lSDL2 -lSDL2_ttf -lGL \
        && ./tet
