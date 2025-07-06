include(CheckIncludeFile)
include(CheckSymbolExists)

check_include_file(string.h I_STRING)
check_include_file(strings.h I_STRINGS)

# String case-insensitive compare functions
if(I_STRINGS)
    check_symbol_exists(strcasecmp "strings.h" HAS_STRCASECMP)
    if(HAS_STRCASECMP)
        set(STRING_CASE_COMPARE_FLAVOR "strings")
    endif()
endif()
if(I_STRING)
    check_symbol_exists(stricmp "string.h" HAS_STRICMP)
    if(HAS_STRICMP)
        set(STRING_CASE_COMPARE_FLAVOR "string")
    endif()
endif()
if(NOT STRING_CASE_COMPARE_FLAVOR)
    set(STRING_CASE_COMPARE_FLAVOR "manual")
endif()

configure_file(
    "string_case_compare.${STRING_CASE_COMPARE_FLAVOR}.c.in"
    "string_case_compare.c"
    COPYONLY)

add_library(config STATIC
    include/config/string_case_compare.h
    ${CMAKE_CURRENT_BINARY_DIR}/string_case_compare.c
)
target_include_directories(config PUBLIC "include")
target_folder(config "Libraries")
