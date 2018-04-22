export DYLD_FRAMEWORK_PATH=../_mac

cc -I../_mac/SDL2.framework/Headers -I../_mac/SDL2_ttf.framework/Headers -I/opt/local/include -L/opt/local/lib -lglew -framework OpenGL -F../_mac -framework SDL2 -framework SDL2_ttf -o blocko blocko.c && ./blocko
