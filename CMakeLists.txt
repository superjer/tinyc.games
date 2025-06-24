cmake_minimum_required(VERSION 3.11)
project(TinyCGames C)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/common/build-config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/build-config.h
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
  GIT_TAG release-3.2.16
)
FetchContent_MakeAvailable(SDL3)

message(STATUS "fetching SDL3_ttf from github")
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
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

find_library(MATH_LIBRARY m)
find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(flappy flappy-game/flappy.c)
add_executable(tet tet-game/tet.c)
add_executable(blocko blocko-game/blocko.c)
add_executable(maker maker-game/maker.c)
add_executable(zel zel-game/zel.c)

target_include_directories(flappy PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(tet PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(blocko PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(maker PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(zel PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(flappy PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_link_libraries(tet PRIVATE SDL3::SDL3 libglew_static OpenGL::GL)
target_link_libraries(blocko PRIVATE OpenMP::OpenMP_C SDL3::SDL3 libglew_static OpenGL::GL)
target_link_libraries(maker PRIVATE SDL3::SDL3)
target_link_libraries(zel PRIVATE SDL3::SDL3)

if(MATH_LIBRARY)
    target_link_libraries(tet PUBLIC ${MATH_LIBRARY})
    target_link_libraries(blocko PUBLIC ${MATH_LIBRARY})
    target_link_libraries(zel PUBLIC ${MATH_LIBRARY})
endif()

macro(copy_dlls target)
    if(WIN32)
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
    endif()
endmacro()

copy_dlls(flappy)
copy_dlls(tet)
copy_dlls(blocko)
copy_dlls(maker)
copy_dlls(zel)
