cmake_minimum_required(VERSION 3.18) # Ensure C++20 support

project(ChromeDashboard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add helper libraries
add_library(payload STATIC src/payload.cpp)
target_include_directories(payload PUBLIC include)

add_library(shell_cmd STATIC src/shell_cmd.cpp)
target_include_directories(shell_cmd PUBLIC include)

add_library(collect STATIC src/collect.cpp)
target_include_directories(collect PUBLIC include)
target_link_libraries(collect PRIVATE shell_cmd)

# Add executables for the collector and UI process
add_executable(collector src/collector.cpp)
add_executable(ui        src/ui.cpp)

# Link the shared library to both executables
target_link_libraries(collector PRIVATE payload collect)
target_link_libraries(ui        PRIVATE payload shell_cmd)

# Find and link ncurses for the UI process if needed
find_package(Curses REQUIRED)
target_link_libraries(ui PRIVATE ${CURSES_LIBRARIES})

foreach(target collector ui payload shell_cmd)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -g)
endforeach()
