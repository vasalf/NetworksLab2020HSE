cmake_minimum_required(VERSION 3.10)
project(proxy)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.71 REQUIRED COMPONENTS iostreams)

add_library(proxy STATIC
    lib/Server.cpp
    lib/Session.cpp
    lib/HTTP.cpp
    lib/Database.cpp
    lib/Compress.cpp)
target_include_directories(proxy PUBLIC lib/)
target_include_directories(proxy PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(proxy PUBLIC pthread Boost::iostreams)

add_executable(http_proxy
    app/Server.cpp)
target_include_directories(http_proxy PUBLIC lib/)
target_include_directories(http_proxy PUBLIC third_party/)
target_link_libraries(http_proxy PUBLIC proxy)
