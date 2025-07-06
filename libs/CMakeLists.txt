add_library(tga INTERFACE
    include/tga.h
)
target_include_directories(tga INTERFACE include)
target_folder(tga "Libraries")
