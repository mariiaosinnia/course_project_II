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
    $<TARGET_PROPERTY:${OPUS_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:${PORTAUDIO_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
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


if(TARGET ${OPUS_TARGET} OR TARGET ${PORTAUDIO_TARGET})
    set(_runtime_deps client)

    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:client>
            $<TARGET_FILE_DIR:client>
        COMMAND_EXPAND_LISTS
    )

    if(WIN32)
        set_property(TARGET client PROPERTY
            VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:client>
        )
    endif()
endif()