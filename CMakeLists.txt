cmake_minimum_required(VERSION 3.11)
project(TinyCGames C)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/common/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  @ONLY
)

include(FetchContent)

set(OpenGL_GL_PREFERENCE GLVND)
set(ONLY_LIBS ON)
set(SDL_TEST_LIBRARY OFF)
set(glew-cmake_BUILD_SHARED OFF)

message(STATUS "fetching SDL3 from github")
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.4
)
FetchContent_MakeAvailable(SDL3)

message(STATUS "fetching SDL3_ttf from github")
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG prerelease-3.1.2
)
FetchContent_MakeAvailable(SDL3_ttf)

message(STATUS "fetching glew-cmake from github")
FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake
  GIT_TAG glew-cmake-2.2.0
)
FetchContent_MakeAvailable(glew)
#include_directories(${GLEW_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(flappy flappy-game/flappy.c)
add_executable(tet tet-game/tet.c)
add_executable(blocko blocko-game/blocko.c)

target_link_libraries(flappy PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_link_libraries(tet PRIVATE SDL3::SDL3 m libglew_static OpenGL::GL)
target_link_libraries(blocko PRIVATE OpenMP::OpenMP_C SDL3::SDL3 m libglew_static OpenGL::GL)
