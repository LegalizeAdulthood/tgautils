# The truevision package is optional and provides headers and libraries
# for the truevision development environment.
find_package(truevision QUIET)

function(add_tool name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PUBLIC tga config)
    target_folder(${name} "Tools")
endfunction()

function(add_truevision_tool name)
    if(truevision_FOUND)
        add_executable(${name} ${name}.c)
        target_link_libraries(${name} PUBLIC tga truevision)
        target_folder(${name} "Tools")
    endif()
endfunction()

foreach(tool tgadump tgaedit tgapack)
    add_tool(${tool})
endforeach()
foreach(tool tstamp vstamp)
    add_truevision_tool(${tool})
endforeach()
