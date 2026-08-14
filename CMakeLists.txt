cmake_minimum_required(VERSION 3.20)
project(SpinFV1Emulator VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FV1_BUILD_TESTS "Build emulator tests" ON)
option(FV1_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(FV1_ENABLE_LIVE_AUDIO "Build Linux realtime audio host when miniaudio is available" ON)
option(FV1_BUILD_GUI "Build the Phase 3 Qt desktop application" ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(SPEEXDSP QUIET IMPORTED_TARGET speexdsp)
endif()
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h)

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
target_link_libraries(fv1-runtime PUBLIC FV1::core)
if(TARGET PkgConfig::SPEEXDSP)
    target_link_libraries(fv1-runtime PRIVATE PkgConfig::SPEEXDSP)
    target_compile_definitions(fv1-runtime PRIVATE FV1_HAVE_SPEEXDSP=1)
    message(STATUS "Phase 2 SRC: SpeexDSP enabled")
else()
    message(WARNING "SpeexDSP not found: building the deterministic linear SRC fallback. Run bootstrap-dev.sh on Linux for production realtime SRC.")
endif()
fv1_strict_warnings(fv1-runtime)

add_library(fv1-analysis STATIC src/analysis/analysis.cpp)
add_library(FV1::analysis ALIAS fv1-analysis)
target_include_directories(fv1-analysis PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(fv1-analysis PUBLIC FV1::runtime Threads::Threads)
fv1_strict_warnings(fv1-analysis)

add_library(fv1-audio STATIC
    src/audio/audio_host.cpp
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
    message(WARNING "miniaudio.h not found: fv1-live will build in no-device stub mode. On Ubuntu 24.04 run bootstrap-dev.sh to install libminiaudio-dev.")
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(fv1-audio PRIVATE m)
endif()
fv1_strict_warnings(fv1-audio)

# -----------------------------------------------------------------------------
# Command-line tools
# -----------------------------------------------------------------------------
add_executable(fv1-cli src/cli/main.cpp)
target_link_libraries(fv1-cli PRIVATE FV1::core FV1::runtime)
target_compile_definitions(fv1-cli PRIVATE
    FV1_ASSEMBLER_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py")
fv1_strict_warnings(fv1-cli)

add_executable(fv1-live src/live/main.cpp)
target_link_libraries(fv1-live PRIVATE FV1::audio FV1::analysis FV1::runtime FV1::core)
target_compile_definitions(fv1-live PRIVATE
    FV1_ASSEMBLER_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py")
fv1_strict_warnings(fv1-live)

# -----------------------------------------------------------------------------
# Phase 3: Qt engineering GUI (Linux/Windows frontend)
# -----------------------------------------------------------------------------
if(FV1_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        add_executable(fv1-lab
            src/gui/main.cpp
            src/gui/main_window.cpp
            src/gui/theme_manager.cpp
            src/gui/instrument_plot.cpp)
        target_link_libraries(fv1-lab PRIVATE
            Qt6::Widgets FV1::audio FV1::analysis FV1::runtime FV1::core)
        target_include_directories(fv1-lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        fv1_strict_warnings(fv1-lab)
        message(STATUS "Phase 3 GUI: Qt ${Qt6_VERSION} enabled")
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

    add_executable(fv1-phase2-tests tests/test_phase2.cpp)
    target_link_libraries(fv1-phase2-tests PRIVATE FV1::runtime FV1::analysis Threads::Threads)
    add_test(NAME fv1-phase2-tests COMMAND fv1-phase2-tests)

    add_executable(fv1-audio-host-tests tests/test_audio_host_api.cpp)
    target_link_libraries(fv1-audio-host-tests PRIVATE FV1::audio)
    add_test(NAME fv1-audio-host-tests COMMAND fv1-audio-host-tests)

    add_test(NAME compile-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compile_examples.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/example-programs)

    add_test(NAME cli-inspect-pitch-maw
        COMMAND fv1-cli inspect ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/03_pitch_maw.spn)

    add_test(NAME render-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_examples.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/render-regression)
    set_tests_properties(render-steal-this-bank PROPERTIES TIMEOUT 60)

    add_test(NAME phase2-render-src
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_phase2_render_src.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/03_pitch_maw.spn
                ${CMAKE_CURRENT_BINARY_DIR}/phase2-src-regression)
    set_tests_properties(phase2-render-src PROPERTIES TIMEOUT 30)

    add_test(NAME fv1-live-help COMMAND fv1-live --help)

    if(TARGET fv1-lab)
        add_test(NAME fv1-lab-smoke COMMAND fv1-lab --smoke)
        set_tests_properties(fv1-lab-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
set(FV1_INSTALL_TARGETS fv1-core fv1-runtime fv1-analysis fv1-audio fv1-cli fv1-live)
if(TARGET fv1-lab)
    list(APPEND FV1_INSTALL_TARGETS fv1-lab)
endif()
install(TARGETS ${FV1_INSTALL_TARGETS}
        EXPORT FV1Targets
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(PROGRAMS tools/fv1_assembler.py DESTINATION libexec/spin-fv1-emulator)
