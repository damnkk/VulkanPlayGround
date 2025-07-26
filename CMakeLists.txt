cmake_minimum_required(VERSION 3.10)

get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJNAME VulkanPlayGround)
project(${PROJNAME} VERSION 1.0 LANGUAGES  C CXX)

message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)



if(NOT BASE_DIRECTORY)
find_path(BASE_DIRECTORY
NAMES nvpro_core/cmake/setup.cmake
PATHS ${CMAKE_CURRENT_SOURCE_DIR}/External
REQUIRED
DOC "Directory containing nvpro_core"
)
endif()

execute_process(
        COMMAND python ./External/shaderc/utils/git-sync-deps
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE GLSLANG_SYNC_RESULT
)
if(NOT GLSLANG_SYNC_RESULT EQUAL "0")
    message(FATAL_ERROR "Can not clone spirv-tools, glslang will not be compile successfully")
endif()
# 在add_subdirectory(External/shaderc)之前添加
add_definitions(-DBUILD_SHARED_LIBS=OFF)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)

# 强制shaderc使用静态版本的依赖
# set(SPIRV_TOOLS_BUILD_STATIC ON CACHE BOOL "Build SPIRV-Tools as static library")
add_subdirectory(External/shaderc)

## Various functions and macros REQUIRED
if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else()
message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing nvpro_core")
endif()



# if(MSVC)
# add_definitions(/wd26812)  # 'enum class' over 'enum'
# add_definitions(/wd26451)  # Arithmetic overflow, casting 4 byte value to 8 byte value
# endif()

set(OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build)
FILE(GLOB_RECURSE COMMON_SOURCE_FILES
${PROJECT_SOURCE_DIR}/code/*.cpp
)

add_executable(VulkanPlayGround 
${COMMON_SOURCE_FILES}
)
_add_project_definitions(${PROJNAME})
compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders" 
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv" 
	VULKAN_TARGET "vulkan1.3"
	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
  FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/External/nvpro_core/nvvkhl/shaders"
)
compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/volumeRender" 
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv" 
	VULKAN_TARGET "vulkan1.3"
	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
  FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/External/nvpro_core/nvvkhl/shaders"
)
compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/VRS" 
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv" 
	VULKAN_TARGET "vulkan1.3"
	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
  FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/External/nvpro_core/nvvkhl/shaders"
)

# Package shared by all projects
_add_package_VulkanSDK()
_add_package_ImGUI()
_add_nvpro_core_lib()
add_subdirectory(External/Assimp)

add_definitions(-DBUILD_SHARED_LIBS=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF )
target_compile_definitions(${PROJNAME} PUBLIC ALLOC_VMA)
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${GLSL_SOURCES})
target_sources(${PROJNAME} PUBLIC ${GLSL_HEADERS})
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core assimp shaderc)
target_link_libraries(${PROJNAME}  "${PROJECT_SOURCE_DIR}/code/debugger/Nsight/graphics/lib/x64/NGFX_Injection.lib")

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

target_include_directories(VulkanPlayGround PRIVATE 
    ${PROJECT_SOURCE_DIR}/code
    ${PROJECT_SOURCE_DIR}/code/renderer
    ${PROJECT_SOURCE_DIR}/code/resourceManagement
    ${PROJECT_SOURCE_DIR}/shaders
    ${CMAKE_SOURCE_DIR}/External/Assimp/include
    ${CMAKE_SOURCE_DIR}/External/shaderc/libshaderc/include
    ${CMAKE_SOURCE_DIR}/External/shaderc/libshaderc_util/include
)

add_custom_command(TARGET ${PROJNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/code/debugger/Nsight/graphics/lib/x64/NGFX_Injection.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug/NGFX_Injection.dll"
)
add_custom_command(TARGET ${PROJNAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:assimp>"
    "$<TARGET_FILE_DIR:${PROJNAME}>/"
)
target_copy_to_output_dir()
_finalize_target( ${PROJNAME} )
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/spv")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/spv")
