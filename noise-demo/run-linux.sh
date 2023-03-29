gcc -std=c2x -Wall -pedantic \
        -g \
        -Wno-overlength-strings \
        -I/usr/include/GL/ \
        -I/usr/include/SDL2 \
        -o noise_demo noise_demo.c \
        -lm -lGLEW -lSDL2 -lGL \
        && ./noise_demo

#       -fopenmp \
#       -O3 \
