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

find_package(SDL3 QUIET)
if (NOT SDL3_FOUND)
  message(STATUS "SDL3 not found, fetching from github")
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.4
  )
  FetchContent_MakeAvailable(SDL3)
endif()

message(STATUS "fetching glew-cmake from github")
FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake
  GIT_TAG glew-cmake-2.2.0
)
FetchContent_MakeAvailable(glew)
#include_directories(${GLEW_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)

add_executable(tet tet-game/tet.c)

target_link_libraries(tet PRIVATE SDL3::SDL3 m libglew_static OpenGL::GL)
