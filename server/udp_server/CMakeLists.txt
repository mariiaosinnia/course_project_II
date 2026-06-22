cmake_minimum_required(VERSION 3.15)
project(udp_audio_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

include(FetchContent)

FetchContent_Declare(
        opus
        GIT_REPOSITORY https://github.com/xiph/opus.git
        GIT_TAG        v1.5.2
)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(opus)

add_executable(udp_server udp_audio_server.cpp
        udp_audio_server.cpp)
target_include_directories(udp_server PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(udp_server PRIVATE opus ws2_32 wsock32)
    target_compile_definitions(udp_server PRIVATE _WIN32_WINNT=0x0601)
else()
    target_link_libraries(udp_server PRIVATE opus pthread)
endif()