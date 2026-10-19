cmake_minimum_required(VERSION 3.16)
project(forge_assembler VERSION 0.5.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)
endif()

add_library(forge_asm_lib STATIC
    src/forge_asm_lexer.c
    src/forge_asm_symtab.c
    src/forge_asm_core.c
    src/forge_disasm.c
)

target_include_directories(forge_asm_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(forge_asm_lib PUBLIC forge_core)

add_library(forge_bridge_lib STATIC
    src/forge_bridge.c
)

target_include_directories(forge_bridge_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/compiler/include
)

target_link_libraries(forge_bridge_lib PUBLIC
    forge_compiler_lib
    forge_asm_lib
    forge_core
)

add_executable(forge-asm
    src/forge_asm_main.c
)
target_link_libraries(forge-asm PRIVATE forge_asm_lib)

add_executable(forge-bridge
    src/forge_bridge_main.c
)
target_link_libraries(forge-bridge PRIVATE forge_bridge_lib)

enable_testing()

add_executable(test_assembler
    tests/test_assembler.c
)
target_compile_definitions(test_assembler PRIVATE
    FORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(test_assembler PRIVATE forge_bridge_lib)

add_test(NAME forge_assembler_tests COMMAND test_assembler)
