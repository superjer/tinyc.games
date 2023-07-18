
release:
	gcc -fopenmp -O3 -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin main.c -lm -lGLEW -lSDL2 -lGL

release-singlethreaded:
	gcc -DTERRAIN_THREAD=0 -fopenmp -O3 -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin main.c -lm -lGLEW -lSDL2 -lGL

debug:
	gcc -fopenmp -O3 -g -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin main.c -lm -lGLEW -lSDL2 -lGL

debug-singlethreaded:
	gcc -DTERRAIN_THREAD=0 -fopenmp -O3 -g -Wall -Wextra -Wno-unused-parameter -I/usr/include/GL/ -I/usr/include/SDL2 -o bin main.c -lm -lGLEW -lSDL2 -lGL

clean:
	rm -f bin bin.exe *.dll

windows: SDL2.dll glew32.dll #libgcc_s_seh-1.dll libgomp-1.dll libwinpthread-1.dll
	gcc --std=c99 -static -fopenmp -I../common/windows/glew-2.1.0/include -I../common/windows/SDL2-2.28.1/include -o bin.exe main.c -lm -lglew32 -lSDL2 -lopenGL32 -L../common/windows/glew-2.1.0/lib/Release/x64 -L../common/windows/SDL2-2.28.1/lib/x64

SDL2.dll:
	cp ../common/windows/SDL2-2.28.1/lib/x64/SDL2.dll .

glew32.dll:
	cp ../common/windows/glew-2.1.0/bin/Release/x64/glew32.dll .

#libgcc_s_seh-1.dll:
#	cp /ucrt64/bin/libgcc_s_seh-1.dll .

#libgomp-1.dll:
#	cp /ucrt64/bin/libgomp-1.dll .

#libwinpthread-1.dll:
#	cp /ucrt64/bin/libwinpthread-1.dll .
