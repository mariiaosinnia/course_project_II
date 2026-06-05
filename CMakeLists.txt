cmake_minimum_required(VERSION 3.8)
project("course_project_II")

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Знаходимо Boost
find_package(Boost CONFIG REQUIRED COMPONENTS system)

if(Boost_FOUND)
    message(STATUS "Boost знайдено: ${Boost_VERSION}")
    include_directories(${Boost_INCLUDE_DIRS})
endif()

add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(shared)