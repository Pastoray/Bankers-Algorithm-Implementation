cmake_minimum_required(VERSION 3.10)
project(os_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(EMBEDDED_UI_H "${CMAKE_CURRENT_BINARY_DIR}/embedded_ui.h")

add_custom_command(
        OUTPUT ${EMBEDDED_UI_H}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/embed.sh ${EMBEDDED_UI_H}
        DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/embed.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.html
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating embedded_ui.h"
)

add_compile_definitions(EMBED_UI=1)
add_executable(main src/main.c mongoose.c ${EMBEDDED_UI_H})

target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(main pthread)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(main PRIVATE -g -O0 -Wall -Wextra)
else()
    target_compile_options(main PRIVATE -O2)
endif()