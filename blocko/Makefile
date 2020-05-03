
release:
	gcc -fopenmp -O3 -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin blocko.c -lm -lGLEW -lSDL2 -lGL

debug:
	gcc -fopenmp -O3 -g -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin blocko.c -lm -lGLEW -lSDL2 -lGL

clean:
	rm -f bin
