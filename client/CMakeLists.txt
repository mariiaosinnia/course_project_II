cmake_minimum_required(VERSION 3.16)
project(client_skeleton CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_executable(client_skeleton
    main.cpp
    TCPClient.cpp
    PacketParser.cpp
    PacketBuilder.cpp
)

target_link_libraries(client_skeleton PRIVATE Boost::boost Threads::Threads)
