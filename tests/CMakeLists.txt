include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(course_project_tests
    main.cpp
    udp_audio_tests.cpp
    ../shared/MusicStreamer.cpp
    ../shared/AudioEncoder.cpp
    ../client/UDPClient.cpp
    ../client/Logger.cpp
    ../server/UdpAudioServer.cpp
    ../server/UdpSocket.cpp
    ../server/Resampler.cpp
    ../server/Mp3Decoder.cpp
    ../server/RoomManager.cpp
    ../server/UserManager.cpp
    ../server/PacketBuilder.cpp
    ../boost_throw_exception_compat.cpp
)

target_include_directories(course_project_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/client
    $<TARGET_PROPERTY:${OPUS_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:${PORTAUDIO_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(course_project_tests PRIVATE
    gtest
    Boost::headers
    $<$<TARGET_EXISTS:Boost::throw_exception>:Boost::throw_exception>
    $<$<TARGET_EXISTS:Boost::exception>:Boost::exception>
    ${OPUS_TARGET}
    ${PORTAUDIO_TARGET}
    Threads::Threads
)
