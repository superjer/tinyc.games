cmake_minimum_required(VERSION 3.15)

project(vulkan_test)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(SDL_TEST_LIBRARY OFF)

include(FetchContent)

message(STATUS "fetching SDL3 from github")
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.8
)
FetchContent_MakeAvailable(SDL3)

add_executable(vulkan-triangle main.c)
# compiles in the demo main() and the stb_image implementation
target_compile_definitions(vulkan-triangle PRIVATE TINYC_VULKAN_TRIANGLE_MAIN)

if(UNIX AND NOT APPLE)
	target_link_libraries(
    vulkan-triangle
		/usr/lib/libvulkan.so
    SDL3::SDL3
		/usr/lib/libm.so
  )
endif()

add_custom_target(shaders
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders)

add_custom_target(triangle.vert.spv
	COMMAND glslangValidator --quiet -V ${CMAKE_SOURCE_DIR}/shaders/triangle.vert -o ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv)

add_custom_target(triangle.geom.spv
	COMMAND glslangValidator --quiet -V ${CMAKE_SOURCE_DIR}/shaders/triangle.geom -o ${CMAKE_BINARY_DIR}/shaders/triangle.geom.spv)

add_custom_target(triangle.frag.spv
	COMMAND glslangValidator --quiet -V ${CMAKE_SOURCE_DIR}/shaders/triangle.frag -o ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv)

add_dependencies(vulkan-triangle
	shaders
	triangle.vert.spv
	triangle.geom.spv
	triangle.frag.spv)
