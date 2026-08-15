if(NOT DEFINED SDK_LIBRARY OR NOT DEFINED NM_TOOL)
    message(FATAL_ERROR "SDK export check missing SDK_LIBRARY or NM_TOOL")
endif()
execute_process(
    COMMAND "${NM_TOOL}" -D --defined-only "${SDK_LIBRARY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "nm failed: ${error}")
endif()
string(REPLACE "\n" ";" lines "${symbols}")
foreach(line IN LISTS lines)
    if(line STREQUAL "")
        continue()
    endif()
    string(REGEX MATCH "[^ ]+$" symbol "${line}")
    string(REGEX REPLACE "@@.*$" "" symbol "${symbol}")
    if(NOT symbol MATCHES "^fv1_sdk_" AND NOT symbol STREQUAL "FV1SDK_1.0")
        message(FATAL_ERROR "unexpected exported SDK symbol: ${symbol}\n${symbols}")
    endif()
endforeach()
message(STATUS "SDK shared-library export surface is C-ABI-only")
