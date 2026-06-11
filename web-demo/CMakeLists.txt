add_executable(ws_server
    main.cpp
)

target_link_libraries(ws_server PRIVATE
    ws2_32
    Boost::headers
)

target_compile_definitions(ws_server PRIVATE _WIN32_WINNT=0x0A00)