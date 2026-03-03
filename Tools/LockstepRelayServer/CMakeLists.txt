cmake_minimum_required(VERSION 3.16)
project(LockstepRelayServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(LockstepRelayServer
    src/main.cpp
)

if(WIN32)
    target_link_libraries(LockstepRelayServer PRIVATE ws2_32)
endif()
