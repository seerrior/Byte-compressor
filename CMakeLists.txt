cmake_minimum_required(VERSION 3.10)
project(bytec C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(bytec
    src/bytec_archive.c
    src/bytec_crc.c
    src/bytec_format.c
    src/bytec_io.c
    src/bytec_lz.c
    src/bytec_pack.c
    src/bytec_read.c
    src/bytec_source.c
)

target_include_directories(bytec
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NOT WIN32)
    target_compile_definitions(bytec PRIVATE _FILE_OFFSET_BITS=64)
endif()

add_executable(bytec_cli tools/bytec_cli.c)
target_include_directories(bytec_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(bytec_cli PRIVATE bytec)

add_executable(bytec_example examples/runtime_usage.c)
target_link_libraries(bytec_example PRIVATE bytec)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/BytecPack.cmake)

option(BYTEC_BUILD_TESTS "Build bytec tests" ON)
if(BYTEC_BUILD_TESTS)
    enable_testing()
    add_executable(bytec_test tests/test_roundtrip.c)
    target_include_directories(bytec_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(bytec_test PRIVATE bytec)
    add_test(NAME roundtrip COMMAND bytec_test)
endif()
