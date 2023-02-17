export DYLD_FRAMEWORK_PATH=../common/macos

/usr/local/opt/llvm/bin/clang \
	-w -Ofast \
        -DGL_SILENCE_DEPRECATION \
	-Xpreprocessor -fopenmp \
        -I ../common/macos/SDL2.framework/Headers \
        -F ../common/macos \
        -framework OpenGL \
        -framework SDL2 \
	-L/usr/local/Cellar/libomp/10.0.0/lib \
	-lomp \
        -o blocko main.c && ./blocko
