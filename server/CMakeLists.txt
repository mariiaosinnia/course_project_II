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
)

target_include_directories(server PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(server PRIVATE
    Boost::system
    ws2_32
    wsock32
)

target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)