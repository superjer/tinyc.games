gcc -std=c2x -Wall -pedantic \
        -g \
        -Wno-overlength-strings \
        -I/usr/include/GL/ \
        -I/usr/include/SDL2 \
        -o noise noise.c \
        -lm -lGLEW -lSDL2 -lGL \
        && ./noise

#       -fopenmp \
#       -O3 \
