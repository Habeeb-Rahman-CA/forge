cmake_minimum_required(VERSION 3.16)
project(forge VERSION 0.4.0 LANGUAGES C)

enable_testing()
add_subdirectory(emulator)
add_subdirectory(assembler)
add_subdirectory(compiler)

