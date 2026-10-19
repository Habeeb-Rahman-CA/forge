cmake_minimum_required(VERSION 3.16)
project(forge_emulator VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)
endif()

add_library(forge_core STATIC
    src/forge_error.c
    src/forge_decoder.c
    src/forge_memory.c
    src/forge_alu.c
    src/forge_stack.c
    src/forge_io.c
    src/forge_os.c
    src/forge_cpu_state.c
    src/forge_loader.c
    src/forge_exec.c
)

target_include_directories(forge_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(forge-emu
    src/forge_main.c
)
target_link_libraries(forge-emu PRIVATE forge_core)

enable_testing()

add_executable(test_emulator
    tests/test_emulator.c
)
target_link_libraries(test_emulator PRIVATE forge_core)

add_test(NAME forge_emulator_tests COMMAND test_emulator)

