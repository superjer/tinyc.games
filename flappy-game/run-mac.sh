export DYLD_FRAMEWORK_PATH=../common/macos

cc -I../common/macos/SDL2.framework/Headers -I../common/macos/SDL2_ttf.framework/Headers -F../common/macos -framework SDL2 -framework SDL2_ttf -o flappy flappy.c && ./flappy
