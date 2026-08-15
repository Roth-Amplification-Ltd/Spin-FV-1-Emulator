# Phase 6C Linux product/install-tree smoke test.
if(NOT DEFINED PROJECT_BINARY_DIR OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "PROJECT_BINARY_DIR and TEST_ROOT are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
set(prefix "${TEST_ROOT}/prefix")

set(install_cmd "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix "${prefix}")
if(DEFINED BUILD_CONFIG AND NOT "${BUILD_CONFIG}" STREQUAL "")
    list(APPEND install_cmd --config "${BUILD_CONFIG}")
endif()
execute_process(COMMAND ${install_cmd} RESULT_VARIABLE install_rv)
if(NOT install_rv EQUAL 0)
    message(FATAL_ERROR "cmake --install failed: ${install_rv}")
endif()

foreach(required
        "${prefix}/bin/fv1-cli"
        "${prefix}/bin/fv1-live"
        "${prefix}/include/fv1/sdk.h"
        "${prefix}/include/fv1/sdk_debug.h"
        "${prefix}/include/fv1/sdk.hpp"
        "${prefix}/include/fv1/module.modulemap"
        "${prefix}/share/applications/roth-fv1-emulator.desktop"
        "${prefix}/share/icons/hicolor/512x512/apps/roth-fv1-emulator.png"
        "${prefix}/share/spin-fv1-emulator/splash/FV1LabSplashImagebase.png")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "required installed product/SDK file missing: ${required}")
    endif()
endforeach()

# Private implementation headers are intentionally not part of the installed SDK.
foreach(forbidden
        "${prefix}/include/fv1/fv1.h"
        "${prefix}/include/fv1/runtime.hpp"
        "${prefix}/include/fv1/validation.hpp"
        "${prefix}/include/fv1/spinasm.hpp")
    if(EXISTS "${forbidden}")
        message(FATAL_ERROR "private implementation header leaked into install tree: ${forbidden}")
    endif()
endforeach()

# If the GUI was part of this build, the product install must include it too.
if(EXISTS "${PROJECT_BINARY_DIR}/fv1-lab" AND NOT EXISTS "${prefix}/bin/fv1-lab")
    message(FATAL_ERROR "fv1-lab was built but is missing from product install")
endif()

# Run the installed CLI with the installed shared SDK discoverable. This tests
# the staged product rather than accidentally executing the build-tree binary.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "LD_LIBRARY_PATH=${prefix}/lib:${prefix}/lib64"
            "${prefix}/bin/fv1-cli" --version
    RESULT_VARIABLE cli_rv
    OUTPUT_VARIABLE cli_out
    ERROR_VARIABLE cli_err)
if(NOT cli_rv EQUAL 0)
    message(FATAL_ERROR "installed fv1-cli --version failed (${cli_rv}): ${cli_err}")
endif()
if(NOT cli_out MATCHES "1\\.0\\.0-rc1")
    message(FATAL_ERROR "installed fv1-cli reported unexpected version: ${cli_out}")
endif()

message(STATUS "Phase 6C product install passed: ${prefix}")
