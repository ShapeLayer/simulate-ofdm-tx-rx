cmake_minimum_required(VERSION 3.10)
project(simulate_ofdm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(simulate-ofdm
  src/main.c
  src/ofdm.c
  src/math_utils.c
)

target_include_directories(simulate-ofdm PRIVATE src)

target_compile_options(simulate-ofdm PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(simulate-ofdm PRIVATE m)
