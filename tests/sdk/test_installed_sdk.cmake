if(NOT DEFINED PROJECT_BINARY_DIR OR NOT DEFINED SDK_HOST_SOURCE_DIR OR NOT DEFINED TEST_ROOT OR NOT DEFINED SDK_BUILD_SHARED)
    message(FATAL_ERROR "installed SDK test missing required variables")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
set(prefix "${TEST_ROOT}/prefix")
set(host_build "${TEST_ROOT}/host-build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix "${prefix}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "SDK staging install failed:\n${install_stdout}\n${install_stderr}")
endif()

set(configure_command "${CMAKE_COMMAND}" -S "${SDK_HOST_SOURCE_DIR}" -B "${host_build}"
    "-DCMAKE_PREFIX_PATH=${prefix}")
if(NOT SDK_BUILD_SHARED)
    list(APPEND configure_command -DFV1SDK_HOST_STATIC=ON)
endif()
# An instrumented SDK requires the external smoke host to link the matching
# sanitizer runtime. Normal builds pass empty flags, preserving the genuine
# clean C-consumer test.
if(DEFINED SDK_C_FLAGS AND NOT SDK_C_FLAGS STREQUAL "")
    list(APPEND configure_command "-DCMAKE_C_FLAGS=${SDK_C_FLAGS}")
endif()
if(DEFINED SDK_CXX_FLAGS AND NOT SDK_CXX_FLAGS STREQUAL "")
    list(APPEND configure_command "-DCMAKE_CXX_FLAGS=${SDK_CXX_FLAGS}")
endif()
if(DEFINED CMAKE_GENERATOR_NAME AND NOT CMAKE_GENERATOR_NAME STREQUAL "")
    list(APPEND configure_command -G "${CMAKE_GENERATOR_NAME}")
endif()
execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "external SDK consumer configure failed:\n${configure_stdout}\n${configure_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${host_build}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "external SDK consumer build failed:\n${build_stdout}\n${build_stderr}")
endif()

set(host "${host_build}/fv1-sdk-host")
if(WIN32)
    set(host "${host_build}/fv1-sdk-host.exe")
    if(NOT EXISTS "${host}")
        foreach(config Debug Release RelWithDebInfo MinSizeRel)
            if(EXISTS "${host_build}/${config}/fv1-sdk-host.exe")
                set(host "${host_build}/${config}/fv1-sdk-host.exe")
                break()
            endif()
        endforeach()
    endif()
endif()
execute_process(
    COMMAND "${host}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "external SDK consumer run failed:\n${run_stdout}\n${run_stderr}")
endif()
message(STATUS "installed external SDK host: ${run_stdout}")
