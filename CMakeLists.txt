cmake_minimum_required(VERSION 3.14)
project(xrgears)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(OpenXR REQUIRED)

add_subdirectory(shaders)
add_subdirectory(textures)
add_subdirectory(src)

