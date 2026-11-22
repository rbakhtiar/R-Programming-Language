
cmake_minimum_required(VERSION 3.20)

project(RPlus
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(rp
    src/main.cpp
)

target_include_directories(rp PRIVATE include)

if(MSVC)
    target_compile_options(rp PRIVATE /W4)
else()
    target_compile_options(rp PRIVATE -Wall -Wextra -Wpedantic)
endif()
