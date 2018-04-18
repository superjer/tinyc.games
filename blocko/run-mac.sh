export DYLD_FRAMEWORK_PATH=../_mac

cc -I../_mac/SDL2.framework/Headers -I../_mac/SDL2_ttf.framework/Headers -I../_mac/SDL2_image.framework/Headers -F../_mac -framework SDL2 -framework SDL2_ttf -framework SDL2_image -o blocko blocko.c && ./blocko
