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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

#####################################################################################
# Source files for this project
#

FILE(GLOB_RECURSE SOURCE_FILES
  # ${CMAKE_CURRENT_LIST_DIR}/code/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/code/main.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})

#####################################################################################
# Executable
#

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE
  ${NVPRO_CORE2_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/code/renderer
  ${CMAKE_CURRENT_LIST_DIR}/code/resourceManagement

)
target_link_libraries(${PROJECT_NAME} PRIVATE
  nvpro2::nvapp
  nvpro2::nvaftermath
  nvpro2::nvgui
  nvpro2::nvimageformats
  nvpro2::nvnsight
  nvpro2::nvshaders_host
  nvpro2::nvslang
  nvpro2::nvutils
  nvpro2::nvvk
  nvpro2::nvvkglsl
  nvpro2::nvvkgltf
)

add_project_definitions(${PROJECT_NAME})

# This sample doesn't need addtional files, but one might need to
# copy required dlls, additional commands etc. through this command
copy_to_runtime_and_install( ${PROJECT_NAME}  AUTO)

