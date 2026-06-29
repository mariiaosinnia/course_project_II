cmake_minimum_required(VERSION 3.15)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(course_project_II LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
endif()

include(FetchContent)

FetchContent_Declare(
    opus
    GIT_REPOSITORY https://github.com/xiph/opus.git
    GIT_TAG v1.5.2
)

set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(opus)

FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG v19.7.0
)

set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(portaudio)

add_subdirectory(server)
add_subdirectory(client)