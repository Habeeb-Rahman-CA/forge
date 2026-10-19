cmake_minimum_required(VERSION 3.16)
project(forge_compiler VERSION 0.4.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)
endif()

add_library(forge_compiler_lib STATIC
    src/forge_lexer.c
    src/forge_parser.c
    src/forge_sema.c
    src/forge_codegen.c
)

target_include_directories(forge_compiler_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(forge_compiler_lib PUBLIC
    forge_asm_lib
    forge_core
)

add_executable(forge-cc
    src/forge_compiler_main.c
)
target_link_libraries(forge-cc PRIVATE forge_compiler_lib)

enable_testing()

add_executable(test_compiler
    tests/test_compiler.c
)
target_compile_definitions(test_compiler PRIVATE
    FORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(test_compiler PRIVATE forge_bridge_lib forge_compiler_lib)

add_test(NAME forge_compiler_tests COMMAND test_compiler)


