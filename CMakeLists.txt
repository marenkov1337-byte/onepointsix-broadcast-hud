cmake_minimum_required(VERSION 3.20)
project(onepointsix_broadcast LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(HLSDK_DIR ${CMAKE_SOURCE_DIR}/external/hlsdk)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/external/lodepng/*.cpp
)

add_library(onepointsix_broadcast SHARED ${SRC_FILES})

target_compile_definitions(onepointsix_broadcast PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0601
        _WIN32_WINNT=0x0601
)

target_include_directories(onepointsix_broadcast PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/lodepng
        ${HLSDK_DIR}
        ${HLSDK_DIR}/cl_dll
        ${HLSDK_DIR}/common
        ${HLSDK_DIR}/devtools
        ${HLSDK_DIR}/dlls
        ${HLSDK_DIR}/dmc
        ${HLSDK_DIR}/engine
        ${HLSDK_DIR}/external
        ${HLSDK_DIR}/game
        ${HLSDK_DIR}/game_shared
        ${HLSDK_DIR}/lib
        ${HLSDK_DIR}/pm_shared
        ${HLSDK_DIR}/utils
        ${HLSDK_DIR}/public
)

target_link_libraries(onepointsix_broadcast
        user32
        gdi32
        winmm
        opengl32
)

set_target_properties(onepointsix_broadcast PROPERTIES OUTPUT_NAME "onepointsix_broadcast")
