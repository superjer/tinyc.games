export DYLD_FRAMEWORK_PATH=../_mac

cc \
        -I /System/Library/Frameworks/OpenGL.framework/Headers \
        -I ../_mac/SDL2.framework/Headers \
        -F ../_mac \
        -framework OpenGL \
        -framework SDL2 \
        -o blocko blocko.c && ./blocko
