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

add_executable(server
    main.cpp
    TCPServer.cpp
    TCPConnection.h
    TCPConnection.cpp
    PacketParser.cpp
    PacketParser.h
    PacketBuilder.h
    PacketBuilder.cpp
    Executor.h
    Executor.cpp
    RoomManager.cpp
    RoomManager.h
    User.h
    UserManager.h  
    UserManager.cpp
    Server.h 
    Server.cpp
    UdpAudioServer.h
    UdpAudioServer.cpp
)

target_include_directories(server PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/shared
)

if(WIN32)
    target_link_libraries(server PRIVATE
        Boost::headers
        opus
        ws2_32
        wsock32
    )
    target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)
else()
    target_link_libraries(server PRIVATE
        Boost::headers
        opus
        pthread
    )
endif()