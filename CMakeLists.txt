cmake_minimum_required(VERSION 3.15)
project("course_project_II")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
endif()

add_subdirectory(server)
