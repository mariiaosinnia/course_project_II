cmake_minimum_required(VERSION 3.20)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(course_project_II LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)
option(USE_FETCHCONTENT_DEPS "Download dependencies with FetchContent when system packages are unavailable" ON)

find_package(Threads REQUIRED)
find_package(Boost QUIET)

if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
else()
    message(FATAL_ERROR "Boost headers were not found. Install libboost-all-dev on Ubuntu/Debian or use a Boost installation via vcpkg on Windows.")
endif()

include(FetchContent)

function(fetch_or_find_dependency dependency_name package_name repo_url tag)
    if(TARGET ${dependency_name} OR TARGET ${package_name}::${package_name} OR TARGET ${package_name})
        return()
    endif()

    set(_dependency_found FALSE)
    find_package(${package_name} QUIET CONFIG)
    if(TARGET ${dependency_name} OR TARGET ${package_name}::${package_name} OR TARGET ${package_name})
        message(STATUS "Using CMake package for ${dependency_name}")
        set(_dependency_found TRUE)
    endif()

    if(NOT _dependency_found)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            if(${package_name} STREQUAL "Opus")
                pkg_check_modules(OPUS QUIET IMPORTED_TARGET opus)
                if(TARGET PkgConfig::OPUS)
                    message(STATUS "Using pkg-config package for ${dependency_name}")
                    set(_dependency_found TRUE)
                endif()
            elseif(${package_name} STREQUAL "portaudio")
                pkg_check_modules(PORTAUDIO QUIET IMPORTED_TARGET portaudio-2.0)
                if(TARGET PkgConfig::PORTAUDIO)
                    message(STATUS "Using pkg-config package for ${dependency_name}")
                    set(_dependency_found TRUE)
                endif()
            endif()
        endif()
    endif()

    if(_dependency_found)
        return()
    endif()

    if(NOT USE_FETCHCONTENT_DEPS)
        message(FATAL_ERROR "Could not find ${package_name} and FetchContent dependency download is disabled")
    endif()

    message(STATUS "Falling back to FetchContent for ${dependency_name}")
    FetchContent_Declare(${dependency_name}
        GIT_REPOSITORY ${repo_url}
        GIT_TAG        ${tag}
    )

    if(${dependency_name} STREQUAL "opus")
        set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
        set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
    elseif(${dependency_name} STREQUAL "portaudio")
        set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(${dependency_name})
endfunction()

# Opus — audio codec, used by server (encoder) and client (decoder)
fetch_or_find_dependency(opus Opus https://github.com/xiph/opus.git v1.5.2)

# PortAudio — audio playback, used by client
fetch_or_find_dependency(portaudio portaudio https://github.com/PortAudio/portaudio.git v19.7.0)

set(OPUS_TARGET opus)
if(TARGET PkgConfig::OPUS)
    set(OPUS_TARGET PkgConfig::OPUS)
endif()

set(PORTAUDIO_TARGET portaudio)
if(TARGET PkgConfig::PORTAUDIO)
    set(PORTAUDIO_TARGET PkgConfig::PORTAUDIO)
endif()

# --- Targets ---

add_subdirectory(server)
add_subdirectory(client)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()