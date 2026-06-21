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
        TCPServer.h
)

target_include_directories(server PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/shared
)

if(WIN32)
    target_link_libraries(server PRIVATE
        Boost::headers
        ws2_32
        wsock32
    )
    target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)
else()
    target_link_libraries(server PRIVATE
        Boost::headers
    )
endif()
