foreach(image cbw8 ccm8 ctc16 ctc24 ctc32 ubw8 ucm8 utc16 utc24 utc32)
    add_test(NAME image-${image}
        COMMAND ${CMAKE_COMMAND}
            -D "CMAKE_COMMAND=${CMAKE_COMMAND}"
            -D "WORKING_DIR=${CMAKE_CURRENT_LIST_DIR}"
            -D "TGADUMP=$<TARGET_FILE:tgadump>"
            -D "IMAGE=${image}.tga"
            -D "OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${image}.txt"
            -D "GOLD_OUTPUT=${CMAKE_CURRENT_LIST_DIR}/${image}.txt"
            -P "${CMAKE_CURRENT_LIST_DIR}/CompareDumpOutput.cmake")
endforeach()
