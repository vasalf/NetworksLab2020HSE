cmake_minimum_required(VERSION 3.10)
project(chat)

set(CMAKE_CXX_STANDARD 17)

add_library(chat STATIC
    lib/Message.cpp
    lib/Socket.cpp)
target_include_directories(chat PUBLIC lib/)


# Tests
find_package(doctest REQUIRED)
add_executable(tests
    test/Main.cpp
    test/util/MockSocket.cpp
    test/Message.cpp
    test/Socket.cpp)
target_include_directories(tests PUBLIC lib/)
target_include_directories(tests PUBLIC test/)
target_link_libraries(tests PUBLIC chat)

include(CTest)
include(doctest)
doctest_discover_tests(tests)
