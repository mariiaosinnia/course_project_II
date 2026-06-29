add_executable(server
    main.cpp
    TCPServer.cpp
    TCPConnection.cpp
    PacketParser.cpp
    PacketBuilder.cpp
    Executor.cpp
    RoomManager.cpp
    UserManager.cpp
    Server.cpp
    UdpAudioServer.cpp
    Mp3Decoder.cpp
    Resampler.cpp
    AudioEncoder.cpp
    MusicStreamer.cpp
    UdpSocket.cpp
)

target_include_directories(server PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(server PRIVATE
    Boost::headers
    ${OPUS_TARGET}
    Threads::Threads
)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32 wsock32)
    target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)
endif()