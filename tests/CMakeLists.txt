add_executable(tests
    main.cpp
    test_packet_parser.cpp
    test_packet_builder.cpp
    test_executor.cpp
    test_room_manager.cpp
    test_user_manager.cpp

    ${CMAKE_SOURCE_DIR}/server/Executor.cpp
    ${CMAKE_SOURCE_DIR}/server/PacketParser.cpp
    ${CMAKE_SOURCE_DIR}/server/PacketBuilder.cpp
    ${CMAKE_SOURCE_DIR}/server/RoomManager.cpp
    ${CMAKE_SOURCE_DIR}/server/UserManager.cpp
    ${CMAKE_SOURCE_DIR}/boost_throw_exception_compat.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/shared
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Boost::headers
    Threads::Threads
)

if(WIN32)
    target_link_libraries(tests PRIVATE ws2_32 wsock32)
    target_compile_definitions(tests PRIVATE _WIN32_WINNT=0x0601)
endif()

include(GoogleTest)
gtest_discover_tests(tests)
