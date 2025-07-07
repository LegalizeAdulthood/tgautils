message(STATUS "CMAKE_COMMAND=${CMAKE_COMMAND}")
message(STATUS "WORKING_DIR=${WORKING_DIR}")
message(STATUS "TGADUMP=${TGADUMP}")
message(STATUS "IMAGE=${IMAGE}")
message(STATUS "OUTPUT=${OUTPUT}")
message(STATUS "GOLD_OUTPUT=${GOLD_OUTPUT}")

function(cat file)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "cat" "${file}")
endfunction()

execute_process(COMMAND "${TGADUMP}" "${IMAGE}" OUTPUT_FILE "${OUTPUT}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "Failed to execute tgadump on ${IMAGE}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${OUTPUT}" "${GOLD_OUTPUT}"
    RESULT_VARIABLE result)
if(result)
    message(STATUS "Actual output: ${OUTPUT}")
    cat(${OUTPUT})
    message(STATUS "Expected output: ${GOLD_OUTPUT}")
    cat(${GOLD_OUTPUT})
    message(FATAL_ERROR "Output file ${OUTPUT} does not match gold file ${GOLD_OUTPUT}")
endif()
