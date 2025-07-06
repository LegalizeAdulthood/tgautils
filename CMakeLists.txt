cmake_minimum_required(VERSION 3.23...3.31)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(misc_target)
include(set_c_standard)
include(target_folder)
include(vs_startup_project)

set_c_standard(99)

set(TGAUTILS_PROJECT_NAME "TGA Utilities" CACHE "STRING" "Default project name for IDEs")
project("${TGAUTILS_PROJECT_NAME}" C)

add_subdirectory(config)
add_subdirectory(libs)
add_subdirectory(tools)

vs_startup_project(tgadump)

misc_target(FILES
    "CMakePresets.json"
    "ReadMe.md")
misc_group_sources("CMake Scripts" FILES
    "cmake/misc_target.cmake"
    "cmake/set_c_standard.cmake"
    "cmake/target_folder.cmake"
    "cmake/vs_startup_project.cmake")
misc_group_sources("Editing" FILES
    ".clang-format"
    ".editorconfig")
misc_group_sources("Git" FILES
    ".gitignore")
misc_group_sources("GitHub" FILES
    "LICENSE.txt"
    ".github/workflows/cmake.yml")
