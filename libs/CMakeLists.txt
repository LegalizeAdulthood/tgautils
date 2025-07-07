add_library(tga STATIC
    include/tga.h
    read.c
    write.c
)
target_include_directories(tga PUBLIC include)
target_folder(tga "Libraries")
