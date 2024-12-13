gcc -std=c2x -Wall -pedantic \
        -Wno-overlength-strings \
        -g \
        -o tet tet.c \
        -I/usr/include/GL/ \
        `pkg-config sdl3 --cflags --libs` \
        -lm -lGLEW -lGL \
        && ./tet
