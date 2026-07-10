cmake_minimum_required(VERSION 3.11)
project(TinyCGames C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# emit compile_commands.json so clangd/clang-tidy pick up the real flags
# (-fopenmp, include dirs); without it clangd can't find <omp.h> and floods
# the editor with a bogus parse-error cascade.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Multi-config generators (Visual Studio, Xcode) default to per-config output
# dirs like build/Debug; keep executables in the build root, next to shaders/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${CONFIG} CONFIG_UPPER)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR})
endforeach()

include(FetchContent)

set(ONLY_LIBS ON)
set(SDL_TEST_LIBRARY OFF)

# SDL3_ttf's vendored freetype (used when no system freetype exists, e.g.
# Windows) declares a cmake_minimum_required too old for CMake 4+
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

message(STATUS "fetching SDL3 from github")
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.8
)
FetchContent_MakeAvailable(SDL3)

message(STATUS "fetching SDL3_ttf from github")
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG prerelease-3.1.2
)
FetchContent_MakeAvailable(SDL3_ttf)

find_library(MATH_LIBRARY m)
find_package(OpenMP) # optional: Apple Clang has none; blocko falls back to NO_OMPH
find_package(Vulkan REQUIRED)

# glslangValidator ships with the Vulkan SDK
find_program(GLSLANG_VALIDATOR glslangValidator
  HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin)
if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found - install the Vulkan SDK "
    "(https://vulkan.lunarg.com) and make sure its bin directory is in PATH "
    "or VULKAN_SDK is set")
endif()

# Bake the full glslangValidator path (and shader/source dirs) into
# build-config.h. The in-game F5 hot-reload shells out to glslangValidator, and
# on Windows the Vulkan SDK's bin dir is not on PATH, so a bare command name
# fails silently and the reload keeps the old shader. Emit the resolved path
# here (after find_program) so the runtime uses it verbatim.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/common/build-config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/build-config.h
  @ONLY
)

add_executable(flappy flappy-game/flappy.c)
add_executable(tet tet-game/tet.c)
add_executable(blocko blocko-game/blocko.c)
add_executable(maker maker-game/maker.c)
add_executable(zel zel-game/zel.c)
add_executable(noise noise-demo/noise_demo.c)

target_include_directories(flappy PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(tet PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(blocko PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(maker PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(zel PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(noise PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(flappy PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_link_libraries(tet PRIVATE SDL3::SDL3 Vulkan::Vulkan)
target_link_libraries(blocko PRIVATE SDL3::SDL3 Vulkan::Vulkan)
if(WIN32)
  target_link_libraries(blocko PRIVATE ws2_32) # winsock for net.c
endif()
if(OpenMP_C_FOUND)
  target_link_libraries(blocko PRIVATE OpenMP::OpenMP_C)
else()
  message(STATUS "OpenMP not found - building blocko without it (NO_OMPH)")
  target_compile_definitions(blocko PRIVATE NO_OMPH)
endif()
target_link_libraries(maker PRIVATE SDL3::SDL3)
target_link_libraries(zel PRIVATE SDL3::SDL3)
target_link_libraries(noise PRIVATE SDL3::SDL3 Vulkan::Vulkan)

if(MATH_LIBRARY)
  target_link_libraries(tet PUBLIC ${MATH_LIBRARY})
  target_link_libraries(blocko PUBLIC ${MATH_LIBRARY})
  target_link_libraries(zel PUBLIC ${MATH_LIBRARY})
  target_link_libraries(noise PUBLIC ${MATH_LIBRARY})
endif()

# Create shader output directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

set(SHADER_FILES
  blocko-game/shaders/main.vert
  blocko-game/shaders/main.frag
  blocko-game/shaders/mob.vert
  blocko-game/shaders/mob_shadow.vert
  blocko-game/shaders/shadow.vert
  blocko-game/shaders/shadow.frag
  blocko-game/shaders/shadow_solid.frag
  blocko-game/shaders/cursor.vert
  blocko-game/shaders/cursor.frag
  common/tinyc.games/shaders/font.vert
  common/tinyc.games/shaders/font.frag
  tet-game/shaders/tet.vert
  tet-game/shaders/tet.frag
  blocko-game/shaders/sky.vert
  blocko-game/shaders/sky.frag
  blocko-game/shaders/sun.vert
  blocko-game/shaders/sun.frag
  blocko-game/shaders/moon.vert
  blocko-game/shaders/moon.frag
  noise-demo/shaders/noise.vert
  noise-demo/shaders/noise.frag
)

# Use add_custom_command with OUTPUT for proper dependency tracking
foreach(SHADER ${SHADER_FILES})
  get_filename_component(SHADER_NAME ${SHADER} NAME)
  set(COMPILED_SHADER ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
  add_custom_command(
    OUTPUT ${COMPILED_SHADER}
    COMMAND ${GLSLANG_VALIDATOR} --quiet -V ${CMAKE_SOURCE_DIR}/${SHADER} -o ${COMPILED_SHADER}
    DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER}
            ${CMAKE_SOURCE_DIR}/blocko-game/shaders/sky_color.glsl
    COMMENT "Compiling shader: ${SHADER}"
  )
  list(APPEND COMPILED_SHADER_FILES ${COMPILED_SHADER})
endforeach()

# Target that depends on all compiled shaders
add_custom_target(shaders DEPENDS ${COMPILED_SHADER_FILES})
add_dependencies(blocko shaders)
add_dependencies(tet shaders)
add_dependencies(noise shaders)

if(WIN32)
  macro(copy_dlls target)
    add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:SDL3::SDL3-shared> ${CMAKE_BINARY_DIR}
      VERBATIM
    )
    add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:SDL3_ttf::SDL3_ttf-shared> ${CMAKE_BINARY_DIR}
      VERBATIM
    )
  endmacro()

  copy_dlls(flappy)
  copy_dlls(tet)
  copy_dlls(blocko)
  copy_dlls(maker)
  copy_dlls(zel)
  copy_dlls(noise)
endif()
