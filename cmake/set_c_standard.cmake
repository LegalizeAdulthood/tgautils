function(set_c_standard version)
    set(CMAKE_C_STANDARD ${version} PARENT_SCOPE)
    set(CMAKE_C_STANDARD_REQUIRED ON PARENT_SCOPE)
endfunction()
