export DYLD_FRAMEWORK_PATH=../common/macos

cc -I../common/macos/SDL2.framework/Headers -F../common/macos -framework SDL2 -o tet tet.c && ./tet
