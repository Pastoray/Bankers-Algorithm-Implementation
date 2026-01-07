cmake_minimum_required(VERSION 3.10)
project(os_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(main src/main.c mongoose.c)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(main pthread)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(main PRIVATE -g -O0 -Wall -Wextra)
else()
    target_compile_options(main PRIVATE -O2)
endif()