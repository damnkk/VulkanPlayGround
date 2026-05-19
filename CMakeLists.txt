cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME} LANGUAGES C CXX)

# Prior adding the package one can disable certain libraries
# e.g.
option(NVPRO2_ENABLE_nvgl OFF)
option(NVPRO2_ENABLE_nvgpu_monitor OFF)

find_path(NVPRO_CORE2_DIR
  NAMES cmake/Setup.cmake
  PATHS ${CMAKE_CURRENT_LIST_DIR}/External/nvpro_core2
  REQUIRED
  DOC "Path to nvpro_core2"
)
include(${NVPRO_CORE2_DIR}/cmake/Setup.cmake)

find_package(SDL3 CONFIG REQUIRED PATHS "$ENV{VULKAN_SDK}/cmake")

# Add compile definitions for nvutils to enable thread pool priority and deadlock check
# This ensures both the library and our application see the same class layout
if(TARGET nvutils)
  target_compile_definitions(nvutils PUBLIC 
    BS_THREAD_POOL_ENABLE_PRIORITY 
    BS_THREAD_POOL_ENABLE_WAIT_DEADLOCK_CHECK
  )
endif()

# implement spirv-reflect as static lib
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build spv-reflect as static lib" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/spv_reflect)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

#####################################################################################
# Source files for this project
#

FILE(GLOB_RECURSE SOURCE_FILES
  ${CMAKE_CURRENT_LIST_DIR}/code/*.cpp
)
list(APPEND SOURCE_FILES
  ${CMAKE_CURRENT_LIST_DIR}/External/miniply/miniply.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
#####################################################################################
# Crc32c
option (CRC32C_BUILD_TESTS OFF)
option (CRC32C_BUILD_BENCHMARKS OFF)
option (CRC32C_USE_GLOG OFF)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/Crc32)

#####################################################################################
# vrdx (vk_radix_sort)

set(VRDX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/vrdx)
if(NOT EXISTS ${VRDX_DIR}/CMakeLists.txt)
  message(FATAL_ERROR "vrdx dependency missing at External/vrdx")
endif()
add_subdirectory(${VRDX_DIR})

set_property(TARGET vk_radix_sort PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET spine_slang PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET downsweep_slang PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET downsweep_key_value_slang PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET upsweep_slang PROPERTY FOLDER "ThirdParty/vrdx")

#####################################################################################
# spz
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/spz)

#####################################################################################
# Executable

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

#####################################################################################
# webview
set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
set(WEBVIEW_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/webview)

#####################################################################################
# rttr
set(BUILD_RTTR_DYNAMIC OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(BUILD_INSTALLER OFF CACHE BOOL "" FORCE)
set(BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(USE_PCH OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/rttr EXCLUDE_FROM_ALL)

if(TARGET rttr_core_lib)
  set_property(TARGET rttr_core_lib PROPERTY FOLDER "ThirdParty/rttr")
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE
  ${NVPRO_CORE2_DIR}
  ${NVPRO_CORE2_DIR}/code/debugger
  ${CMAKE_CURRENT_LIST_DIR}/code/renderer
  ${CMAKE_CURRENT_LIST_DIR}/code
  ${CMAKE_CURRENT_LIST_DIR}/code/resourceManagement
  ${CMAKE_CURRENT_LIST_DIR}/External/spv_reflect/include
  ${CMAKE_CURRENT_LIST_DIR}/External/Crc32/include
  ${CMAKE_CURRENT_LIST_DIR}/External/miniply
  ${CMAKE_CURRENT_LIST_DIR}/External/vrdx/include
  ${CMAKE_CURRENT_LIST_DIR}/External/sqlite3
  ${CMAKE_CURRENT_LIST_DIR}/shaders
  ${CMAKE_CURRENT_LIST_DIR}/External/spz/src/cc


)
target_link_libraries(${PROJECT_NAME} PRIVATE
  nvpro2::nvapp
  nvpro2::nvaftermath
  nvpro2::nvimageformats
  nvpro2::nvnsight
  nvpro2::nvshaders_host
  nvpro2::nvslang
  nvpro2::nvutils
  nvpro2::nvvk
  nvpro2::nvvkglsl
  nvpro2::nvvkgltf
  SDL3::SDL3
  vk_radix_sort
  webview::core
  RTTR::Core_Lib
  spirv-reflect-static
  crc32c
  "${CMAKE_CURRENT_SOURCE_DIR}/External/sqlite3/libsqlite3.lib"
  spz
)

add_project_definitions(${PROJECT_NAME})

# This sample doesn't need addtional files, but one might need to
# copy required dlls, additional commands etc. through this command
message(STATUS "NsightAftermath_LIBRARY_DIR: ${NsightAftermath_LIBRARY_DIR}")
copy_to_runtime_and_install( ${PROJECT_NAME} 
FILES
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/gfx.dll
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/slang-glsl-module.dll
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/slang-glslang.dll
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/slang-llvm.dll
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/slang-rt.dll
${CMAKE_CURRENT_BINARY_DIR}/_deps/Slang-windows-x86_64-2025.13.1/bin/slang.dll
${NsightAftermath_DLLS}
 AUTO)

