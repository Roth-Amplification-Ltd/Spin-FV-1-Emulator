cmake_minimum_required(VERSION 3.20)
project(SpinFV1Emulator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FV1_BUILD_TESTS "Build Phase-1 emulator tests" ON)
option(FV1_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

add_library(fv1-core STATIC
    src/core/engine.cpp
)
add_library(FV1::core ALIAS fv1-core)
target_include_directories(fv1-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(FV1_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(fv1-core PRIVATE /W4)
    else()
        target_compile_options(fv1-core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(fv1-cli src/cli/main.cpp)
target_link_libraries(fv1-cli PRIVATE FV1::core)
target_compile_definitions(fv1-cli PRIVATE
    FV1_ASSEMBLER_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py"
)
if(FV1_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(fv1-cli PRIVATE /W4)
    else()
        target_compile_options(fv1-cli PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

if(FV1_BUILD_TESTS)
    enable_testing()
    add_executable(fv1-core-tests tests/test_core.cpp)
    target_link_libraries(fv1-core-tests PRIVATE FV1::core)
    add_test(NAME fv1-core-tests COMMAND fv1-core-tests)

    add_test(NAME compile-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compile_examples.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/fv1_assembler.py
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/example-programs
    )

    add_test(NAME cli-inspect-pitch-maw
        COMMAND fv1-cli inspect ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs/03_pitch_maw.spn
    )

    add_test(NAME render-steal-this-bank
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_examples.py
                $<TARGET_FILE:fv1-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/steal-this-dsp-programs
                ${CMAKE_CURRENT_BINARY_DIR}/render-regression
    )
    set_tests_properties(render-steal-this-bank PROPERTIES TIMEOUT 60)
endif()

install(TARGETS fv1-core fv1-cli
        EXPORT FV1Targets
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(PROGRAMS tools/fv1_assembler.py DESTINATION libexec/spin-fv1-emulator)
