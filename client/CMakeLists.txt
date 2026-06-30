add_executable(client
    main.cpp
    TCPClient.cpp
    PacketParser.cpp
    PacketBuilder.cpp
    ClientApp.cpp
    Logger.cpp
    UDPClient.cpp
)

target_include_directories(client PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(client PRIVATE
    Boost::headers
    ${OPUS_TARGET}
    ${PORTAUDIO_TARGET}
    Threads::Threads
    ftxui::screen
    ftxui::dom
    ftxui::component
)

if(WIN32)
    target_link_libraries(client PRIVATE ws2_32 wsock32)
    target_compile_definitions(client PRIVATE _WIN32_WINNT=0x0601)
endif()