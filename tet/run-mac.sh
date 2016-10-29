export DYLD_FRAMEWORK_PATH=../mac
cc -I../mac/SDL2.framework/Headers -F../mac -framework SDL2 -o tet tet.c && ./tet
