export DYLD_FRAMEWORK_PATH=../_mac

/usr/local/opt/llvm/bin/clang \
	-w -Ofast \
        -DGL_SILENCE_DEPRECATION \
	-Xpreprocessor -fopenmp \
        -I ../_mac/SDL2.framework/Headers \
        -F ../_mac \
        -framework OpenGL \
        -framework SDL2 \
	-L/usr/local/Cellar/libomp/10.0.0/lib \
	-lomp \
        -o blocko blocko.c && ./blocko
