cmake_minimum_required(VERSION 3.20)

project(
    pvb
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(
    GLOB_RECURSE
    PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
add_executable(
    ${PROJECT_NAME}
    ${PROJECT_SOURCES}
)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/"
)

# Tests
file(
    GLOB_RECURSE
    TEST_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
)

set(TEST_PROJECT_SOURCES ${PROJECT_SOURCES})
list(FILTER TEST_PROJECT_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_executable(
    test_runner
    ${TEST_PROJECT_SOURCES}
    ${TEST_SOURCES}
)

target_include_directories(
    test_runner
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/"
)

add_test(
    NAME test_runner
    COMMAND test_runner
)

# Useful warnings
if(MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        /W4
    )
    target_compile_options(
        test_runner
        PRIVATE
        /W4
    )
else()
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    target_compile_options(
        test_runner
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
