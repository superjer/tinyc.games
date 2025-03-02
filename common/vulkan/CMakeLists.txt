cmake_minimum_required(VERSION 3.15)

project(vulkan_test)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(vulkan-triangle main.c)

if(UNIX AND NOT APPLE)
	target_link_libraries(vulkan-triangle
		/usr/lib/libvulkan.so
		/usr/lib/libglfw.so
		/usr/lib/libm.so)
endif()

add_custom_target(shaders
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders)

add_custom_target(triangle_vertex.spv
	COMMAND glslangValidator --quiet -V ${CMAKE_SOURCE_DIR}/shaders/triangle.vert -o ${CMAKE_BINARY_DIR}/shaders/triangle_vertex.spv)

add_custom_target(triangle_fragment.spv
	COMMAND glslangValidator --quiet -V ${CMAKE_SOURCE_DIR}/shaders/triangle.frag -o ${CMAKE_BINARY_DIR}/shaders/triangle_fragment.spv)

add_dependencies(vulkan-triangle
	shaders
	triangle_vertex.spv
	triangle_fragment.spv)
