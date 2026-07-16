# Convenience wrapper: `make` in the repo root configures (if needed) and
# builds via CMake in ./build. Pass a game name to build one target, e.g.
# `make blocko`. `make clean` cleans; `make distclean` removes ./build.

BUILD_DIR := build

EMOJIS := 😀 😁 😄 😆 😉 😊 😋 😎 🤩 🤠 🎈 ✨ ⭐ 🌟 💥 🌈 🍾 🥂 🎮 🦄 🐉 😺 🤖
emoji = $$(set -- $(EMOJIS); shift $$(awk -v n=$(words $(EMOJIS)) -v s=$$$$ 'BEGIN { srand(s); print int(rand() * n) }'); printf '%s' "$$1")

.PHONY: all clean distclean blocko flappy tet zel maker

all: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR)
	@e=$(emoji); printf "\n\033[1;32m  $$e BUILD SUCCEEDED — let's goooo! $$e\033[0m\n\n"
	@printf '  \033[1;36mRun a game:\033[0m\n'
	@printf '    \033[1;33m./$(BUILD_DIR)/blocko\033[0m   \033[1;35m./$(BUILD_DIR)/tet\033[0m   \033[1;34m./$(BUILD_DIR)/flappy\033[0m   \033[1;32m./$(BUILD_DIR)/zel\033[0m   \033[1;31m./$(BUILD_DIR)/maker\033[0m\n\n'

$(BUILD_DIR)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR)

blocko flappy tet zel maker: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target=$@
	@e=$(emoji); printf "\n\033[1;32m  $$e $@ built — go play! \033[0m Run it with: \033[1;33m./$(BUILD_DIR)/$@\033[0m\n\n"

clean: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target=clean

distclean:
	rm -rf $(BUILD_DIR)
