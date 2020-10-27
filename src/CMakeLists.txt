cmake_minimum_required(VERSION 3.10.2)

project("xrgears")

add_definitions(-DXR_OS_ANDROID)
add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_VULKAN)

find_package(Vulkan REQUIRED)
find_package(OpenXR REQUIRED)

add_subdirectory(../shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# build native_app_glue as a static lib
set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(xrgears SHARED
    main.cpp
    ktx_stream.c
    ktx_texture.c
    log.c
    main.cpp
    pipeline_equirect.cpp
    pipeline_gears.cpp
    xr.c
    xr_quad.c
    xr_equirect.c
    textures.c
    vulkan_texture.c
    vulkan_buffer.c
    vulkan_device.c
    vulkan_shader.c
    vulkan_context.c
    settings.c
    vulkan_framebuffer.c
)

target_include_directories(xrgears PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_BINARY_DIR}/shaders/
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/include
)

# add lib dependencies
target_link_libraries(xrgears
    android
    native_app_glue
    Vulkan::Vulkan
    OpenXR::openxr_loader
    log
)

add_dependencies(xrgears shadrs)
