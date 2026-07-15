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


#GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)


# ImGui
add_library(imgui
    "vendor/imgui/imgui.cpp"
    "vendor/imgui/imgui_draw.cpp"
    "vendor/imgui/imgui_tables.cpp"
    "vendor/imgui/imgui_widgets.cpp"
    "vendor/imgui/imgui_demo.cpp"
    "vendor/imgui/misc/cpp/imgui_stdlib.cpp"
    "vendor/imgui/backends/imgui_impl_glfw.cpp"
    "vendor/imgui/backends/imgui_impl_opengl3.cpp"
    "vendor/ImGuiFileDialog/ImGuiFileDialog.cpp"
)
target_include_directories(imgui
    PUBLIC
    "${CMAKE_SOURCE_DIR}/vendor/imgui"
    "${CMAKE_SOURCE_DIR}/vendor/imgui/backends"
    "${CMAKE_SOURCE_DIR}/vendor/imgui/misc/cpp"
    "${CMAKE_SOURCE_DIR}/vendor/ImGuiFileDialog"
)
find_package(OpenGL REQUIRED)
target_link_libraries(
    imgui
    PUBLIC
    glfw
    OpenGL::GL
)


# Core Library
add_library(
    core
    "src/block/converter.cpp"
    "src/block/definition.cpp"
    "src/block/registry.cpp"
    "src/codegen/backend.cpp"
    "src/codegen/cpp_backend.cpp"
    "src/codegen/py_backend.cpp"
    "src/codegen/frontend.cpp"
)
target_include_directories(
    core
    PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)
set_target_properties(
    core
    PROPERTIES
    UNITY_BUILD ON
)


# PVB
add_executable(
    pvb
    "src/main.cpp"
)
target_link_libraries(
    pvb
    PRIVATE
    core
    ui
)


# Ui
add_library(
    ui
    "src/ui/canvas.cpp"
    "src/ui/block.cpp"
    "src/ui/block_manager.cpp"
    "src/ui/palette.cpp"
    "src/ui/editor.cpp"
    "src/ui/project_ini.cpp"
    "src/ui/app.cpp"
)
target_link_libraries(
    ui
    PUBLIC
    imgui
    core
)
target_include_directories(
    ui
    PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)
set_target_properties(
    ui
    PROPERTIES
    UNITY_BUILD ON
)


# Tests
add_executable(
    test_runner
    "tests/cpp_emitter.cpp"
    "tests/py_emitter.cpp"
    "tests/block_converter.cpp"
    "tests/test_runner.cpp"
    "tests/block_schema.cpp"
)
target_link_libraries(
    test_runner
    PRIVATE
    core
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
target_link_libraries(core PRIVATE project_warnings)
target_link_libraries(ui PRIVATE project_warnings)
target_link_libraries(pvb PRIVATE project_warnings)
target_link_libraries(test_runner PRIVATE project_warnings)
