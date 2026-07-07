cmake_minimum_required(VERSION 3.20)
project(pvb VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Caching
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Core Library
add_library(
    pvb_lib
    "src/block/block.cpp"
    "src/codegen/backend.cpp"
    "src/codegen/cpp_backend.cpp"
    "src/codegen/py_backend.cpp"
    "src/codegen/frontend.cpp"
)
target_include_directories(
    pvb_lib
    PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)
set_target_properties(
    pvb_lib
    PROPERTIES
    UNITY_BUILD ON
)


# App
add_executable(
    pvb
    src/main.cpp
)
target_link_libraries(
    pvb
    PRIVATE
    pvb_lib
)


# Tests
add_executable(
    test_runner
    "tests/cpp_emitter.cpp"
    "tests/py_emitter.cpp"
    "tests/test_runner.cpp"
)
target_link_libraries(
    test_runner
    PRIVATE
    pvb_lib
)


# Useful warnings
add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
target_link_libraries(pvb_lib PRIVATE project_warnings)
target_link_libraries(pvb PRIVATE project_warnings)
target_link_libraries(test_runner PRIVATE project_warnings)
