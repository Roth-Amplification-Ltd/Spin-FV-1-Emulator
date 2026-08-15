cmake_minimum_required(VERSION 3.20)
project(FV1SDKHost LANGUAGES C)

option(FV1SDK_HOST_STATIC "Link against a static FV1 SDK package" OFF)

find_package(FV1SDK CONFIG REQUIRED)

add_executable(fv1-sdk-host main.c)
target_link_libraries(fv1-sdk-host PRIVATE FV1SDK::sdk)

# A shared SDK is a genuine C ABI consumer: no C++ compiler is required by
# this host project.  A static SDK necessarily pulls the implementation's C++
# runtime into the final executable; enable C++ only for that linker step while
# keeping the public source and header pure C.
if(FV1SDK_HOST_STATIC)
    enable_language(CXX)
    set_property(TARGET fv1-sdk-host PROPERTY LINKER_LANGUAGE CXX)
endif()
