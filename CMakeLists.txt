cmake_minimum_required(VERSION 3.20)
project(SpinFV1Emulator VERSION 1.0.0 LANGUAGES C CXX)

set(FV1_RELEASE_CHANNEL "rc1" CACHE STRING "Release channel suffix (for example rc1; empty for final)")
if(FV1_RELEASE_CHANNEL STREQUAL "")
    set(FV1_RELEASE_STRING "${PROJECT_VERSION}")
else()
    set(FV1_RELEASE_STRING "${PROJECT_VERSION}-${FV1_RELEASE_CHANNEL}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(FV1_BUILD_TESTS "Build emulator tests" ON)
option(FV1_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(FV1_ENABLE_LIVE_AUDIO "Build Linux realtime audio host when miniaudio is available" ON)
option(FV1_BUILD_GUI "Build the Qt FV-1 emulator/testbench application" ON)
if(WIN32)
    set(FV1_WINDOWS_FRONTEND_DEFAULT ON)
else()
    set(FV1_WINDOWS_FRONTEND_DEFAULT OFF)
endif()
option(FV1_BUILD_WINDOWS_FRONTEND "Build the native Win32 FV-1 Lab frontend on Windows" ${FV1_WINDOWS_FRONTEND_DEFAULT})
unset(FV1_WINDOWS_FRONTEND_DEFAULT)
option(FV1_BUILD_FUZZERS "Build Clang/libFuzzer hardening targets" OFF)
option(FV1_SDK_BUILD_SHARED "Build the embeddable FV-1 SDK as a shared library" ON)
option(FV1_SDK_ONLY "Build/install only the embeddable FV-1 SDK and SDK-facing tests" OFF)

if(FV1_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()
if(NOT FV1_SDK_ONLY)
    find_package(Threads REQUIRED)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SPEEXDSP QUIET IMPORTED_TARGET speexdsp)
    endif()
    find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")
endif()

function(fv1_strict_warnings target)
    if(NOT FV1_ENABLE_WARNINGS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Phase 1: chip emulator
# -----------------------------------------------------------------------------
add_library(fv1-core STATIC src/core/engine.cpp)
add_library(FV1::core ALIAS fv1-core)
target_include_directories(fv1-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
fv1_strict_warnings(fv1-core)
set_target_properties(fv1-core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME _core_internal
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

# -----------------------------------------------------------------------------
# Phase 6A: native SpinASM compiler + embeddable C ABI SDK.
#
# The Qt application, CLI and realtime host consume the native compiler directly
# so installed builds no longer shell out to Python.  The public SDK is a small,
# versioned C ABI over opaque handles; internal C++ implementation remains free
# to evolve until ABI v1 is explicitly ratified after the Phase-6C gate.
# -----------------------------------------------------------------------------
add_library(fv1-spinasm STATIC src/spinasm/spinasm.cpp)
add_library(FV1::spinasm ALIAS fv1-spinasm)
target_include_directories(fv1-spinasm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(fv1-spinasm PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME _spinasm_internal
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
fv1_strict_warnings(fv1-spinasm)

set(FV1_SDK_SOURCES
    src/sdk/sdk.cpp
    src/core/engine.cpp
    src/spinasm/spinasm.cpp)
if(FV1_SDK_BUILD_SHARED)
    add_library(fv1-sdk SHARED ${FV1_SDK_SOURCES})
else()
    add_library(fv1-sdk STATIC ${FV1_SDK_SOURCES})
endif()
add_library(FV1::sdk ALIAS fv1-sdk)
target_include_directories(fv1-sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(fv1-sdk PRIVATE
    FV1_SDK_BUILDING=1
    FV1_SDK_VERSION_STRING="${FV1_RELEASE_STRING}"
    FV1_SDK_VERSION_MAJOR_VALUE=${PROJECT_VERSION_MAJOR}
    FV1_SDK_VERSION_MINOR_VALUE=${PROJECT_VERSION_MINOR}
    FV1_SDK_VERSION_PATCH_VALUE=${PROJECT_VERSION_PATCH}
)
if(FV1_SDK_BUILD_SHARED)
    target_compile_definitions(fv1-sdk PUBLIC FV1_SDK_SHARED=1)
    if(UNIX AND NOT APPLE)
        target_link_options(fv1-sdk PRIVATE
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/fv1-sdk.map")
    elseif(APPLE)
        # Mach-O has no GNU version script. Keep the same narrow public C ABI
        # with an explicit exported-symbol list so C++ internals stay private.
        target_link_options(fv1-sdk PRIVATE
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/cmake/fv1-sdk.exports")
    endif()
endif()
set_target_properties(fv1-sdk PROPERTIES
    EXPORT_NAME sdk
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
fv1_strict_warnings(fv1-sdk)

# SDK installation is deliberately narrow: installed consumers receive only
# FV1SDK::sdk plus the C ABI, optional debugger C header, C++ convenience
# wrapper, and Clang module map. Application-internal headers/libraries are not
# part of the SDK package contract.
function(fv1_install_sdk)
    install(TARGETS fv1-sdk
            EXPORT FV1SDKTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/include/fv1/sdk.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/fv1/sdk_debug.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/fv1/sdk.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/fv1/module.modulemap
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fv1)
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FV1SDKConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/FV1SDKConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FV1SDK)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/FV1SDKConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(EXPORT FV1SDKTargets
            FILE FV1SDKTargets.cmake
            NAMESPACE FV1SDK::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FV1SDK)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/FV1SDKConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/FV1SDKConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FV1SDK)
endfunction()

function(fv1_add_sdk_tests)
    add_executable(fv1-sdk-c-api-tests tests/sdk/test_sdk_c.c)
    target_link_libraries(fv1-sdk-c-api-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-c-api-tests)
    add_test(NAME fv1-sdk-c-api-tests COMMAND fv1-sdk-c-api-tests)

    add_executable(fv1-sdk-debug-api-tests tests/sdk/test_sdk_debug.c)
    target_link_libraries(fv1-sdk-debug-api-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-debug-api-tests)
    add_test(NAME fv1-sdk-debug-api-tests COMMAND fv1-sdk-debug-api-tests)

    add_executable(fv1-sdk-cpp-wrapper-tests tests/sdk/test_sdk_cpp.cpp)
    target_link_libraries(fv1-sdk-cpp-wrapper-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-cpp-wrapper-tests)
    add_test(NAME fv1-sdk-cpp-wrapper-tests COMMAND fv1-sdk-cpp-wrapper-tests)

    add_executable(fv1-sdk-abi-layout-tests tests/sdk/test_sdk_abi_layout.c)
    target_link_libraries(fv1-sdk-abi-layout-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-abi-layout-tests)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(fv1-sdk-abi-layout-tests PRIVATE -fshort-enums)
    endif()
    add_test(NAME fv1-sdk-abi-layout-tests COMMAND fv1-sdk-abi-layout-tests)

    # Compile an external consumer against the exact Phase-6B/0.9.0 public
    # headers and link it to the current SDK. This is a practical ABI
    # compatibility tripwire before ABI v1 is ratified.
    add_executable(fv1-sdk-abi-backcompat-0_9-tests tests/sdk/test_sdk_c.c)
    target_include_directories(fv1-sdk-abi-backcompat-0_9-tests BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk/abi-baseline/0.9.0)
    target_link_libraries(fv1-sdk-abi-backcompat-0_9-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-abi-backcompat-0_9-tests)
    add_test(NAME fv1-sdk-abi-backcompat-0_9-tests COMMAND fv1-sdk-abi-backcompat-0_9-tests)

    add_executable(fv1-sdk-abuse-tests tests/sdk/test_sdk_abuse.c)
    target_link_libraries(fv1-sdk-abuse-tests PRIVATE FV1::sdk)
    fv1_strict_warnings(fv1-sdk-abuse-tests)
    add_test(NAME fv1-sdk-abuse-tests COMMAND fv1-sdk-abuse-tests)

    if(FV1_SDK_BUILD_SHARED AND UNIX AND NOT APPLE AND CMAKE_NM)
        add_test(NAME sdk-export-surface
            COMMAND ${CMAKE_COMMAND}
                -DSDK_LIBRARY=$<TARGET_FILE:fv1-sdk>
                -DNM_TOOL=${CMAKE_NM}
                -DEXPECTED_SYMBOLS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/fv1-sdk-symbols.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk/check_exports.cmake)
    endif()

    add_test(NAME installed-sdk-consumer
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DPROJECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DSDK_HOST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/examples/sdk-host
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/installed-sdk-consumer
            -DSDK_BUILD_SHARED=${FV1_SDK_BUILD_SHARED}
            "-DSDK_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DSDK_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
            -DCMAKE_GENERATOR_NAME=${CMAKE_GENERATOR}
            -DSDK_BUILD_CONFIG=$<CONFIG>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk/test_installed_sdk.cmake)
    set_tests_properties(installed-sdk-consumer PROPERTIES TIMEOUT 90)

    # Foreign-language runtimes (Python/Swift) are not themselves linked with
    # our sanitizer runtime. Loading an ASan-instrumented shared SDK into those
    # processes is therefore not a meaningful SDK test and can fail before
    # main() due to runtime-order rules. Normal shared builds run the full
    # cross-language proving suite; sanitizer builds retain C/C++ installed
    # consumer coverage through installed-sdk-consumer.
    set(_fv1_sdk_sanitized FALSE)
    if(CMAKE_C_FLAGS MATCHES "-fsanitize" OR CMAKE_CXX_FLAGS MATCHES "-fsanitize")
        set(_fv1_sdk_sanitized TRUE)
    endif()
    if(FV1_SDK_BUILD_SHARED AND NOT _fv1_sdk_sanitized)
        add_test(NAME sdk-cross-language-consumers
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk/test_cross_language_sdk.py
                --project-binary ${CMAKE_CURRENT_BINARY_DIR}
                --source ${CMAKE_CURRENT_SOURCE_DIR}
                --test-root ${CMAKE_CURRENT_BINARY_DIR}/sdk-cross-language
                --config $<CONFIG>)
        set_tests_properties(sdk-cross-language-consumers PROPERTIES TIMEOUT 180)
    endif()
endfunction()

# A native Windows/macOS host should be able to configure this project without
# discovering Qt, miniaudio, SpeexDSP, debugger, validation, or Linux product
# dependencies. This is the portability proving mode used by Phase 6B CI.
if(FV1_SDK_ONLY)
    # fv1-core/fv1-spinasm remain available as source-level implementation
    # targets for this repository but are not built by the SDK-only default
    # target and are never installed/exported.
    set_target_properties(fv1-core fv1-spinasm PROPERTIES EXCLUDE_FROM_ALL TRUE)
    if(FV1_BUILD_TESTS)
        enable_testing()
        fv1_add_sdk_tests()
    endif()
    fv1_install_sdk()
    return()
endif()

# -----------------------------------------------------------------------------
# Phase 7: native Windows frontend SDK boundary.  The session wrapper is kept
# platform-neutral so Linux CI can continuously prove it uses only the public
# SDK.  The actual Win32 shell is added later under WIN32.
# -----------------------------------------------------------------------------
add_library(fv1-windows-frontend-session STATIC
    src/windows/fv1_session.cpp
    src/windows/realtime_processor.cpp)
target_include_directories(fv1-windows-frontend-session PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/windows)
target_link_libraries(fv1-windows-frontend-session PUBLIC FV1::sdk)
fv1_strict_warnings(fv1-windows-frontend-session)

# -----------------------------------------------------------------------------
# Phase 5C: independent golden/reference model.  This target intentionally does
# not link FV1::core so differential tests do not execute the production model
# twice through shared implementation code.
# -----------------------------------------------------------------------------
add_library(fv1-reference STATIC src/reference/reference_model.cpp)
add_library(FV1::reference ALIAS fv1-reference)
target_include_directories(fv1-reference PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
fv1_strict_warnings(fv1-reference)

add_library(fv1-conformance STATIC src/conformance/conformance.cpp)
add_library(FV1::conformance ALIAS fv1-conformance)
target_include_directories(fv1-conformance PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-conformance PUBLIC FV1::core FV1::reference)
fv1_strict_warnings(fv1-conformance)

# -----------------------------------------------------------------------------
# Phase 2: host/FV-1 clock bridge and interchangeable audio sources
# -----------------------------------------------------------------------------
add_library(fv1-runtime STATIC
    src/runtime/runtime.cpp
    src/runtime/audio_source.cpp)
add_library(FV1::runtime ALIAS fv1-runtime)
target_include_directories(fv1-runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-runtime PRIVATE FV1::sdk)
if(TARGET PkgConfig::SPEEXDSP)
    target_link_libraries(fv1-runtime PRIVATE PkgConfig::SPEEXDSP)
    target_compile_definitions(fv1-runtime PRIVATE FV1_HAVE_SPEEXDSP=1)
    message(STATUS "Phase 2 SRC: SpeexDSP enabled")
else()
    message(WARNING "SpeexDSP not found: building the deterministic linear SRC fallback. Run bootstrap-dev.sh on Linux for production realtime SRC.")
endif()
fv1_strict_warnings(fv1-runtime)

# -----------------------------------------------------------------------------
# Reusable offline instruction debugger / chip inspector
# -----------------------------------------------------------------------------
add_library(fv1-debugger STATIC src/debug/debugger.cpp)
add_library(FV1::debugger ALIAS fv1-debugger)
target_include_directories(fv1-debugger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-debugger PUBLIC FV1::core)
fv1_strict_warnings(fv1-debugger)

add_library(fv1-validation STATIC src/validation/validation.cpp)
add_library(FV1::validation ALIAS fv1-validation)
target_include_directories(fv1-validation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-validation PUBLIC FV1::runtime)
fv1_strict_warnings(fv1-validation)

add_library(fv1-analysis STATIC src/analysis/analysis.cpp)
add_library(FV1::analysis ALIAS fv1-analysis)
target_include_directories(fv1-analysis PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-analysis PUBLIC FV1::runtime Threads::Threads)
fv1_strict_warnings(fv1-analysis)

add_library(fv1-audio STATIC
    src/audio/audio_host.cpp
    src/audio/audio_recorder.cpp
    src/audio/miniaudio_impl.c)
add_library(FV1::audio ALIAS fv1-audio)
target_include_directories(fv1-audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-audio PUBLIC FV1::runtime FV1::analysis Threads::Threads ${CMAKE_DL_LIBS})
if(FV1_ENABLE_LIVE_AUDIO AND MINIAUDIO_INCLUDE_DIR)
    target_include_directories(fv1-audio PRIVATE ${MINIAUDIO_INCLUDE_DIR})
    target_compile_definitions(fv1-audio PRIVATE FV1_HAVE_MINIAUDIO=1)
    if(MSVC)
        set_source_files_properties(src/audio/miniaudio_impl.c PROPERTIES COMPILE_OPTIONS "/w")
    else()
        set_source_files_properties(src/audio/miniaudio_impl.c PROPERTIES COMPILE_OPTIONS "-w")
    endif()
    message(STATUS "Phase 2 audio: miniaudio enabled from ${MINIAUDIO_INCLUDE_DIR}")
else()
    message(WARNING "miniaudio.h not found: fv1-live will build in no-device stub mode. Run bootstrap-dev.sh to fetch the pinned development header.")
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(fv1-audio PRIVATE m)
endif()
fv1_strict_warnings(fv1-audio)

# -----------------------------------------------------------------------------
# Command-line tools
# -----------------------------------------------------------------------------
add_executable(fv1-cli src/cli/main.cpp)
target_compile_definitions(fv1-cli PRIVATE FV1_PRODUCT_VERSION_STRING=\"${FV1_RELEASE_STRING}\")
target_link_libraries(fv1-cli PRIVATE FV1::core FV1::reference FV1::conformance FV1::runtime FV1::validation FV1::spinasm)
fv1_strict_warnings(fv1-cli)

add_executable(fv1-live src/live/main.cpp)
target_compile_definitions(fv1-live PRIVATE FV1_PRODUCT_VERSION_STRING=\"${FV1_RELEASE_STRING}\")
target_link_libraries(fv1-live PRIVATE FV1::audio FV1::analysis FV1::runtime FV1::core FV1::spinasm)
fv1_strict_warnings(fv1-live)

# -----------------------------------------------------------------------------
# Phase 7: complete native Windows product frontend.  This target deliberately links
# only the public SDK wrapper plus Windows system libraries; it does not reuse
# Qt, the Linux runtime/audio stack, or private emulator libraries.
# -----------------------------------------------------------------------------
if(WIN32 AND FV1_BUILD_WINDOWS_FRONTEND)
    configure_file(
        packaging/windows/fv1-lab-win32.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/fv1-lab-win32.rc
        @ONLY)
    add_executable(fv1-lab-win32 WIN32
        src/windows/main.cpp
        src/windows/wasapi_probe.cpp
        src/windows/wasapi_engine.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/fv1-lab-win32.rc)
    target_include_directories(fv1-lab-win32 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/windows
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows)
    target_compile_definitions(fv1-lab-win32 PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        FV1_PRODUCT_VERSION_STRING=\"${FV1_RELEASE_STRING}\")
    target_link_libraries(fv1-lab-win32 PRIVATE
        fv1-windows-frontend-session
        FV1::sdk
        comctl32
        comdlg32
        ole32
        uuid
        avrt)
    fv1_strict_warnings(fv1-lab-win32)
    message(STATUS "Phase 7 native Windows frontend enabled")
endif()

# -----------------------------------------------------------------------------
# Qt FV-1 emulator/testbench application (Linux product frontend)
# -----------------------------------------------------------------------------
if(FV1_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        add_executable(fv1-lab
            src/gui/main.cpp
            src/gui/main_window.cpp
            src/gui/startup_splash.cpp
            src/gui/theme_manager.cpp
            src/gui/instrument_plot.cpp
            src/gui/validation_panel.cpp)
        target_link_libraries(fv1-lab PRIVATE
            Qt6::Widgets FV1::audio FV1::analysis FV1::validation FV1::runtime FV1::debugger FV1::core FV1::spinasm)
        target_include_directories(fv1-lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_compile_definitions(fv1-lab PRIVATE FV1_PRODUCT_VERSION_STRING=\"${FV1_RELEASE_STRING}\")
        fv1_strict_warnings(fv1-lab)
        message(STATUS "Phase 5B hardware-validation/testbench GUI: Qt ${Qt6_VERSION} enabled")
    else()
        message(WARNING "Qt 6 Widgets not found: fv1-lab GUI will not be built. Run bootstrap-dev.sh on Linux to install qt6-base-dev.")
    endif()
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(FV1_BUILD_TESTS)
    enable_testing()

    add_executable(fv1-core-tests tests/test_core.cpp)
    target_link_libraries(fv1-core-tests PRIVATE FV1::core)
    add_test(NAME fv1-core-tests COMMAND fv1-core-tests)

    add_executable(fv1-numeric-boundary-tests tests/test_numeric_boundaries.cpp)
    target_link_libraries(fv1-numeric-boundary-tests PRIVATE FV1::core)
    fv1_strict_warnings(fv1-numeric-boundary-tests)
    add_test(NAME fv1-numeric-boundary-tests COMMAND fv1-numeric-boundary-tests)

    add_executable(fv1-instruction-contract-tests tests/test_instruction_contract.cpp)
    target_link_libraries(fv1-instruction-contract-tests PRIVATE FV1::core)
    fv1_strict_warnings(fv1-instruction-contract-tests)
    add_test(NAME fv1-instruction-contract-tests COMMAND fv1-instruction-contract-tests)

    add_executable(fv1-conformance-tests tests/test_conformance.cpp)
    target_link_libraries(fv1-conformance-tests PRIVATE FV1::conformance)
    fv1_strict_warnings(fv1-conformance-tests)
    add_test(NAME fv1-conformance-tests COMMAND fv1-conformance-tests)

    add_executable(fv1-phase2-tests tests/test_phase2.cpp)
    target_link_libraries(fv1-phase2-tests PRIVATE FV1::runtime FV1::analysis Threads::Threads)
    add_test(NAME fv1-phase2-tests COMMAND fv1-phase2-tests)

    add_executable(fv1-audio-host-tests tests/test_audio_host_api.cpp)
    target_link_libraries(fv1-audio-host-tests PRIVATE FV1::audio)
    add_test(NAME fv1-audio-host-tests COMMAND fv1-audio-host-tests)

    add_executable(fv1-debugger-tests tests/test_debugger.cpp)
    target_link_libraries(fv1-debugger-tests PRIVATE FV1::debugger)
    add_test(NAME fv1-debugger-tests COMMAND fv1-debugger-tests)

    add_executable(fv1-recorder-tests tests/test_audio_recorder.cpp)
    target_link_libraries(fv1-recorder-tests PRIVATE FV1::audio)
    add_test(NAME fv1-recorder-tests COMMAND fv1-recorder-tests)

    add_executable(fv1-validation-tests tests/test_validation.cpp)
    target_link_libraries(fv1-validation-tests PRIVATE FV1::validation)
    add_test(NAME fv1-validation-tests COMMAND fv1-validation-tests)

    add_executable(fv1-spinasm-tests tests/test_spinasm.cpp)
    target_link_libraries(fv1-spinasm-tests PRIVATE FV1::spinasm)
    fv1_strict_warnings(fv1-spinasm-tests)
    add_test(NAME fv1-spinasm-tests COMMAND fv1-spinasm-tests)

    fv1_add_sdk_tests()

    add_executable(fv1-native-frontend-session-tests
        tests/windows/test_windows_frontend_sdk.cpp)
    target_link_libraries(fv1-native-frontend-session-tests PRIVATE
        fv1-windows-frontend-session
        FV1::sdk)
    fv1_strict_warnings(fv1-native-frontend-session-tests)
    add_test(NAME fv1-native-frontend-session-tests
        COMMAND fv1-native-frontend-session-tests)

    add_executable(fv1-native-frontend-realtime-tests
        tests/windows/test_windows_realtime_processor.cpp)
    target_link_libraries(fv1-native-frontend-realtime-tests PRIVATE
        fv1-windows-frontend-session
        FV1::sdk)
    fv1_strict_warnings(fv1-native-frontend-realtime-tests)
    add_test(NAME fv1-native-frontend-realtime-tests
        COMMAND fv1-native-frontend-realtime-tests)
    add_test(NAME phase7-windows-frontend-boundary
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_windows_frontend_boundary.py)

    add_test(NAME native-spinasm-python-equivalence
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_spinasm_equivalence.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/native-spinasm-equivalence
                phase6a)

    add_test(NAME compile-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compile_examples.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/example-programs)
    add_test(NAME spinasm-diagnostics
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_assembler_diagnostics.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py
                ${CMAKE_CURRENT_BINARY_DIR}/spinasm-diagnostics)


    add_test(NAME cli-inspect-gravity-clerk
        COMMAND fv1-cli inspect ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/03_gravity_clerk.spn)

    add_test(NAME render-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_examples.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/render-regression)
    set_tests_properties(render-steal-this-bank PROPERTIES TIMEOUT 60)

    add_test(NAME phase2-render-src
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_phase2_render_src.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/03_gravity_clerk.spn
                ${CMAKE_CURRENT_BINARY_DIR}/phase2-src-regression)
    set_tests_properties(phase2-render-src PROPERTIES TIMEOUT 30)

    add_test(NAME fv1-live-help COMMAND fv1-live --help)

    add_test(NAME phase5-validation-cli
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_validation_cli.py
                $<TARGET_FILE:fv1-cli> ${CMAKE_CURRENT_BINARY_DIR}/phase5-validation-cli)
    set_tests_properties(phase5-validation-cli PROPERTIES TIMEOUT 30)

    add_test(NAME phase5b-validation-pack-cli
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_validation_pack_cli.py
                $<TARGET_FILE:fv1-cli> ${CMAKE_CURRENT_BINARY_DIR}/phase5b-validation-pack-cli)
    set_tests_properties(phase5b-validation-pack-cli PROPERTIES TIMEOUT 30)

    add_test(NAME phase5c-demo-bank-conformance
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_conformance_examples.py
                $<TARGET_FILE:fv1-cli> ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs)
    set_tests_properties(phase5c-demo-bank-conformance PROPERTIES TIMEOUT 90)

    add_test(NAME phase6c-malformed-inputs
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_malformed_inputs.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/00_55_gallon_saint.spn
                ${CMAKE_CURRENT_BINARY_DIR}/phase6c-malformed-inputs)
    set_tests_properties(phase6c-malformed-inputs PROPERTIES TIMEOUT 45)

    add_test(NAME phase6c-differential-stress
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_phase6c_differential_stress.py
                $<TARGET_FILE:fv1-cli> ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs)
    set_tests_properties(phase6c-differential-stress PROPERTIES TIMEOUT 120)

    add_test(NAME phase6c-product-install
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/phase6c-product-install
            -DBUILD_CONFIG=$<CONFIG>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_product_install.cmake)
    set_tests_properties(phase6c-product-install PROPERTIES TIMEOUT 60)

    if(TARGET fv1-lab)
        add_test(NAME fv1-lab-smoke COMMAND fv1-lab --smoke)
        set_tests_properties(fv1-lab-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        add_test(NAME fv1-lab-hidpi-smoke COMMAND fv1-lab --smoke)
        set_tests_properties(fv1-lab-hidpi-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2")
        add_test(NAME fv1-lab-splash-background-smoke COMMAND fv1-lab --smoke-splash)
        set_tests_properties(fv1-lab-splash-background-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        add_test(NAME fv1-lab-about-smoke COMMAND fv1-lab --smoke-about)
        set_tests_properties(fv1-lab-about-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

# -----------------------------------------------------------------------------
# Optional Phase-6C release-hardening fuzzing.  These targets are intentionally opt-in because
# libFuzzer is a Clang runtime and is not required for normal developer builds.
# -----------------------------------------------------------------------------
if(FV1_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "FV1_BUILD_FUZZERS requires Clang/libFuzzer")
    endif()
    foreach(target fv1-core fv1-reference fv1-conformance)
        target_compile_options(${target} PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
        # The instrumented static libraries are also linked by normal helper
        # executables in this build tree (for example fv1-cli).  Propagate the
        # ASan/UBSan runtimes so an all-target fuzz build remains linkable.
        target_link_options(${target} INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
    add_executable(fv1-conformance-fuzzer tests/fuzz/fuzz_conformance.cpp)
    target_link_libraries(fv1-conformance-fuzzer PRIVATE FV1::conformance)
    target_compile_options(fv1-conformance-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fv1-conformance-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

    # The native SpinASM compiler is now part of the public SDK attack surface.
    # Build this fuzzer from the parser source directly so sanitizer/fuzzer
    # instrumentation remains isolated from normal application targets.
    add_executable(fv1-spinasm-fuzzer
        tests/fuzz/fuzz_spinasm.cpp
        src/spinasm/spinasm.cpp)
    target_include_directories(fv1-spinasm-fuzzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_options(fv1-spinasm-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fv1-spinasm-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

    # Public-ABI operation-sequence fuzzer. Build directly from SDK/core/compiler
    # sources so arbitrary create/load/control/process/debug sequences are fully
    # instrumented without changing normal SDK packaging.
    add_executable(fv1-sdk-fuzzer
        tests/fuzz/fuzz_sdk.cpp
        src/sdk/sdk.cpp
        src/core/engine.cpp
        src/spinasm/spinasm.cpp)
    target_include_directories(fv1-sdk-fuzzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(fv1-sdk-fuzzer PRIVATE
        FV1_SDK_BUILDING=1
        FV1_SDK_VERSION_STRING=\"${FV1_RELEASE_STRING}\"
        FV1_SDK_VERSION_MAJOR_VALUE=${PROJECT_VERSION_MAJOR}
        FV1_SDK_VERSION_MINOR_VALUE=${PROJECT_VERSION_MINOR}
        FV1_SDK_VERSION_PATCH_VALUE=${PROJECT_VERSION_PATCH})
    target_compile_options(fv1-sdk-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fv1-sdk-fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
endif()

# -----------------------------------------------------------------------------
# Installation / Phase-6 SDK package
# -----------------------------------------------------------------------------
fv1_install_sdk()

# Product installation is separate from the SDK development package. Internal
# static libraries are implementation details and are intentionally not
# installed as public development targets.
set(FV1_INSTALL_TARGETS fv1-cli fv1-live)
if(TARGET fv1-lab)
    list(APPEND FV1_INSTALL_TARGETS fv1-lab)
endif()
if(TARGET fv1-lab-win32)
    list(APPEND FV1_INSTALL_TARGETS fv1-lab-win32)
endif()
install(TARGETS ${FV1_INSTALL_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY assets/icons/ DESTINATION share/spin-fv1-emulator/icons
        PATTERN "hicolor" EXCLUDE)
install(DIRECTORY assets/splash/ DESTINATION share/spin-fv1-emulator/splash)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/roth-fv1-emulator.desktop")
    install(FILES packaging/linux/roth-fv1-emulator.desktop DESTINATION share/applications)
endif()
install(DIRECTORY assets/icons/hicolor/ DESTINATION share/icons/hicolor)
