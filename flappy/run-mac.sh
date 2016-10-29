export DYLD_FRAMEWORK_PATH=../mac

cc -I../mac/SDL2.framework/Headers -I../mac/SDL2_ttf.framework/Headers -F../mac -framework SDL2 -framework SDL2_ttf -o flappy flappy.c && ./flappy
