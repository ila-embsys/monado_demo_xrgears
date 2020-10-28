# Copyright (c) 2017 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author:
#

#include(StdFilesystemFlags.cmake)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/../external)

add_library(openxr_loader STATIC
    api_layer_interface.cpp
    api_layer_interface.hpp
    loader_core.cpp
    loader_instance.cpp
    loader_instance.hpp
    loader_logger.cpp
    loader_logger.hpp
    loader_logger_recorders.cpp
    loader_logger_recorders.hpp
    manifest_file.cpp
    manifest_file.hpp
    runtime_interface.cpp
    runtime_interface.hpp
    common/hex_and_handles.h
    common/object_info.cpp
    common/object_info.h
    common/platform_utils.hpp
    xr_generated_dispatch_table.c
    xr_generated_loader.hpp
    xr_generated_loader.cpp
)

target_sources(openxr_loader
    PRIVATE
    ${EXTERNAL_DIR}/jsoncpp/src/lib_json/json_reader.cpp
    ${EXTERNAL_DIR}/jsoncpp/src/lib_json/json_value.cpp
    ${EXTERNAL_DIR}/jsoncpp/src/lib_json/json_writer.cpp
)
target_include_directories(openxr_loader
    PRIVATE
    ${EXTERNAL_DIR}/jsoncpp/include
)
if(SUPPORTS_Werrorunusedparameter)
    # Don't error on this - triggered by jsoncpp
    target_compile_options(openxr_loader PRIVATE -Wno-unused-parameter)
endif()

set_target_properties(openxr_loader PROPERTIES FOLDER "Loader")

set_source_files_properties(${LOADER_EXTERNAL_GEN_FILES} PROPERTIES GENERATED TRUE)

target_include_directories(
    openxr_loader
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common
            ${PROJECT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/common

            # for OpenXR headers
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_BINARY_DIR}/include

            # for generated dispatch table, common_config.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_BINARY_DIR}/..

            # for target-specific generated files
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
)

if(VulkanHeaders_FOUND)
    target_include_directories(openxr_loader PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()
if(NOT BUILD_LOADER_WITH_EXCEPTION_HANDLING)
    target_compile_definitions(openxr_loader PRIVATE XRLOADER_DISABLE_EXCEPTION_HANDLING)
endif()

find_library(android-log log)

target_link_libraries(
    openxr_loader
    PRIVATE ${CMAKE_DL_LIBS}
    PUBLIC ${CMAKE_THREAD_LIBS_INIT}
    ${android-log}
)

target_compile_definitions(openxr_loader PRIVATE API_NAME="OpenXR")

set(JNIPP_ROOT ${EXTERNAL_DIR}/jnipp)
set(JNIWRAPPER_ROOT ${EXTERNAL_DIR}/android-jni-wrappers)
file(GLOB ANDROID_WRAP_SOURCES ${JNIWRAPPER_ROOT}/wrap/*.cpp)
set_target_properties(openxr_loader
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED TRUE)
target_sources(openxr_loader
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/android_utilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/android_utilities.h
    ${ANDROID_WRAP_SOURCES}
    ${JNIPP_ROOT}/jnipp.cpp
)
target_include_directories(openxr_loader
    PRIVATE
    ${JNIPP_ROOT}
    ${JNIWRAPPER_ROOT}
)
target_link_libraries(openxr_loader
    PRIVATE
    ${ANDROID_LOG_LIBRARY}
)


if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(
        openxr_loader
        PRIVATE -Wextra
                -fno-strict-aliasing
                -fno-builtin-memcmp
                "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>"
                -ffunction-sections
                -fdata-sections
    )
    # Make build depend on the version script/export map
    target_sources(openxr_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.map)
    # Add the linker flag.
    set_target_properties(openxr_loader PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.map")
    # For GCC version 7.1 or greater, we need to disable the implicit fallthrough warning since
    # there's no consistent way to satisfy all compilers until they all accept the C++17 standard
    if(CMAKE_COMPILER_IS_GNUCC AND NOT (CMAKE_CXX_COMPILER_VERSION LESS 7.1))
        target_compile_options(openxr_loader PRIVATE -Wimplicit-fallthrough=0)
    endif()
endif()


