add_executable(server
    main.cpp
    TCPServer.cpp
    TCPConnection.cpp
    PacketParser.cpp
    PacketBuilder.cpp
    Executor.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

if(WIN32)
    target_link_libraries(server PRIVATE
        ws2_32
        Boost::headers
    )
    target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0A00)
endif()