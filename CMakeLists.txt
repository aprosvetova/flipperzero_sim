cmake_minimum_required(VERSION 3.16)
project(sim VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)

if (MSVC)
    # warning level 4
    add_compile_options(/W4 /WX)
else()
    # warnings as errors
    add_compile_options(-Wall -Werror)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

add_executable(snake snake.c flipper.c flipper.h)
target_link_libraries(snake PRIVATE SDL2::Main SDL2::Image)

add_executable(tetris tetris.c flipper.c flipper.h tetris_pieces.h img/micro4x6.xbm)
target_link_libraries(tetris PRIVATE SDL2::Main SDL2::Image)

file(COPY img DESTINATION .)