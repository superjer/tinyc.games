export DYLD_FRAMEWORK_PATH=../_mac
cc -I../_mac/SDL2.framework/Headers -F../_mac -framework SDL2 -o tet tet.c && ./tet
