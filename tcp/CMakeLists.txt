cmake_minimum_required(VERSION 3.10)
project(chat)

set(CMAKE_CXX_STANDARD 17)

# Main code
add_library(chat STATIC
    lib/Message.cpp
    lib/Socket.cpp
    lib/Client.cpp
    lib/Server.cpp)
target_include_directories(chat PUBLIC lib/)
target_link_libraries(chat pthread)

# Server app
add_executable(server apps/Server.cpp)
target_include_directories(server PUBLIC lib/)
target_include_directories(server PUBLIC third_party/)
target_link_libraries(server PUBLIC chat)

# Client app
add_executable(client apps/Client.cpp)
target_include_directories(client PUBLIC lib/)
target_include_directories(client PUBLIC third_party/)
target_link_libraries(client PUBLIC chat)

# Tests
find_package(doctest REQUIRED)
add_executable(tests
    test/Main.cpp
    test/util/MockSocket.cpp
    test/Message.cpp
    test/Socket.cpp
    test/Client.cpp)
target_include_directories(tests PUBLIC lib/)
target_include_directories(tests PUBLIC test/)
target_link_libraries(tests PUBLIC chat)

include(CTest)
include(doctest)
doctest_discover_tests(tests)
