cmake_minimum_required(VERSION 3.10)
project(chat)

set(CMAKE_CXX_STANDARD 17)

add_library(tftp-packet STATIC
    lib/Packet.cpp
    lib/Client.cpp)
target_include_directories(tftp-packet PUBLIC lib/)

# Client
add_executable(tftp-client
    apps/Client.cpp)
target_include_directories(tftp-client PUBLIC lib/)
target_include_directories(tftp-client PUBLIC third_party/)
target_link_libraries(tftp-client PUBLIC tftp-packet)

# Tests
find_package(doctest REQUIRED)
add_executable(tests
    test/Main.cpp
    test/Packet.cpp)
target_include_directories(tests PUBLIC lib/)
target_link_libraries(tests PUBLIC tftp-packet)

include(CTest)
include(doctest)
doctest_discover_tests(tests)
