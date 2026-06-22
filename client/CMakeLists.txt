cmake_minimum_required(VERSION 3.15)
project(audio_client_app)

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

FetchContent_Declare(
        portaudio
        GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
        GIT_TAG        v19.7.0
)
set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(portaudio)


add_executable(client
        main.cpp
        TCPClient.cpp
        PacketParser.cpp
        PacketBuilder.cpp
        UDPClient.cpp
)


target_include_directories(client PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/shared
        ${portaudio_SOURCE_DIR}/include
)


if(WIN32)
    target_link_libraries(client PRIVATE
            Boost::headers
            opus
            portaudio
            ws2_32
            wsock32
    )
    target_compile_definitions(client PRIVATE _WIN32_WINNT=0x0601)
else()

    target_link_libraries(client PRIVATE
            Boost::headers
            opus
            portaudio
            pthread
    )
endif()