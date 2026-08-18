# Cross-platform product/install-tree smoke test.
if(NOT DEFINED PROJECT_BINARY_DIR OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "PROJECT_BINARY_DIR and TEST_ROOT are required")
endif()

if(NOT DEFINED EXPECTED_VERSION)
    set(EXPECTED_VERSION "1.0.0-rc1")
endif()

if(NOT DEFINED EXPECT_GUI)
    set(EXPECT_GUI OFF)
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

if(WIN32)
    set(exe ".exe")
else()
    set(exe "")
endif()

foreach(required
        "${prefix}/bin/fv1-cli${exe}"
        "${prefix}/bin/fv1-live${exe}"
        "${prefix}/include/fv1/sdk.h"
        "${prefix}/include/fv1/sdk_debug.h"
        "${prefix}/include/fv1/sdk.hpp"
        "${prefix}/include/fv1/module.modulemap"
        "${prefix}/share/spin-fv1-emulator/icons/fv1-emulator-silver.png"
        "${prefix}/share/spin-fv1-emulator/splash/FV1LabSplashImagebase.png")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "required installed product/SDK file missing: ${required}")
    endif()
endforeach()

if(UNIX AND NOT APPLE)
    foreach(required
            "${prefix}/share/applications/roth-fv1-emulator.desktop"
            "${prefix}/share/icons/hicolor/512x512/apps/roth-fv1-emulator.png")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "required Linux desktop file missing: ${required}")
        endif()
    endforeach()
endif()

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

if(EXPECT_GUI)
    if(WIN32)
        set(installed_gui "${prefix}/bin/FV1Lab.exe")
    else()
        set(installed_gui "${prefix}/bin/fv1-lab")
    endif()
    if(NOT EXISTS "${installed_gui}")
        message(FATAL_ERROR "Qt FV-1 Lab was built but is missing from product install: ${installed_gui}")
    endif()
endif()

set(cli "${prefix}/bin/fv1-cli${exe}")

# On Windows the SDK DLL is installed beside the executable. Unix installs use
# the product RPATH plus this explicit test environment for the staged tree.
if(WIN32)
    execute_process(
        COMMAND "${cli}" --version
        RESULT_VARIABLE cli_rv
        OUTPUT_VARIABLE cli_out
        ERROR_VARIABLE cli_err)
else()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "LD_LIBRARY_PATH=${prefix}/lib:${prefix}/lib64"
                "${cli}" --version
        RESULT_VARIABLE cli_rv
        OUTPUT_VARIABLE cli_out
        ERROR_VARIABLE cli_err)
endif()

if(NOT cli_rv EQUAL 0)
    message(FATAL_ERROR "installed fv1-cli --version failed (${cli_rv}): ${cli_err}")
endif()

string(FIND "${cli_out}" "${EXPECTED_VERSION}" version_index)
if(version_index EQUAL -1)
    message(FATAL_ERROR
        "installed fv1-cli reported unexpected version: ${cli_out}; expected ${EXPECTED_VERSION}")
endif()

message(STATUS "Cross-platform product install passed: ${prefix}")
